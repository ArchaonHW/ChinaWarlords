#!/usr/bin/env node
// Story 11.6 — strike a keepsake plate (藏書票):
// burn POT + mint the collectible carrying a
// potato.collectible/1 descriptor, gated on the artifact
// existing in the player's presented campaign record.
//
// Usage:
//   node scripts/strike.mjs --kind bencao --id sanqi \
//     --ledger ledger.json --evidence bencao_state.json \
//     --config deployed.local.json --to 0x<player>
//
// Key: POTATO_RELAYER_KEY env. Config may carry endpoint/
// anchor/token/collectible/to/price.
import { readFile } from "node:fs/promises";
import { loadLedger } from "../src/ledger.mjs";
import { parseJson, decodeWire } from "../src/json.mjs";
import { buildDescriptor, DEFAULT_PRICE } from
  "../src/collectible.mjs";
import { EthersSubmitter } from "../src/submit.mjs";

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === "--evidence") {
    (args.evidence ??= []).push(process.argv[++i]);
  } else if (a.startsWith("--")) {
    args[a.slice(2)] = process.argv[++i];
  }
}

if (!args.kind || !args.id || !args.ledger) {
  console.error("usage: strike.mjs --kind K --id ID " +
    "--ledger FILE [--evidence FILE ...] [--config FILE] " +
    "[--to ADDR] [--price N]");
  process.exit(2);
}

const config = args.config
  ? JSON.parse(await readFile(args.config, "utf8"))
  : {};
const key = process.env.POTATO_RELAYER_KEY;
if (!key) {
  console.error("POTATO_RELAYER_KEY env required");
  process.exit(1);
}

const ledger = loadLedger(await readFile(args.ledger)); // throws = reject
const evidenceDocs = [];
for (const f of args.evidence ?? []) {
  evidenceDocs.push(
    parseJson(decodeWire(await readFile(f))));
}

const built = buildDescriptor(args.kind, args.id, ledger,
                              evidenceDocs);
if (!built.ok) {
  console.error(`gate refused: ${built.reason}`);
  process.exit(1);
}

const sub = new EthersSubmitter({
  endpoint: config.endpoint,
  key,
  token: config.token,
  collectible: config.collectible,
  to: args.to ?? config.to,
});
const price = BigInt(args.price ?? config.price ?? DEFAULT_PRICE);
const r = await sub.strike(built.descriptor, price);
if (r.note) {
  console.log(`already struck: ${args.kind}:${args.id}`);
} else {
  console.log(`struck token #${r.tokenId} ` +
    `(burned ${r.burned}) tx ${r.tx}`);
}
