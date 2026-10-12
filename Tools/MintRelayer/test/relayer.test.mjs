// Story 11.4 relayer suite — fixtures are C++-produced
// (potato_export_chain) so every "accepted" pin is a real
// cross-implementation verification, not the JS checking itself.
import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, readFile, writeFile, cp, rm, readdir } from
  "node:fs/promises";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { runRelayer } from "../src/run.mjs";
import { parseJson, decodeWire, emitCanon } from "../src/json.mjs";
import { loadLedger } from "../src/ledger.mjs";
import { validateClaim, claimId } from "../src/claim.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const FIXTURES = join(here, "fixtures");
const LEDGER = join(FIXTURES, "ledger.json");
const CLAIMS = join(FIXTURES, "claims");

const tmp = () => mkdtemp(join(tmpdir(), "relayer-"));

async function staged() {
  const dir = await tmp();
  await cp(CLAIMS, join(dir, "claims"), { recursive: true });
  return dir;
}

const ledgerState = async () =>
  loadLedger(await readFile(LEDGER));

test("accepts the C++-exported outbox verbatim", async () => {
  const dir = await staged();
  const reportPath = join(dir, "report.json");
  const { report } = await runRelayer({
    outbox: join(dir, "claims"),
    ledgerPath: LEDGER,
    reportPath,
  });
  assert.equal(report.claims.length, 2);
  assert.equal(report.planned.length, 2);
  for (const c of report.claims) {
    assert.equal(c.status, "accepted", `${c.file}: ${c.reason}`);
  }
  assert.deepEqual(report.planned, [...report.planned].sort());
  // Report on disk is itself a schema-tagged doc.
  const doc = parseJson(decodeWire(await readFile(reportPath)));
  assert.equal(doc.schema, "potato.relayer_report/1");
  const ledger = await ledgerState();
  assert.equal(BigInt.asUintN(64, doc.ledger.tip), ledger.tip);
});

test("tamper battery: per-file isolation, batch never aborts", async () => {
  const dir = await staged();
  const claims = join(dir, "claims");
  const ledger = await ledgerState();
  const tip = BigInt.asIntN(64, ledger.tip);
  const count = BigInt(ledger.entries.length);

  const drop = async (name, body) =>
    writeFile(join(claims, name), body);

  await drop("01-badschema.json",
    JSON.stringify({ schema: "potato.mintclaim/2" }));
  await drop("02-malformed.json", "{not json");
  await drop("03-nonobject.json", "[1,2]");
  // Valid claim whose anchor the ledger never posted.
  const orphan = {
    schema: "potato.mintclaim/1",
    id: "settlement-0000000000000001",
    kind: "chapter_settlement",
    chapter: 7n,
    record_root: "0000000000000001",
    resolution: "defeat",
    ledger: { count: count, tip: tip },
  };
  await drop("04-noanchor.json", emitCanon(orphan));
  // Valid shape but the id lies about its content.
  const liar = {
    schema: "potato.mintclaim/1",
    id: "settlement-ffffffffffffffff",
    kind: "chapter_settlement",
    chapter: 7n,
    record_root: "0000000000000002",
    resolution: "defeat",
    ledger: { count: count, tip: tip },
  };
  await drop("05-idlie.json", emitCanon(liar));
  // Seal mismatch: count off by one.
  const stale = {
    schema: "potato.mintclaim/1",
    id: "settlement-8553593427cadb3a",
    kind: "chapter_settlement",
    chapter: 7n,
    record_root: "8553593427cadb3a",
    resolution: "battle_victory",
    ledger: { count: count + 1n, tip: tip },
  };
  await drop("06-badseal.json", emitCanon(stale));
  // Achievement with a forbidden charset.
  const badkey = {
    schema: "potato.mintclaim/1",
    id: "achievement-Bad_Key-0007",
    kind: "achievement",
    chapter: 7n,
    achievement: "Bad_Key",
    ledger: { count: count, tip: tip },
  };
  await drop("07-badkey.json", emitCanon(badkey));
  // Settlement claim carrying an achievement field.
  const mixed = {
    schema: "potato.mintclaim/1",
    id: "settlement-8553593427cadb3a",
    kind: "chapter_settlement",
    chapter: 7n,
    record_root: "8553593427cadb3a",
    resolution: "battle_victory",
    achievement: "oops",
    ledger: { count: count, tip: tip },
  };
  await drop("08-mixed.json", emitCanon(mixed));
  // Case-folded extension still scans.
  await cp(join(claims, "achievement-zero_combat-0007.json"),
           join(claims, "09-upper.JSON"));

  const { report } = await runRelayer({
    outbox: claims,
    ledgerPath: LEDGER,
    reportPath: join(dir, "report.json"),
  });
  const byFile = Object.fromEntries(
    report.claims.map((c) => [c.file, c]));

  assert.match(byFile["01-badschema.json"].reason, /schema/);
  assert.match(byFile["02-malformed.json"].reason, /parse/);
  assert.match(byFile["03-nonobject.json"].reason, /not an object/);
  assert.match(byFile["04-noanchor.json"].reason, /anchor/);
  assert.match(byFile["05-idlie.json"].reason, /id/);
  assert.match(byFile["06-badseal.json"].reason, /seal/);
  assert.match(byFile["07-badkey.json"].reason, /achievement/);
  assert.match(byFile["08-mixed.json"].reason, /achievement/);
  assert.equal(byFile["09-upper.JSON"].status, "accepted");
  assert.equal(
    byFile["settlement-8553593427cadb3a.json"].status, "accepted");
  assert.equal(
    byFile["achievement-zero_combat-0007.json"].status, "accepted");
  assert.equal(report.planned.length, 2); // dup id planned once
  await rm(dir, { recursive: true });
});

test("tampered ledger file rejects the whole batch", async () => {
  const dir = await staged();
  const doc = parseJson(decodeWire(await readFile(LEDGER)));
  doc.entries[0].memo = "rewritten history"; // breaks entry 0's hash
  const bad = join(dir, "ledger.json");
  await writeFile(bad, emitCanon(doc));
  await assert.rejects(
    runRelayer({ outbox: join(dir, "claims"), ledgerPath: bad,
                 reportPath: join(dir, "r.json") }),
    /entry hash mismatch/);
  await rm(dir, { recursive: true });
});

test("truncated ledger fails seal check", async () => {
  const dir = await staged();
  const doc = parseJson(decodeWire(await readFile(LEDGER)));
  doc.entries.pop(); // seal commits to count AND tip
  // Re-link nothing — the seal itself must catch it.
  const bad = join(dir, "ledger.json");
  await writeFile(bad, emitCanon(doc));
  await assert.rejects(
    runRelayer({ outbox: join(dir, "claims"), ledgerPath: bad,
                 reportPath: join(dir, "r.json") }),
    /seal mismatch/);
  await rm(dir, { recursive: true });
});

test("report path inside the outbox is refused", async () => {
  const dir = await staged();
  await assert.rejects(
    runRelayer({
      outbox: join(dir, "claims"),
      ledgerPath: LEDGER,
      reportPath: join(dir, "claims", "report.json"),
    }),
    /inside the outbox/);
  await rm(dir, { recursive: true });
});

test("empty outbox yields an empty plan, still writes a report",
     async () => {
  const dir = await staged();
  const empty = join(dir, "empty");
  const { mkdir } = await import("node:fs/promises");
  await mkdir(empty);
  const reportPath = join(dir, "report.json");
  const { report } = await runRelayer({
    outbox: empty, ledgerPath: LEDGER, reportPath,
  });
  assert.equal(report.claims.length, 0);
  assert.equal(report.planned.length, 0);
  const names = await readdir(dir);
  assert.ok(names.includes("report.json"));
  await rm(dir, { recursive: true });
});

test("claim validator: wire parity pins", () => {
  // Round-trip a real fixture claim through the validator.
  const good = parseJson(
    `{"schema":"potato.mintclaim/1","id":"settlement-8553593427cadb3a","kind":"chapter_settlement","chapter":7,"record_root":"8553593427cadb3a","resolution":"battle_victory","ledger":{"count":3,"tip":-2604144523890565395}}`);
  const v = validateClaim(good);
  assert.ok(v.ok);
  assert.equal(v.claim.id, "settlement-8553593427cadb3a");
  assert.equal(claimId(v.claim), v.claim.id);
  // Real-typed numbers are not ints — schema rejects them.
  const real = { ...good, chapter: 7.0 };
  assert.equal(validateClaim(real).ok, false);
});

test("claim validator: settlement kind (12.5 encounter claims)", () => {
  // The C++ MintClaim::ToJson shape for a world-encounter verdict:
  // no chapter, encounter + node carry the ground it settled on.
  const enc = parseJson(
    `{"schema":"potato.mintclaim/1","id":"settlement-8553593427cadb3a","kind":"settlement","encounter":"longmen_ambush","node":"longmen","record_root":"8553593427cadb3a","resolution":"battle_victory","ledger":{"count":3,"tip":-2604144523890565395}}`);
  const v = validateClaim(enc);
  assert.ok(v.ok, v.reason);
  assert.equal(v.claim.kind, "settlement");
  assert.equal(v.claim.encounter, "longmen_ambush");
  assert.equal(v.claim.node, "longmen");
  // Same id shape as chapter_settlement: the record root binds.
  assert.equal(claimId(v.claim), v.claim.id);
  assert.equal(v.claim.id, "settlement-8553593427cadb3a");
  // chapter is forbidden on settlement claims — not just absent.
  assert.equal(validateClaim({ ...enc, chapter: 7n }).ok, false);
  // encounter/node are required, non-empty, bounded.
  const noNode = { ...enc };
  delete noNode.node;
  assert.equal(validateClaim(noNode).ok, false);
  assert.equal(
    validateClaim({ ...enc, encounter: "" }).ok, false);
  // settlement claims still can't carry achievement fields.
  assert.equal(
    validateClaim({ ...enc, achievement: "oops" }).ok, false);
  // ...and the other flavors can't borrow encounter fields.
  const chapWithEnc = parseJson(
    `{"schema":"potato.mintclaim/1","id":"settlement-8553593427cadb3a","kind":"chapter_settlement","chapter":7,"record_root":"8553593427cadb3a","resolution":"battle_victory","encounter":"x","ledger":{"count":3,"tip":-2604144523890565395}}`);
  assert.equal(validateClaim(chapWithEnc).ok, false);
  const achWithNode = parseJson(
    `{"schema":"potato.mintclaim/1","id":"achievement-zero_combat-0007","kind":"achievement","chapter":7,"achievement":"zero_combat","node":"longmen","ledger":{"count":3,"tip":-2604144523890565395}}`);
  assert.equal(validateClaim(achWithNode).ok, false);
});
