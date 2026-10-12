#include "Campaign/Chapters/Progression.h"

#include "Campaign/Governance/Accumulators.h"
#include "Campaign/World/WorldState.h"

namespace Potato::Campaign {

using Gameplay::Result;

namespace {

// Progress vectors must match the library's chapter space.
bool SpaceMatches(const ChapterProgress& cp,
                  const ChapterLibrary& lib) {
    return cp.unlocked.size() == cp.resolved.size() &&
           cp.unlocked.size() ==
               static_cast<std::size_t>(lib.MaxIndex() + 1);
}

// Index of `def` inside the index-sorted Chapters() vector —
// predecessor rule walks this order, not index-1.
std::size_t PositionOf(const ChapterLibrary& lib,
                       const ChapterDef& def) {
    const auto& cs = lib.Chapters();
    for (std::size_t i = 0; i < cs.size(); ++i) {
        if (cs[i].index == def.index) return i;
    }
    return cs.size();
}

std::int64_t LedgerAxisValue(const GovernanceAccumulators& acc,
                             LedgerAxis axis) {
    switch (axis) {
        case LedgerAxis::PopularSupport: return acc.popularSupport;
        case LedgerAxis::Order: return acc.order;
        case LedgerAxis::Corruption: return acc.corruption;
    }
    return 0;
}

// `current` repoint after a resolution or init: lowest-index
// playable chapter, else the next gate (lowest unresolved), else
// the complete sentinel.
void RepointCurrent(CampaignState& state,
                    const ChapterLibrary& lib) {
    ChapterProgress& cp = state.GetChapter();
    const auto space = static_cast<std::int64_t>(cp.unlocked.size());
    std::int64_t playable = -1;
    std::int64_t nextGate = -1;
    for (const ChapterDef& c : lib.Chapters()) {
        const auto i = static_cast<std::size_t>(c.index);
        if (cp.resolved[i]) continue;
        if (nextGate < 0) nextGate = c.index;
        if (playable < 0 && cp.unlocked[i]) playable = c.index;
    }
    if (playable >= 0) {
        cp.current = playable;
    } else if (nextGate >= 0) {
        cp.current = nextGate;
    } else {
        cp.current = space; // everything resolved — sentinel
    }
}

} // namespace

bool ChapterAvailable(const ChapterDef& def,
                      const ChapterLibrary& lib,
                      const CampaignState& state,
                      const WorldState* ws,
                      const WorldMap* world) {
    const ChapterProgress& cp = state.GetChapter();
    const auto i = static_cast<std::size_t>(def.index);
    if (i >= cp.resolved.size() || cp.resolved[i]) return false;

    if (!def.bound) {
        // Legacy predecessor rule: available once the previous
        // library entry is resolved (the first entry always is).
        const std::size_t pos = PositionOf(lib, def);
        if (pos == 0) return true;
        if (pos >= lib.Chapters().size()) return false;
        const auto prev =
            static_cast<std::size_t>(lib.Chapters()[pos - 1].index);
        return cp.resolved[prev];
    }

    const ChapterBind& b = def.bind;
    for (const std::string& req : b.prereqs) {
        const ChapterDef* r = lib.Find(req);
        if (!r) return false; // unreachable — Load prunes, saves don't
        if (!cp.resolved[static_cast<std::size_t>(r->index)]) {
            return false;
        }
    }
    if (!b.node.empty() && !b.hasControl) {
        // Pure arrival gate — the warband discovers the 回目
        // by marching onto its node.
        if (!ws || ws->WarbandAt() != b.node) return false;
    }
    if (b.hasControl) {
        // Held-region predicate — `node` is control's argument;
        // the gate reads the region's control, not the warband's
        // position (holding the ground opens the book).
        if (!ws || ws->ControlAt(b.node) != b.control) return false;
    }
    if (!b.resolved.empty()) {
        if (!ws) return false;
        for (const std::string& id : b.resolved) {
            if (!ws->IsResolved(id)) return false;
        }
    }
    if (b.hasLedger) {
        const GovernanceAccumulators acc =
            FoldGovernance(state.GetLedger());
        if (LedgerAxisValue(acc, b.axis) < b.atLeast) return false;
    }
    (void)world;
    return true;
}

void RefreshAvailability(CampaignState& state,
                         const ChapterLibrary& lib,
                         const WorldState* ws,
                         const WorldMap* world) {
    ChapterProgress& cp = state.GetChapter();
    if (cp.unlocked.size() != cp.resolved.size() ||
        cp.unlocked.size() !=
            static_cast<std::size_t>(lib.MaxIndex() + 1)) {
        return; // uninitialized or mismatched space — nothing to latch
    }
    for (const ChapterDef& c : lib.Chapters()) {
        const auto i = static_cast<std::size_t>(c.index);
        if (cp.unlocked[i]) continue; // latched — never re-locks
        if (ChapterAvailable(c, lib, state, ws, world)) {
            cp.unlocked[i] = true;
        }
    }
}

bool CampaignComplete(const CampaignState& state,
                      const ChapterLibrary& lib) {
    const ChapterProgress& cp = state.GetChapter();
    if (cp.resolved.empty()) return false;
    bool anyMandatory = false;
    for (const ChapterDef& c : lib.Chapters()) {
        if (c.bound && c.bind.IsMandatory()) anyMandatory = true;
    }
    for (const ChapterDef& c : lib.Chapters()) {
        const auto i = static_cast<std::size_t>(c.index);
        if (anyMandatory && !(c.bound && c.bind.IsMandatory())) {
            continue; // side content doesn't gate completion
        }
        if (!cp.resolved[i]) return false;
    }
    return true;
}

bool SetCurrentChapter(CampaignState& state,
                       const ChapterLibrary& lib,
                       std::int64_t index) {
    const ChapterDef* def = lib.AtIndex(index);
    if (!def) return false;
    ChapterProgress& cp = state.GetChapter();
    const auto i = static_cast<std::size_t>(index);
    if (i >= cp.unlocked.size() || !cp.unlocked[i] ||
        cp.resolved[i]) {
        return false;
    }
    cp.current = index;
    return true;
}

Result<int> InitializeProgress(CampaignState& state,
                               const ChapterLibrary& lib) {
    if (lib.Size() == 0 || lib.MaxIndex() < 0) {
        return Gameplay::Fail<int>("content",
                                   "chapter library is empty");
    }
    if (lib.MaxIndex() + 1 >
        static_cast<std::int64_t>(CampaignState::MAX_CHAPTERS)) {
        return Gameplay::Fail<int>(
            "content", "chapter space exceeds save format bound");
    }
    ChapterProgress& cp = state.GetChapter();
    // Boot-only: refuses to silently wipe a campaign in progress.
    // A true reset is a caller decision (construct fresh state).
    if (!cp.unlocked.empty() || !cp.resolved.empty()) {
        return Gameplay::Fail<int>(
            "state", "progress already initialized");
    }
    const std::size_t space =
        static_cast<std::size_t>(lib.MaxIndex() + 1);
    cp.unlocked.assign(space, false);
    cp.resolved.assign(space, false);
    cp.current = lib.Chapters().front().index;
    // Null-world refresh: unbound first chapters and requires/
    // ledger-only binds latch; node-bound content waits for the
    // shell's world-beat refresh.
    RefreshAvailability(state, lib, nullptr, nullptr);
    if (!cp.unlocked[static_cast<std::size_t>(cp.current)]) {
        // First entry is itself gated — start `current` on the
        // lowest playable chapter (or the first gate if none).
        RepointCurrent(state, lib);
    }
    return Gameplay::Ok(static_cast<int>(cp.current));
}

const char* CanResolveChapter(const CampaignState& state,
                              const ChapterLibrary& lib,
                              std::string_view& error) {
    const ChapterProgress& cp = state.GetChapter();
    if (!SpaceMatches(cp, lib)) {
        error = "state";
        return "progress vectors don't match chapter space";
    }
    const auto space = static_cast<std::int64_t>(cp.unlocked.size());
    if (space == 0) {
        error = "state";
        return "progress not initialized";
    }
    if (cp.current < 0 || cp.current > space) {
        error = "state";
        return "current out of space";
    }
    if (cp.current == space) {
        error = "campaign";
        return "campaign already complete";
    }
    if (lib.AtIndex(cp.current) == nullptr) {
        error = "state";
        return "current is not a registered chapter";
    }
    // Untrusted-state posture: a crafted save can't resolve a
    // chapter that was never unlocked, nor re-resolve one.
    const auto cur = static_cast<std::size_t>(cp.current);
    if (!cp.unlocked[cur] || cp.resolved[cur]) {
        error = "state";
        return "current chapter is not in play";
    }
    return nullptr;
}

Result<int> ResolveAndAdvance(CampaignState& state,
                              const ChapterLibrary& lib) {
    std::string_view error;
    if (const char* why = CanResolveChapter(state, lib, error)) {
        return Gameplay::Fail<int>(std::string(error), why);
    }
    ChapterProgress& cp = state.GetChapter();

    cp.resolved[static_cast<std::size_t>(cp.current)] = true;

    // Null-world refresh: requires/ledger binds may latch here;
    // node-bound predicates latch at the shell's world-beat
    // refresh. Unbound next-in-sequence reproduces the legacy
    // linear advance.
    RefreshAvailability(state, lib, nullptr, nullptr);

    if (CampaignComplete(state, lib)) {
        const auto space =
            static_cast<int>(cp.unlocked.size()); // sentinel
        cp.current = space;
        return Gameplay::Ok(space);
    }
    RepointCurrent(state, lib);
    return Gameplay::Ok(static_cast<int>(cp.current));
}

} // namespace Potato::Campaign
