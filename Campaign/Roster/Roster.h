#pragma once

#include "Campaign/State/CampaignState.h"
#include "Gameplay/Result.h"

#include <string>
#include <vector>

namespace Potato::Campaign {

// Persistent roster (Story 3.5): the roster's business logic.
// 3.1 shipped the storage spine; these are its write paths —
// both enforce the same invariants FromJson holds at the wire.
// GetRoster() still hands out a mutable vector& (test/legacy
// seam), so ToJson re-validates the emitted roster: a save
// FromJson would reject is never written.
//
// Dead entries are memorials: corpses keep their roster slots
// and names forever — no disband/reuse path exists by design.
// A long campaign can exhaust MAX_ROSTER this way; that's the
// intended weight of permanent loss.

// One squad's outcome in a battle, keyed by roster NAME — the
// campaign identity. The battle layer reports per-squad events
// keyed by deploy index; the caller maps index -> name at
// deployment (BattleResult carries only aggregate side counts).
struct AftermathRow {
    std::string name;
    std::int64_t casualties = 0; // this battle's dead
    bool wiped = false;          // squad destroyed -> dead=true
};

struct AftermathResult {
    int rows = 0;     // rows applied
    int buried = 0;   // squads wiped (dead=true)
    int veterans = 0; // squads that survived (veterancy++)
};

// Shared enlistability predicate — nullptr means the name can be
// enlisted. Used by Enlist AND by RefitCamp::Recruit's pre-post
// gate, so a paid recruit can't strand on a missed invariant.
const char* CanEnlist(const CampaignState& state,
                      const std::string& name);

// Adds a named squad. Fails on: empty/oversize name, duplicate
// name, roster full. Returns the new entry's index.
Gameplay::Result<int> Enlist(CampaignState& state,
                             const std::string& name);

// Applies a casualty report. Atomic — ALL rows validate before
// ANY mutation: unknown name, already-dead squad, duplicate row
// name, or counter bound overflow rejects the whole aftermath.
// Dead is permanent: a wiped squad stays dead forever. Survivors
// (any non-wiped row) gain veterancy. Empty report is a no-op.
Gameplay::Result<AftermathResult> ApplyAftermath(
    CampaignState& state, const std::vector<AftermathRow>& rows);

} // namespace Potato::Campaign
