// EthersSubmitter — the concrete driver behind the relayer's
// submitter seam (Story 11.5). Anchors the recomputed
// LedgerMerkleRoot once, then mints per accepted claim.
// Idempotence is enforced on-chain (PotatoAnchor's timestamp
// no-op, PotatoToken's claimed[claimId] dedupe) — a re-submit
// is safe by construction, not by bookkeeping.
import { ledgerMerkleRoot, rootToBytes32 } from "./merkle.mjs";
import { rootHex } from "./claim.mjs";

const DEFAULT_AMOUNTS = { chapter_settlement: 100, achievement: 10 };

export class EthersSubmitter {
  // cfg: {endpoint, key, anchor, token, to,
  //       amounts?{chapter_settlement, achievement},
  //       signer?} — `signer` (an ethers Signer) overrides
  //       endpoint+key for in-process chains/tests.
  constructor(cfg) {
    this.cfg = cfg;
    this.amounts = { ...DEFAULT_AMOUNTS, ...(cfg.amounts ?? {}) };
  }

  async submit(claimDocs, ledgerState) {
    const { JsonRpcProvider, Wallet, Contract, keccak256,
            toUtf8Bytes } = await import("ethers");
    let provider, wallet;
    if (this.cfg.signer) {
      wallet = this.cfg.signer;
      provider = wallet.provider;
    } else {
      provider = new JsonRpcProvider(this.cfg.endpoint);
      wallet = new Wallet(this.cfg.key, provider);
    }
    const net = await provider.getNetwork();

    const anchor = new Contract(
      this.cfg.anchor,
      ["function anchor(bytes32) returns (uint64)",
       "function anchoredAt(bytes32) view returns (uint64)"],
      wallet);
    const token = new Contract(
      this.cfg.token,
      ["function mint(bytes32,address,uint256) returns (bool)",
       "function balanceOf(address) view returns (uint256)",
       "function claimed(bytes32) view returns (bool)"],
      wallet);

    // The anchor covers exactly what the relayer verified —
    // recomputed from the replayed ledger + accepted docs.
    const root = ledgerMerkleRoot(ledgerState, claimDocs);
    const root32 = rootToBytes32(root);
    const anchorTx = await anchor.anchor(root32);
    await anchorTx.wait();
    const anchoredAt = await anchor.anchoredAt(root32);

    const minted = [];
    for (const doc of claimDocs) {
      const key = keccak256(toUtf8Bytes(doc.id));
      const amount = BigInt(this.amounts[doc.kind] ?? 0n);
      // Client-side skip when already claimed — the contract
      // dedupe is the real guarantee; this just saves gas.
      if (await token.claimed(key)) {
        minted.push({ id: doc.id, amount: "0", tx: null,
                      note: "already claimed" });
        continue;
      }
      const tx = await token.mint(key, this.cfg.to, amount);
      const receipt = await tx.wait();
      minted.push({ id: doc.id, amount: amount.toString(),
                    tx: receipt.hash });
    }

    return {
      root: "0x" + rootHex(root),
      rootBytes32: root32,
      anchoredAt: anchoredAt.toString(),
      chainId: "0x" + net.chainId.toString(16),
      minted,
    };
  }

  // Story 11.6 — strike a keepsake: burn the price from the
  // player, then mint the collectible carrying the descriptor.
  // Both contracts dedupe on-chain — a re-struck artifact burns
  // nothing and mints nothing.
  // Returns {tokenId, tx} or {tokenId:0, note:"already struck"}.
  async strike(descriptor, price = 50n) {
    const { JsonRpcProvider, Wallet, Contract, keccak256,
            toUtf8Bytes } = await import("ethers");
    let wallet;
    if (this.cfg.signer) {
      wallet = this.cfg.signer;
    } else {
      wallet = new Wallet(this.cfg.key,
                          new JsonRpcProvider(this.cfg.endpoint));
    }
    const { emitCanon } = await import("./json.mjs");
    const descText = emitCanon(descriptor);
    const key = keccak256(toUtf8Bytes(
      descriptor.kind + ":" + descriptor.source_id));

    const collectible = new Contract(
      this.cfg.collectible,
      ["function strike(bytes32,address,string) returns (uint256)",
       "function struck(bytes32) view returns (bool)",
       "function descriptor(uint256) view returns (string)",
       "function ownerOf(uint256) view returns (address)",
       "function balanceOf(address) view returns (uint256)"],
      wallet);
    const token = new Contract(
      this.cfg.token,
      ["function burnFrom(address,uint256) returns (bool)",
       "function balanceOf(address) view returns (uint256)"],
      wallet);

    if (await collectible.struck(key)) {
      return { tokenId: 0n, note: "already struck" };
    }
    const burnTx = await token.burnFrom(this.cfg.to, price);
    await burnTx.wait();
    // staticCall reads the would-be return value before sending.
    const tokenId = await collectible.strike.staticCall(
      key, this.cfg.to, descText);
    const tx = await collectible.strike(key, this.cfg.to, descText);
    const receipt = await tx.wait();
    return { tokenId, tx: receipt.hash, burned: price.toString() };
  }
}
