#pragma once

#include "Campaign/Chapters/ChapterLibrary.h"
#include "Campaign/State/CampaignState.h"
#include "Gameplay/Result.h"

#include <string_view>

namespace Potato::Campaign {

class WorldMap;
class WorldState;

// Chapter shell & progression (Stories 3.4 + 12.6): binds the
// ChapterLibrary registry to the persisted ChapterProgress.
//
// Chapter space contract: `unlocked`/`resolved` are sized to
// `lib.MaxIndex()+1` and indexed BY ChapterDef.index — sparse
// indexes leave dead flag slots. `current` holds a ChapterDef
// .index (or MaxIndex()+1 = campaign-complete sentinel).
//
// Carry-forward is structural: roster and ledger live in
// CampaignState and progression never resets them — the same
// objects flow into the next chapter.
//
// Availability (12.6): an UNBOUND chapter follows the legacy
// predecessor rule — it is available once the previous library
// entry is resolved (the lowest-index chapter always is). A
// BOUND chapter ignores index order: every `bind.requires`
// chapter must be resolved AND every world predicate must hold
// right now (node arrival, region control, POI resolution,
// ledger threshold). `unlocked` LATCHES — predicates that once
// held keep the flag; the warband marching on never re-locks.
//
// `current` is "the chapter in play, or the next gate": after a
// resolution it repoints at the lowest-index unlocked-and-
// unresolved chapter; when none are playable it points at the
// lowest-index unresolved chapter (the gate the campaign is
// waiting on); when the spine is done it lands the sentinel.

// Pure availability predicate. Unbound: predecessor rule.
// Bound: all `requires` resolved + all world predicates hold —
// node/control/resolved evaluate against `ws`; a null `ws`
// makes them UNSATISFIABLE (no world, no place). `ledger` folds
// state.GetLedger() and works without a world. Already-resolved
// chapters are never "available". `world` is reserved for
// node-existence checks (unused today; pass nullptr).
bool ChapterAvailable(const ChapterDef& def,
                      const ChapterLibrary& lib,
                      const CampaignState& state,
                      const WorldState* ws,
                      const WorldMap* world);

// Latch pass: set unlocked[i] for every available unresolved
// chapter. Monotone — never clears a flag. Call after world
// beats and settlements with the live world (nullptr-safe).
void RefreshAvailability(CampaignState& state,
                         const ChapterLibrary& lib,
                         const WorldState* ws,
                         const WorldMap* world);

// Any mandatory marker present (bind.mandatory or bind.beat) →
// complete when every mandatory chapter is resolved. Otherwise
// complete when every chapter is resolved (legacy semantics —
// the sentinel convention rides on this).
bool CampaignComplete(const CampaignState& state,
                      const ChapterLibrary& lib);

// The shell's pick among unlocked chapters: `index` must name a
// registered chapter that is unlocked and unresolved. On success
// `current` moves there. Invalid picks leave state untouched.
bool SetCurrentChapter(CampaignState& state,
                       const ChapterLibrary& lib,
                       std::int64_t index);

// Sizes the chapter vectors and points `current` at the library's
// first chapter (unlocked). Fails on an empty library or one
// whose MaxIndex exceeds the save format's bound.
Gameplay::Result<int> InitializeProgress(
    CampaignState& state, const ChapterLibrary& lib);

// Records the current chapter as resolved (win OR converted
// defeat — the caller decides; Epic 4 owns conversion), then
// refreshes availability and repoints `current`. Unbound
// chapters reproduce the legacy linear advance exactly; bound
// chapters latch only when their `requires`/ledger gates hold
// here (null-world refresh) — node-bound predicates latch at
// the shell's world-beat RefreshAvailability call. Past the
// spine's end, `current` becomes the complete sentinel.
// Fails when: campaign already complete, `current` isn't a
// registered chapter, or the progress vectors don't match the
// library's chapter space.
Gameplay::Result<int> ResolveAndAdvance(
    CampaignState& state, const ChapterLibrary& lib);

// The same gates ResolveAndAdvance enforces, exported as a pure
// predicate. ConcludeChapter (Story 4.4) preflights with it so a
// ledger seal only lands when the advance cannot fail — the gate
// list is shared, not mirrored, so the two can never drift.
// Returns nullptr when the current chapter can close, else a
// static reason; `error` receives the gate's error class.
const char* CanResolveChapter(const CampaignState& state,
                              const ChapterLibrary& lib,
                              std::string_view& error);

} // namespace Potato::Campaign
