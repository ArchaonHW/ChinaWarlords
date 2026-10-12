#pragma once

#include "Campaign/Chapters/ChapterLibrary.h"
#include "Campaign/Governance/Accumulators.h"
#include "Campaign/State/CampaignState.h"
#include "Gameplay/Result.h"

#include <cstdint>

namespace Potato::Campaign {

// Governance victory path (Story 4.4): one of the GDD's three
// chapter-victory paths (military / governance / subversion). 民心
// >= 70 and 秩序 >= 60 held at chapter end resolves as a victory of
// RULE — and ruling well beats fighting well, so the thresholds
// override the field result entirely: a chapter won in the ledger
// does not need a won battle.
constexpr std::int64_t VICTORY_POPULAR_SUPPORT = 70;
constexpr std::int64_t VICTORY_ORDER = 60;

enum class ChapterResolution : std::uint8_t {
    BattleVictory = 0, // field won, governance thresholds unmet
    GovernanceVictory, // thresholds met — the ledger won the chapter
    Defeat,            // field lost AND thresholds unmet; Story 4.5
                       // owns what conversion does with it
};

// Canonical spellings — the `resolution:` tag payload and the
// chronicle render share them.
const char* ResolutionName(ChapterResolution r);

// The threshold check, pure. Exported so callers can preview the
// outcome without committing the chapter.
bool IsGovernanceVictory(const GovernanceAccumulators& a);

// The chapter-end ceremony. Folds the ledger, decides the
// resolution (governance thresholds FIRST — they override
// battleWon), seals the verdict into the ledger as a
// `resolution:<kind>` + `chapter:<idx>` tagged posting, then
// advances progression. The seal is a real double-entry posting
// (token amount 1: the tag is the verdict, the legs keep the books
// honest).
//
// `battleWon` means the chapter's field/objective succeeded — a
// zero-combat chapter (ChapterDef.combat==false) reports its own
// outcome (negotiation/deterrence) through the same flag.
//
// The governance seal debits 民心 by the token grain — the seal
// consumes the resource it measures, so k prior governance
// victories raise the practical threshold to 70+k across the
// campaign. Deliberate: support consolidates into mandate.
//
// Atomicity: preflight uses CanResolveChapter — the SAME gate list
// ResolveAndAdvance enforces — plus ledger capacity, so once the
// seal lands the advance cannot fail; the verdict is in the book
// before the chapter counts as closed, and a chapter that cannot
// close never gets a seal.
//
// `recordRoot` (when nonzero) anchors the seal to the battle
// record's integrity root via the `record_root:` tag family
// (CrossCheck, Story 2.5) — the verdict cites its evidence.
Gameplay::Result<ChapterResolution> ConcludeChapter(
    CampaignState& state, const ChapterLibrary& lib, bool battleWon,
    std::uint64_t recordRoot = 0);

} // namespace Potato::Campaign
