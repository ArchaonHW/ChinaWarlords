// LedgerMerkleRoot — JS port of Campaign/Chain/LedgerMerkle.cpp.
// Domain-separated FNV fold: entry leaves in seq order, claim
// leaves sorted by (id, canonical emit), 0x00‖L‖R nodes, odd
// leaf promotes. The root attests the exact ledger+claim set the
// relayer verified — it is recomputed, never read from a file.
import { emitBytes } from "./json.mjs";
import { fnv1aBytes, fnv1aU64 } from "./fnv.mjs";

const M64 = (v) => BigInt.asUintN(64, v);

const entryLeaf = (hash) =>
  fnv1aU64(M64(hash), fnv1aBytes(Uint8Array.of(0x01)));
const claimLeaf = (emit) =>
  fnv1aBytes(emit, fnv1aBytes(Uint8Array.of(0x02)));
const node = (l, r) =>
  fnv1aU64(M64(r), fnv1aU64(M64(l), fnv1aBytes(Uint8Array.of(0x00))));

// ledgerState: {entries:[…]} from verifyLedger.
// claimDocs: the RAW parsed claim docs that passed validation —
// their canonical emit is what the C++ side hashes.
export function ledgerMerkleRoot(ledgerState, claimDocs) {
  const leaves = [];
  for (const e of ledgerState.entries) leaves.push(entryLeaf(e.hash));

  const keyed = claimDocs
    .map((d) => [claimIdFromDoc(d), emitBytes(d)])
    .sort((a, b) => {
      // Sort by (id, emit) — id is ASCII so byte compare = locale-safe
      if (a[0] !== b[0]) return a[0] < b[0] ? -1 : 1;
      const ea = a[1], eb = b[1];
      const n = Math.min(ea.length, eb.length);
      for (let i = 0; i < n; i++) if (ea[i] !== eb[i]) return ea[i] - eb[i];
      return ea.length - eb.length;
    });
  // (id, emit) dedupe — set semantics like the C++ side.
  let prev = null;
  for (const [id, emit] of keyed) {
    const sig = id + "|" + emit.join(",");
    if (sig === prev) continue;
    prev = sig;
    leaves.push(claimLeaf(emit));
  }

  if (leaves.length === 0) return 0n;
  while (leaves.length > 1) {
    const next = [];
    for (let i = 0; i < leaves.length; i += 2) {
      next.push(i + 1 < leaves.length ? node(leaves[i], leaves[i + 1])
                                      : leaves[i]);
    }
    leaves.splice(0, leaves.length, ...next);
  }
  return leaves[0];
}

// doc.id was already verified == ClaimId(content) by validateClaim.
const claimIdFromDoc = (d) => d.id;

// u64 root → bytes32 (low 8 bytes, big-endian) for the anchor call.
export function rootToBytes32(root) {
  return "0x" + M64(root).toString(16).padStart(16, "0")
    .padStart(64, "0");
}
