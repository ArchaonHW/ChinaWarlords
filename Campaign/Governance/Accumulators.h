#pragma once

#include "Campaign/Ledger/Ledger.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace Potato::Campaign {

// Derived governance state (Story 4.3): 民心/秩序/墮落 computed by
// folding the ledger — "computed truth, not a stat bar". There is no
// accumulator object to write: the ONLY way a governance number
// changes is a posting.
struct GovernanceAccumulators {
    // 民心 — net postings to the PopularSupport account.
    std::int64_t popularSupport = 0;
    // 秩序 — net fold of `order:±N` tags (order is not an account;
    // tags are the channel).
    std::int64_t order = 0;
    // 墮落 — running MAXIMUM of the `corruption:±N` fold. Penance
    // amortizes against the running sum (fresh cruelty hides until
    // the debt is outrun) but the peak is never repaid — "a one-way
    // debt that can only be outrun" (GDD). Monotonic across any
    // entry sequence by construction.
    std::int64_t corruption = 0;
};

// Pure fold — reads the whole chain, writes nothing, stores nothing.
// Forged entries fold identically to honest ones (2.3 contract:
// suspicion is data, not exclusion) — a forged atrocity still
// ratchets 墮落.
GovernanceAccumulators FoldGovernance(const Ledger& l);

// Per-world-node folds (Story 12.7) derived from `region:<id>`-tagged
// entries: a tagged entry's PopularSupport legs net into the region's
// 民心, its order:/corruption: tags into its 秩序/墮落 — same
// saturating math and per-(axis,value)-dedupe rules as the campaign
// fold. An entry carrying several distinct region tags folds into
// each; untagged entries feed only the campaign totals. std::map
// keeps the output id-sorted for determinism. Derived state — never
// stored, same discipline as FoldGovernance.
std::map<std::string, GovernanceAccumulators>
FoldGovernanceByRegion(const Ledger& l);

// Per-tag magnitude bound. Governance deltas are deed-scale numbers
// (double digits today); the cap exists because tags are unvalidated
// fold inputs — a hostile `corruption:-9e18` would otherwise poison
// the running sum so deep that honest atrocities could never lift it
// back, and a forged +INT64_MAX would pin the rail permanently.
// Over-cap tags are malformed — they fold NOTHING, not a clamp.
constexpr std::int64_t MAX_GOVERNANCE_DELTA = 1000;

// Tag grammar: "<axis>:<signed int64>" — order:5, order:-10,
// corruption:+15. Sign optional; magnitude <= MAX_GOVERNANCE_DELTA.
// Anything malformed folds nothing. Exported for validation and
// future folds (producers like DeedBook emit literals within it).
//
// Producer convention (AC wording): 墮落 equals the max over
// atrocity-tagged folds BECAUSE atrocity deeds are the producers of
// corruption:+N tags — the fold reads the axis, not the atrocity
// tag. A `corruption:+N` posting without `atrocity` still folds;
// that's a producer-contract matter, not a fold invariant.
bool ParseGovernanceTag(std::string_view tag, std::string_view axis,
                        std::int64_t& out);

} // namespace Potato::Campaign
