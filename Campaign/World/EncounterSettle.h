#pragma once

#include "Campaign/Governance/Victory.h" // ChapterResolution
#include "Campaign/Roster/Roster.h"      // AftermathResult/Row
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Potato::Gameplay {
struct SimEvent;
}

namespace Potato::Campaign {

class CampaignState;
class MythLog;
class WorldMap;
class WorldState;
struct EncounterDef;

// What one encounter's settlement produced (Story 12.5). Mirrors
// ChapterSettlement minus the chapter: the ledger still decides
// the verdict, casualties still bury, deeds still post — but the
// writeback lands on the WORLD (resolved marker, control stake)
// instead of advancing progression. There is deliberately no
// "game over" field: a lost field still settles (the 4.5
// converted-defeat rule — no re-trigger death-loop, the warband
// stays put).
struct EncounterSettlement {
    ChapterResolution resolution = ChapterResolution::Defeat;
    AftermathResult roster;
    std::size_t deedsPosted = 0;
    std::size_t mythLogged = 0;
    bool controlFlipped = false;
};

// The encounter-settlement ceremony — ResolveAftermath's order,
// minus the chapter:
//
//   1. casualties persist to the roster (atomic)
//   2. player-side deeds post to the ledger
//   3. myth invasions fold into the MythLog
//   4. FoldGovernance thresholds override the field result (the
//      ledger decides, same as ConcludeChapter), then the verdict
//      seals with the SAME leg table — memo/tags cite the
//      encounter, never the chapter
//   5. world writeback: MarkResolved(enc.id) ALWAYS (defeat
//      included — the marker prevents a refire deadlock), and a
//      won battle with a control stake enqueues SetControl at the
//      current day, drained canonically by ResolveBeats(0)
//
// Preflighted so the mutating stages cannot fail: record root
// must be nonzero and not already anchored (shared
// SettlementRecorded check), playerSide must be 0/1, the node
// must still exist in the world doc, ledger capacity covers
// deeds + the seal, the MythLog covers invasions, the world
// queue has room, the resolved set has room, and the seal's
// "encounter:"/"region:" tags fit MAX_TAG_LEN. Either the whole
// settlement lands or nothing does.
Gameplay::Result<EncounterSettlement> SettleEncounter(
    CampaignState& state, WorldState& ws, const WorldMap& world,
    const EncounterDef& enc, bool battleWon, int playerSide,
    std::span<const Gameplay::SimEvent> deeds,
    const std::vector<AftermathRow>& casualties,
    std::uint64_t recordRoot, MythLog* mythLog = nullptr);

} // namespace Potato::Campaign
