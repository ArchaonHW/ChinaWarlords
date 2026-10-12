// Story 11.6 collectible suite — strike E2E on the in-process
// ganache chain: gate → burn+mint → descriptor on-chain →
// dedupe → non-owner revert.
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import ganache from "ganache";
import { loadLedger } from "../src/ledger.mjs";
import { parseJson, decodeWire, emitCanon } from "../src/json.mjs";
import { buildDescriptor } from "../src/collectible.mjs";
import { deploy } from "../src/chain.mjs";
import { EthersSubmitter } from "../src/submit.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const FIXTURES = join(here, "fixtures");
const LEDGER = join(FIXTURES, "ledger.json");

let gp, bp, deployer, player, contracts, ledger;

before(async () => {
  gp = ganache.provider({ wallet: { deterministic: true } });
  const { BrowserProvider } = await import("ethers");
  bp = new BrowserProvider(gp);
  deployer = await bp.getSigner(0);
  player = await bp.getSigner(1);
  contracts = await deploy(deployer);
  ledger = loadLedger(await readFile(LEDGER));
  // Fund the player wallet through the real mint path.
  const sub = new EthersSubmitter({
    signer: deployer, token: contracts.token,
    collectible: contracts.collectible,
    to: await player.getAddress(),
  });
  // Direct mint call — the submit() path needs claim docs; a raw
  // funding mint is the same onlyOwner surface.
  const { Contract, keccak256, toUtf8Bytes } =
    await import("ethers");
  const token = new Contract(contracts.token,
    ["function mint(bytes32,address,uint256) returns (bool)"],
    deployer);
  await (await token.mint(
    keccak256(toUtf8Bytes("funding")),
    await player.getAddress(), 200n)).wait();
});

after(async () => { await gp?.disconnect?.(); });

const codexState = (ids) => ({
  schema: "potato.bencao_state/1",
  unlocked: ids,
  pending: [],
});

test("bencao strike: gate passes, burns price, NFT minted", async () => {
  const playerAddr = await player.getAddress();
  const { Contract, keccak256, toUtf8Bytes } =
    await import("ethers");
  const token = new Contract(contracts.token,
    ["function balanceOf(address) view returns (uint256)"], bp);
  const nft = new Contract(contracts.collectible,
    ["function balanceOf(address) view returns (uint256)",
     "function ownerOf(uint256) view returns (address)",
     "function descriptor(uint256) view returns (string)",
     "function struck(bytes32) view returns (bool)"], bp);

  const built = buildDescriptor("bencao", "sanqi", ledger,
                                [codexState(["sanqi", "fuzi"])]);
  assert.ok(built.ok, built.reason);
  assert.equal(built.descriptor.schema, "potato.collectible/1");
  assert.equal(built.descriptor.kind, "bencao");
  assert.equal(built.descriptor.source_id, "sanqi");
  // Seal stamped from the replayed ledger.
  assert.equal(
    BigInt.asUintN(64, built.descriptor.campaign.tip),
    ledger.tip);

  const sub = new EthersSubmitter({
    signer: deployer, token: contracts.token,
    collectible: contracts.collectible, to: playerAddr,
  });
  const r = await sub.strike(built.descriptor, 50n);
  assert.ok(r.tokenId > 0n);
  assert.equal(await token.balanceOf(playerAddr), 150n,
               "price burned");
  assert.equal(await nft.balanceOf(playerAddr), 1n);
  assert.equal(await nft.ownerOf(r.tokenId), playerAddr);
  const onchain = await nft.descriptor(r.tokenId);
  assert.equal(onchain, emitCanon(built.descriptor),
               "descriptor stored byte-exact");
  assert.ok(await nft.struck(
    keccak256(toUtf8Bytes("bencao:sanqi"))));
});

test("gate refuses an artifact absent from the record", async () => {
  const built = buildDescriptor("bencao", "ghost_herb", ledger,
                                [codexState(["sanqi"])]);
  assert.equal(built.ok, false);
  assert.match(built.reason, /not in presented record/);
  const badKind = buildDescriptor("haiku", "x", ledger, []);
  assert.equal(badKind.ok, false);
});

test("placeholder kinds gate on verbatim evidence text", async () => {
  const doc = { schema: "potato.frontispiece/0",
                entries: ["ch7_frontis"] };
  assert.ok(buildDescriptor("frontispiece", "ch7_frontis",
                            ledger, [doc]).ok);
  assert.equal(buildDescriptor("dossier", "missing", ledger,
                               [doc]).ok, false);
});

test("re-strike is a no-op — burns nothing", async () => {
  const playerAddr = await player.getAddress();
  const { Contract } = await import("ethers");
  const token = new Contract(contracts.token,
    ["function balanceOf(address) view returns (uint256)"], bp);
  const sub = new EthersSubmitter({
    signer: deployer, token: contracts.token,
    collectible: contracts.collectible, to: playerAddr,
  });
  const built = buildDescriptor("bencao", "sanqi", ledger,
                                [codexState(["sanqi"])]);
  const r = await sub.strike(built.descriptor, 50n);
  assert.equal(r.tokenId, 0n);
  assert.equal(r.note, "already struck");
  assert.equal(await token.balanceOf(playerAddr), 150n,
               "no second burn");
});

test("non-owner strike and burn revert", async () => {
  const { Contract } = await import("ethers");
  const rogue = await bp.getSigner(2);
  const nft = new Contract(contracts.collectible,
    ["function strike(bytes32,address,string) returns (uint256)"],
    rogue);
  const token = new Contract(contracts.token,
    ["function burnFrom(address,uint256) returns (bool)"], rogue);
  await assert.rejects(
    nft.strike("0x" + "cd".repeat(32), await rogue.getAddress(),
               "{}"),
    /NotOwner|revert/);
  await assert.rejects(
    token.burnFrom(await player.getAddress(), 1n),
    /NotOwner|revert/);
});
