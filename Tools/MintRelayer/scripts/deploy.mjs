#!/usr/bin/env node
// Deploy PotatoAnchor + PotatoToken to a local chain and write
// deployed.local.json (gitignored) for relayer --config use.
//
// Usage:
//   node scripts/deploy.mjs --endpoint http://127.0.0.1:8545
// Key: POTATO_RELAYER_KEY env (ganache's deterministic first
// account works for local demos).
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { writeFile } from "node:fs/promises";
import { deploy } from "../src/chain.mjs";

const here = dirname(fileURLToPath(import.meta.url));

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  if (process.argv[i] === "--endpoint") args.endpoint = process.argv[++i];
}
const endpoint = args.endpoint ?? "http://127.0.0.1:8545";
const key = process.env.POTATO_RELAYER_KEY;
if (!key) {
  console.error("POTATO_RELAYER_KEY env required " +
    "(ganache account[0] private key for local demos)");
  process.exit(1);
}

const { JsonRpcProvider, Wallet } = await import("ethers");
const provider = new JsonRpcProvider(endpoint);
const wallet = new Wallet(key, provider);
const d = await deploy(wallet);
const net = await provider.getNetwork();

const out = {
  endpoint,
  chainId: "0x" + net.chainId.toString(16),
  anchor: d.anchor,
  token: d.token,
  deployer: wallet.address,
};
const path = join(here, "..", "deployed.local.json");
await writeFile(path, JSON.stringify(out, null, 2) + "\n");
console.log(`anchor ${d.anchor}`);
console.log(`token  ${d.token}`);
console.log(`wrote  ${path}`);
