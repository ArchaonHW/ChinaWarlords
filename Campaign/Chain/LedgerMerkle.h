#pragma once

#include "Campaign/Chain/MintClaim.h"
#include "Campaign/Ledger/Ledger.h"
#include "Gameplay/Result.h"

#include <cstdint>
#include <span>

namespace Potato::Campaign {

// Story 11.2 — the anchor fold: one uint64 committing to the whole
// ledger hash chain plus a claim set. What the external relayer
// (11.4) anchors on-chain (11.5) — the game emits the number, the
// chain boundary stays versioned-JSON files (D-ARCH-9).
//
// Canonical leaf order: all ledger entries in seq order, then
// claims sorted by (ClaimId, canonical emit). The claim set is a
// SET — exact duplicates dedupe before the fold, and input order
// is irrelevant.
//
// FNV-1a under a per-level domain byte — the same hash family as
// the ledger chain and the battle record's integrity root:
//   entry leaf = Fnv1a(0x01 ‖ entry.hash LE8)
//   claim leaf = Fnv1a(0x02 ‖ claim canonical emit bytes)
//   node       = Fnv1a(0x00 ‖ left LE8 ‖ right LE8)
// Pair-wise fold left to right; an odd leaf promotes unpaired
// (RFC-6962 style — deterministic, no duplication ambiguity).
// `suspect` flags are NOT covered: they live outside hashed
// content (Ledger.h), and flipping judgment must not move the
// anchor. Empty ledger + empty claim set yields 0 — the "nothing
// anchored" sentinel, matching the recordRoot==0 convention.
//
// Fails `field` when a claim's ToJson fails — an oversized
// in-memory claim can't reach the fold, the same rejection class
// as outbox emission.
Gameplay::Result<std::uint64_t>
LedgerMerkleRoot(const Ledger& l, std::span<const MintClaim> claims);

} // namespace Potato::Campaign
