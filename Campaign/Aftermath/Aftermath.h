#pragma once

#include "Campaign/Chapters/ChapterLibrary.h"
#include "Campaign/Governance/Victory.h"
#include "Campaign/Roster/Roster.h"
#include "Campaign/State/CampaignState.h"
#include "Gameplay/Doctrine/Doctrine.h" // SimEvent
#include "Gameplay/Result.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Potato::Campaign {

class MythLog;

// What one battle's settlement produced. `resolution` is the
// ledger-sealed verdict; `nextChapter` is the chapter now in play
// (or the campaign-complete sentinel). There is deliberately no
// "game over" field: a single lost battle cannot end the run —
// defeat converts to continuation (Story 4.5).
struct ChapterSettlement {
    ChapterResolution resolution = ChapterResolution::Defeat;
    AftermathResult roster;      // rows applied / buried / veterans
    std::size_t deedsPosted = 0; // ledger entries BookDeeds wrote
    std::size_t mythLogged = 0;  // MythLog entries the fold appended
    int nextChapter = 0;         // chapter index now in play
};

// The aftermath ceremony — one call settles one chapter
// (Story 4.5, the Epic-4 wiring seam). Order is semantics:
//
//   1. casualties persist to the roster (atomic; a bad report
//      rejects the whole settlement)
//   2. player-side deeds post to the ledger (atrocity corruption
//      tags feed the fold — a brutal loss CAN ratchet 墮落)
//   3. the chapter seals its verdict and advances — deeds are
//      inside the fold when the thresholds are read
//
// Preflighted so the mutating stages cannot fail: the shared
// CanResolveChapter gate plus a ledger-capacity bound run first;
// ApplyAftermath is atomic by itself; BookDeeds and the seal post
// are then guaranteed. Either the whole settlement lands or the
// campaign is untouched.
//
// `battleWon` is the chapter's field/objective outcome — a defeat
// still seals and advances (converted defeat is a chapter outcome;
// recovery is next-chapter + RefitCamp, not a reload).
// `playerSide` must be 0 or 1 — validated in preflight, before any
// mutation. Settling the final chapter yields `nextChapter` equal
// to the campaign-complete sentinel (continuation is vacuous
// there: the campaign is over, not game-over'd).
//
// `recordRoot` is the battle record's integrity root — REQUIRED
// nonzero. It anchors the resolution seal (`record_root:` tag,
// CrossCheck) AND makes settlement idempotent: a root already
// anchored in the ledger rejects before any mutation, so an
// ambiguous-failure retry can't double-bury the roster or seal a
// second verdict into the next chapter.
// `mythLog` is optional (Story 5.5): when non-null the invasion
// events in `deeds` are folded into the MythLog alongside the deeds
// posting — the visitation books into ledger AND chronicle in one
// settlement. Capacity is preflighted like the ledger's.
Gameplay::Result<ChapterSettlement> ResolveAftermath(
    CampaignState& state, const ChapterLibrary& lib, bool battleWon,
    int playerSide,
    std::span<const Gameplay::SimEvent> deeds,
    const std::vector<AftermathRow>& casualties,
    std::uint64_t recordRoot,
    MythLog* mythLog = nullptr);

// A settled battle's record root anchors its resolution seal —
// finding it already in the ledger means this battle was already
// recorded (idempotency: the retry sees "recorded", not success).
// Exported for the encounter-settlement preflight (Story 12.5),
// which shares the rule but not the chapter machinery.
bool SettlementRecorded(const Ledger& l, std::uint64_t root);

} // namespace Potato::Campaign
