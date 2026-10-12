#pragma once

#include "Campaign/Ledger/Ledger.h"
#include "Gameplay/Doctrine/Doctrine.h" // SimEvent
#include "Gameplay/Result.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace Potato::Campaign {

// Battle deeds -> ledger postings (Story 4.2, the A-2 auto-detection
// seam). The recorded SimEvent stream is the truth: BookDeeds folds
// the player's deeds into double-entry postings — atrocity kinds
// (VillageBurned, SquadExecuted) carry the "atrocity" audit tag plus
// corruption:/order: governance deltas that the 墮落 ratchet and
// 秩序 accumulator fold (4.3 — computed, never written).
//
// Scope: only deeds committed by `playerSide` post to this ledger —
// it is the player's domain chronicle; enemy deeds are the enemy's
// ledger problem (and survive in the battle record regardless).
// Myth deeds ride the same bridge (5.3): ShrineCaptured credits 天命
// — the earn leg of the gods' economy.
// Event kinds that aren't deeds (CardFired, BeatChanged, ...) are
// skipped silently; deed events stamped -1 or for the other side
// post nothing. MythActionInvoked is deliberately not a deed: its
// ledger entry is the SpendMandate posting written at action time
// (5.3/5.4) — booking it again here would double-count the cost.
// MythInvasion (5.5) is the one exemption to the side-gate: a
// visitation lands on the player's chronicle whether the god's host
// marched for us (blessing — bills 天命, 神助要還) or against us
// (terror — 民心 empties, ranks stiffen).
//
// Leg pricing is the architecture's example economics — asymmetric
// costs are the design: burning banks 物資 against 民心, execution
// banks 軍威 dread against 民心 revulsion. Magnitudes are initial
// balance targets, not final tuning.
//
// World attribution (Story 12.7): a non-empty `worldNode` stamps
// `region:<node>` on every generated posting — the battle happened
// AT that world node, so its deeds fold into that region's
// 民心/秩序/墮落. Map-local regions still ride as `field:<n>`.
//
// INVARIANT for future deed kinds: every generated posting must
// satisfy ValidateLegs + ValidateMeta — ResolveAftermath (4.5)
// preflights only capacity, so a malformed deed would post a prefix
// then fail mid-settlement, reopening the partial-land hole.
Gameplay::Result<std::size_t>
BookDeeds(Ledger& ledger, int playerSide,
          std::span<const Gameplay::SimEvent> events,
          std::string_view worldNode = {});

} // namespace Potato::Campaign
