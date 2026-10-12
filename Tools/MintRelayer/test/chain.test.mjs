// Story 11.5 chain suite — full E2E on an in-process ganache EVM:
// deploy → validate → submit → balances/anchor assertions →
// idempotent re-submit → unauthorized mint reverts.
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { readFile, mkdtemp, cp, rm, readdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import ganache from "ganache";
import { runRelayer } from "../src/run.mjs";
import { loadLedger } from "../src/ledger.mjs";
import { parseJson, decodeWire } from "../src/json.mjs";
import { ledgerMerkleRoot, rootToBytes32 } from "../src/merkle.mjs";
import { deploy } from "../src/chain.mjs";
import { EthersSubmitter } from "../src/submit.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const FIXTURES = join(here, "fixtures");
const LEDGER = join(FIXTURES, "ledger.json");
const CLAIMS = join(FIXTURES, "claims");
const MERKLE = join(FIXTURES, "merkle.json");

let gp, bp, deployer, player, contracts;

before(async () => {
  gp = ganache.provider({ wallet: { deterministic: true } });
  const { BrowserProvider, Contract } = await import("ethers");
  bp = new BrowserProvider(gp);
  deployer = await bp.getSigner(0);
  player = await bp.getSigner(1);
  contracts = await deploy(deployer);
});

after(async () => { await gp?.disconnect?.(); });

const acceptedDocs = async () => {
  const docs = [];
  for (const n of (await readdir(CLAIMS)).sort()) {
    docs.push(parseJson(
      decodeWire(await readFile(join(CLAIMS, n)))));
  }
  return docs;
};

test("JS merkle root == C++ exported root (byte-exact parity)",
     async () => {
  const ledger = loadLedger(await readFile(LEDGER));
  const docs = await acceptedDocs();
  const root = ledgerMerkleRoot(ledger, docs);
  const golden = parseJson(decodeWire(await readFile(MERKLE)));
  assert.equal(golden.schema, "potato.merkle/1");
  assert.equal(root.toString(16).padStart(16, "0"), golden.root);
});

test("submit anchors root and mints; resubmit is idempotent",
     async () => {
  const dir = await mkdtemp(join(tmpdir(), "chain-"));
  await cp(CLAIMS, join(dir, "claims"), { recursive: true });
  const { Contract, keccak256, toUtf8Bytes, getAddress } =
    await import("ethers");
  const token = new Contract(contracts.token,
    ["function balanceOf(address) view returns (uint256)",
     "function claimed(bytes32) view returns (bool)",
     "function totalSupply() view returns (uint256)"], bp);
  const anchor = new Contract(contracts.anchor,
    ["function anchoredAt(bytes32) view returns (uint64)"], bp);
  const playerAddr = await player.getAddress();

  const submitter = new EthersSubmitter({
    signer: deployer,
    anchor: contracts.anchor,
    token: contracts.token,
    to: playerAddr,
    amounts: { chapter_settlement: 100, achievement: 10 },
  });
  const { report } = await runRelayer({
    outbox: join(dir, "claims"),
    ledgerPath: LEDGER,
    reportPath: join(dir, "r1.json"),
    submitter,
  });

  const s = report.submission;
  assert.ok(s, "submission recorded");
  const ledger = loadLedger(await readFile(LEDGER));
  const expectRoot = rootToBytes32(
    ledgerMerkleRoot(ledger, await acceptedDocs()));
  assert.equal(s.rootBytes32, expectRoot);
  assert.notEqual(await anchor.anchoredAt(expectRoot), 0n,
                  "root anchored on-chain");
  assert.equal(await token.balanceOf(playerAddr), 110n,
               "100 settlement + 10 achievement minted");
  assert.equal(await token.totalSupply(), 110n);

  // Second run: same outbox → anchor no-ops, mints dedupe.
  const { report: r2 } = await runRelayer({
    outbox: join(dir, "claims"),
    ledgerPath: LEDGER,
    reportPath: join(dir, "r2.json"),
    submitter,
  });
  assert.equal(await token.balanceOf(playerAddr), 110n,
               "resubmit mints nothing");
  assert.equal(await token.totalSupply(), 110n);
  assert.ok(r2.submission.minted.every(
    (m) => m.amount === "0" && m.note === "already claimed"),
    "dedupe reported per claim");
  assert.equal(await anchor.anchoredAt(expectRoot),
               BigInt(s.anchoredAt), "anchor timestamp stable");
  await rm(dir, { recursive: true });
});

test("non-owner mint reverts", async () => {
  const { Contract } = await import("ethers");
  const rogue = await tokenAs(await bp.getSigner(2));
  const key = "0x" + "ab".repeat(32);
  await assert.rejects(
    rogue.mint(key, await player.getAddress(), 1n),
    /NotOwner|revert/);
  async function tokenAs(signer) {
    return new Contract(contracts.token,
      ["function mint(bytes32,address,uint256) returns (bool)"],
      signer);
  }
});
