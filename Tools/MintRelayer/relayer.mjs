#!/usr/bin/env node
// Story 11.4 — external relayer (D-ARCH-9 boundary).
//
// Scans a mint-claim outbox, re-validates every claim against the
// campaign ledger (schema, id recompute, round-trip, seal
// consistency, record_root anchor), and writes a
// potato.relayer_report/1. Invalid claims are skipped with a
// written reason — the batch never aborts on one bad file.
//
// Usage:
//   node relayer.mjs --outbox DIR --ledger FILE [options]
//
// Options:
//   --report FILE     report path (default <outbox>.report.json)
//   --endpoint URL    JSON-RPC endpoint; probes eth_chainId
//   --submit          anchor the batch merkle root + mint per
//                     accepted claim (needs endpoint+key+anchor+
//                     token+to — CLI flags or --config fields)
//   --anchor ADDR     PotatoAnchor contract address
//   --token ADDR      PotatoToken contract address
//   --to ADDR         player wallet receiving mints
//   --config FILE     local JSON config {endpoint, keyFile, key,
//                     anchor, token, to, amounts}; *.local.json
//                     is gitignored — never commit
//
// Dev-wallet key resolution (when --endpoint or --submit is set):
//   1. env POTATO_RELAYER_KEY
//   2. config "keyFile" (read, trimmed) or "key" field
// The key is never printed, logged, or written to the report.
import { readFile } from "node:fs/promises";
import { runRelayer } from "./src/run.mjs";

const usage = `usage: node relayer.mjs --outbox DIR --ledger FILE
  [--report FILE] [--endpoint URL] [--config FILE]
  [--submit --anchor ADDR --token ADDR --to ADDR]`;

function parseArgs(argv) {
  const o = {};
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith("--")) {
      throw new Error(`unexpected argument ${a}`);
    }
    const k = a.slice(2);
    if (k === "submit") { o.submit = true; continue; }
    if (i + 1 >= argv.length) throw new Error(`--${k} needs a value`);
    o[k] = argv[++i];
  }
  return o;
}

async function resolveKey(config, env) {
  if (env.POTATO_RELAYER_KEY) return { source: "env", key: env.POTATO_RELAYER_KEY };
  if (config?.keyFile) {
    const key = (await readFile(config.keyFile, "utf8")).trim();
    return { source: "keyFile", key };
  }
  if (config?.key) return { source: "config", key: config.key };
  return null;
}

async function main() {
  let args;
  try {
    args = parseArgs(process.argv.slice(2));
  } catch (err) {
    console.error(err.message);
    console.error(usage);
    return 2;
  }
  if (!args.outbox || !args.ledger) {
    console.error(usage);
    return 2;
  }

  let config = null;
  if (args.config) {
    try {
      config = JSON.parse(await readFile(args.config, "utf8"));
    } catch (err) {
      console.error(`config: ${err.message}`);
      return 1;
    }
  }
  const endpoint = args.endpoint ?? config?.endpoint ?? null;
  const wantSubmit = args.submit === true;
  const needsKey = wantSubmit || endpoint !== null;

  let key = null;
  if (needsKey) {
    const cred = await resolveKey(config, process.env).catch(
      (err) => {
        console.error(`key: ${err.message}`);
        return null;
      });
    if (!cred) {
      console.error(
        "key: no dev-wallet key found " +
        "(POTATO_RELAYER_KEY env or config keyFile/key)");
      return 1;
    }
    key = cred.key; // held only in scope — never echoed
  }

  let submitter = null;
  if (wantSubmit) {
    const anchor = args.anchor ?? config?.anchor;
    const token = args.token ?? config?.token;
    const to = args.to ?? config?.to;
    if (!endpoint || !anchor || !token || !to) {
      console.error(
        "submit: needs endpoint + anchor + token + to " +
        "(flags or config fields)");
      return 1;
    }
    const { EthersSubmitter } = await import("./src/submit.mjs");
    submitter = new EthersSubmitter({
      endpoint, key, anchor, token, to,
      amounts: config?.amounts,
    });
  }

  try {
    const { report, reportPath } = await runRelayer({
      outbox: args.outbox,
      ledgerPath: args.ledger,
      reportPath: args.report,
      endpoint,
      env: process.env,
      submitter,
    });
    const ok = report.claims.filter((c) => c.status === "accepted");
    const bad = report.claims.filter((c) => c.status === "skipped");
    console.log(`ledger: ${report.claims.length} claim file(s), ` +
      `${ok.length} accepted, ${bad.length} skipped`);
    for (const c of bad) {
      console.log(`  skip ${c.file}: ${c.reason}`);
    }
    if (report.chainId) {
      console.log(`endpoint reachable: chainId ${report.chainId}`);
    }
    if (report.submission) {
      const s = report.submission;
      console.log(`anchored root ${s.root} at ${s.anchoredAt}`);
      for (const m of s.minted) {
        console.log(`  mint ${m.id} = ${m.amount}` +
                    (m.tx ? ` (${m.tx})` : ` [${m.note}]`));
      }
    }
    console.log(`report: ${reportPath}`);
    return 0;
  } catch (err) {
    console.error(`relayer: ${err.message}`);
    return 1;
  }
}

process.exitCode = await main();
