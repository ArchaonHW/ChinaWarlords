// Batch orchestration: scan the outbox (sorted .json, case-folded
// extension, MAX_CLAIMS bound — MintOutbox::Scan parity), isolate
// each file's failure, and write a potato.relayer_report/1.
//
// The report is a SIBLING of the outbox by default
// (`<outbox>.report.json`) — never inside it, or the relayer would
// ingest its own output next run.
import { readFile, readdir, writeFile, rename, stat } from
  "node:fs/promises";
import { resolve, join, extname, basename, dirname } from
  "node:path";
import { loadLedger } from "./ledger.mjs";
import { processClaimBytes } from "./claim.mjs";
import { emitCanon } from "./json.mjs";

export const REPORT_SCHEMA = "potato.relayer_report/1";
export const MAX_CLAIMS = 4096; // MintOutbox::MAX_CLAIMS

const M64 = (v) => BigInt.asUintN(64, v);

// int64 round-trip: u64 BigInt → the signed literal the wire uses.
const i64 = (u) => BigInt.asIntN(64, M64(u));

async function listClaimFiles(dir) {
  const names = await readdir(dir);
  const files = [];
  for (const n of names) {
    if (extname(n).toLowerCase() !== ".json") continue;
    const st = await stat(join(dir, n));
    if (st.isFile()) files.push(n);
  }
  files.sort();
  if (files.length > MAX_CLAIMS) {
    throw new Error("overflow: outbox claims exceed MAX_CLAIMS");
  }
  return files;
}

async function writeAtomic(dst, text) {
  const tmp = dst + ".tmp";
  await writeFile(tmp, text, "utf8");
  await rename(tmp, dst);
}

// opts: {outbox, ledgerPath, reportPath, endpoint, env, config}
// Returns the report object; throws on ledger-load or outbox-dir
// failure (batch-level conditions, not per-claim).
export async function runRelayer(opts) {
  const ledgerBytes = await readFile(opts.ledgerPath);
  const ledger = loadLedger(ledgerBytes); // throws → wholesale reject

  const reportPath = opts.reportPath ?? (opts.outbox + ".report.json");
  const rel = resolve(opts.outbox);
  if (resolve(dirname(reportPath)) === rel ||
      resolve(reportPath).startsWith(rel + "\\") ||
      resolve(reportPath).startsWith(rel + "/")) {
    throw new Error(
      "config: report path must not live inside the outbox");
  }

  const files = await listClaimFiles(opts.outbox);
  const results = [];
  const acceptedDocs = []; // canonical docs for the merkle fold
  for (const name of files) {
    const file = join(opts.outbox, name);
    try {
      const bytes = await readFile(file);
      const r = processClaimBytes(bytes, ledger);
      if (r.ok) {
        results.push({ file: name, status: "accepted",
                       id: r.claim.id });
        acceptedDocs.push(r.doc);
      } else {
        results.push({ file: name, status: "skipped",
                       reason: `${r.error}: ${r.reason}` });
      }
    } catch (err) {
      // A read/stat failure is per-file — never aborts the batch.
      results.push({ file: name, status: "skipped",
                     reason: `io: ${err.message}` });
    }
  }
  // Plan once per claim id — two files carrying the same id are
  // the same claim (the game's own dedupe is filename-based).
  const seen = new Set();
  const uniqueDocs = [];
  const accepted = [];
  for (const d of acceptedDocs) {
    if (seen.has(d.id)) continue;
    seen.add(d.id);
    uniqueDocs.push(d);
    accepted.push(d.id);
  }
  accepted.sort();

  // Submitter seam: dry-run by default. With --submit an
  // EthersSubmitter anchors the recomputed merkle root and mints
  // per claim; with --endpoint alone we only probe connectivity
  // (eth_chainId).
  let chainId = null;
  let submission = null;
  if (opts.submitter) {
    submission = await opts.submitter.submit(uniqueDocs, ledger);
    chainId = submission.chainId ?? null;
  } else if (opts.endpoint) {
    const res = await fetch(opts.endpoint, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        jsonrpc: "2.0", id: 1,
        method: "eth_chainId", params: [],
      }),
    });
    const body = await res.json();
    chainId = body.result ?? null;
  }

  const report = {
    schema: REPORT_SCHEMA,
    ledger: {
      count: i64(BigInt(ledger.entries.length)),
      tip: i64(ledger.tip),
      seal: i64(ledger.seal),
    },
    endpoint: opts.endpoint ?? null,
    chainId,
    claims: results,
    planned: accepted,
    submission,
  };
  await writeAtomic(reportPath, emitCanon(report));
  return { report, reportPath };
}
