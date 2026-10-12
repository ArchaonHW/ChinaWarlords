#include "Gameplay/Myth/Infiltration.h"

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Squad/Squad.h"

namespace Potato::Gameplay {

namespace {

// Same exclusive-presence discipline GovernanceField uses (the
// field owns its invariant — replicated deliberately, not shared):
// the side with >= 1 effective non-Routing squad at `region`, -1
// when empty or contested.
int ExclusiveSide(const std::vector<Squad>& squads,
                  std::size_t region) {
    int side = -1;
    for (const Squad& s : squads) {
        if (!s.IsEffective() || s.state == SquadState::Routing ||
            s.regionIndex != region) continue;
        if (side == -1) {
            side = s.side;
        } else if (side != s.side) {
            return -1; // contested
        }
    }
    return side;
}

// First effective non-Routing squad of `side` at `region`, or -1 —
// the canonical witness (lowest squad index, deterministic).
int FirstAt(const std::vector<Squad>& squads, int side,
            std::size_t region) {
    for (std::size_t i = 0; i < squads.size(); ++i) {
        const Squad& s = squads[i];
        if (s.side == side && s.IsEffective() &&
            s.state != SquadState::Routing &&
            s.regionIndex == region) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

static_assert(static_cast<int>(InfiltrationLevel::Invaded) + 1 ==
                  static_cast<int>(kInfiltrationLevelCount),
              "InfiltrationLevel grew — update the transition table");
static_assert(static_cast<int>(MythEventKind::Pacification) + 1 ==
                  static_cast<int>(kMythEventKindCount),
              "MythEventKind grew — update the transition table");

InfiltrationLevel Transition(InfiltrationLevel level,
                             MythEventKind event) {
    // [level][event] — see the header for the spec table.
    static constexpr std::uint8_t TABLE[kInfiltrationLevelCount]
                                       [kMythEventKindCount] = {
        {1, 0}, // None
        {2, 0}, // Whispered
        {3, 1}, // Haunted
        {3, 2}, // Invaded
    };
    const auto l = static_cast<std::size_t>(level);
    const auto e = static_cast<std::size_t>(event);
    if (l >= kInfiltrationLevelCount || e >= kMythEventKindCount) {
        return level; // unreachable through the wire gates
    }
    return static_cast<InfiltrationLevel>(TABLE[l][e]);
}

const char* InfiltrationLevelName(InfiltrationLevel level) {
    switch (level) {
    case InfiltrationLevel::None:      return "none";
    case InfiltrationLevel::Whispered: return "whispered";
    case InfiltrationLevel::Haunted:   return "haunted";
    case InfiltrationLevel::Invaded:   return "invaded";
    }
    return "unknown";
}

const char* MythEventKindName(MythEventKind kind) {
    switch (kind) {
    case MythEventKind::Incursion:    return "incursion";
    case MythEventKind::Pacification: return "pacification";
    }
    return "unknown";
}

bool InfiltrationLevelFromInt(std::int64_t v, InfiltrationLevel& out) {
    if (v < 0 ||
        v >= static_cast<std::int64_t>(kInfiltrationLevelCount)) {
        return false;
    }
    out = static_cast<InfiltrationLevel>(v);
    return true;
}

bool MythEventKindFromInt(std::int64_t v, MythEventKind& out) {
    if (v < 0 || v >= static_cast<std::int64_t>(kMythEventKindCount)) {
        return false;
    }
    out = static_cast<MythEventKind>(v);
    return true;
}

const char* MythActionKindName(MythActionKind kind) {
    switch (kind) {
    case MythActionKind::PacifyShrine:     return "pacify_shrine";
    case MythActionKind::InvokePossession: return "invoke_possession";
    case MythActionKind::RaiseGhostArmy:   return "ghost_army";
    }
    return "unknown";
}

bool MythActionKindFromInt(std::int64_t v, MythActionKind& out) {
    if (v < 0 || v >= static_cast<std::int64_t>(kMythActionKindCount)) {
        return false;
    }
    out = static_cast<MythActionKind>(v);
    return true;
}

const char* GodStanceName(GodStance stance) {
    switch (stance) {
    case GodStance::Wrathful:  return "wrathful";
    case GodStance::Neutral:   return "neutral";
    case GodStance::Favorable: return "favorable";
    }
    return "unknown";
}

void MythField::Init(const BattleMap& map) {
    levels_.assign(map.RegionCount(), 0);
    ghosts_.assign(map.RegionCount(), -1);
    shrines_.clear();
    for (std::size_t r = 0; r < map.RegionCount(); ++r) {
        if (map.RegionAt(r).myth & MYTH_SHRINE) {
            ShrineTrack t;
            t.region = r;
            shrines_.push_back(t);
        }
    }
}

bool MythField::Seed(std::size_t region, InfiltrationLevel level) {
    if (region >= levels_.size() ||
        static_cast<std::size_t>(level) >= kInfiltrationLevelCount) {
        return false;
    }
    // Journal contract: an accepted call must change observable
    // state — a seed to the current level is rejected, never
    // recorded, so no-op ops can't ride a valid record.
    if (levels_[region] == static_cast<std::uint8_t>(level)) {
        return false;
    }
    levels_[region] = static_cast<std::uint8_t>(level);
    return true;
}

std::optional<SimEvent> MythField::Apply(std::size_t region,
                                         MythEventKind kind,
                                         int tick) {
    if (region >= levels_.size() ||
        static_cast<std::size_t>(kind) >= kMythEventKindCount) {
        return std::nullopt;
    }
    const InfiltrationLevel next =
        Transition(static_cast<InfiltrationLevel>(levels_[region]),
                   kind);
    if (next == static_cast<InfiltrationLevel>(levels_[region])) {
        return std::nullopt;
    }
    levels_[region] = static_cast<std::uint8_t>(next);
    // param = region, aux = new level (0..3 fits the 0..6 aux wire
    // gate), squadIndex/side = -1: the myth layer is side-less —
    // spirits answer to no banner.
    return SimEvent{SimEvent::Kind::InfiltrationChanged, tick, -1, -1,
                    static_cast<int>(region),
                    static_cast<int>(next), -1, {}, {}};
}

InfiltrationLevel MythField::LevelAt(std::size_t region) const {
    if (region >= levels_.size()) return InfiltrationLevel::None;
    return static_cast<InfiltrationLevel>(levels_[region]);
}

std::optional<std::vector<SimEvent>>
MythField::PacifyRegion(std::size_t region, int tick) {
    if (region >= levels_.size()) return std::nullopt;
    // Pre-state: quiet land holding no allegiance has nothing to
    // pacify — nullopt, the caller rejects (journal no-op rule).
    const ShrineTrack* shrine = ShrineAt(region);
    const bool releasable = shrine != nullptr && shrine->owner >= 0;
    const bool infiltrated =
        levels_[region] !=
        static_cast<std::uint8_t>(InfiltrationLevel::None);
    if (!releasable && !infiltrated) return std::nullopt;
    std::vector<SimEvent> out;
    if (auto ev = Apply(region, MythEventKind::Pacification, tick)) {
        out.push_back(*ev);
    }
    // A pacified god withdraws his allegiance — the latch frees but
    // the stance memory stays (the god remembers; 5.2 contract).
    // The release itself emits no event: MythActionInvoked covers
    // the act; `out` only carries the infiltration transition.
    if (ShrineTrack* s = ShrineAtMut(region)) {
        s->owner = -1;
        s->dwell = 0;
    }
    return out; // engaged even when empty — release alone is a change
}

bool MythField::RaiseGhost(std::size_t region, int side) {
    if (region >= levels_.size() || (side != 0 && side != 1)) {
        return false;
    }
    // The veil must be thin — spirits don't rise on quiet land.
    if (levels_[region] ==
        static_cast<std::uint8_t>(InfiltrationLevel::None)) {
        return false;
    }
    if (ghosts_[region] != -1) return false; // one garrison holds it
    ghosts_[region] = static_cast<std::int8_t>(side);
    return true;
}

int MythField::GhostAt(std::size_t region) const {
    if (region >= ghosts_.size()) return -1;
    return ghosts_[region];
}

std::vector<SimEvent>
MythField::Tick(const std::vector<Squad>& squads, int tick) {
    std::vector<SimEvent> out;
    for (ShrineTrack& s : shrines_) {
        const bool invaded =
            s.region < levels_.size() &&
            levels_[s.region] ==
                static_cast<std::uint8_t>(InfiltrationLevel::Invaded);
        // ghosts_ is region-indexed like levels_ — a shrine's region
        // is always in bounds (Init builds both from the same map).
        std::int8_t& ghost = ghosts_[s.region];
        // A wild haunting subsides the moment the land drops below
        // Invaded — pacify is the counter-play that reopens a
        // seeded-3 shrine to dedication. Side-bannered garrisons
        // are contracted spirit hosts; they do NOT dissipate.
        if (!invaded && ghost == GHOST_WILD) ghost = -1;
        // --- Invasion (Story 5.5): the deity pushes back on a
        // cadence while its ground sits overrun. The pushback side
        // is read off FLESH presence only — the rising host is what
        // this tick blends in, so it can't claim for itself.
        if (invaded && tick - s.invasionTick >= INVASION_PERIOD_TICKS) {
            s.invasionTick = tick;
            int aux;
            int side;
            if (ghost == -1) {
                const int flesh = ExclusiveSide(squads, s.region);
                if (flesh == 0 || flesh == 1) {
                    // The god refuses the occupier — the host rises
                    // for the OTHER banner (aux 0 = pushback rise).
                    ghost = static_cast<std::int8_t>(1 - flesh);
                    aux = 0;
                    side = ghost;
                } else {
                    // Empty or contested ground gets the unaligned
                    // haunting (aux 1 = wild) — no banner may claim
                    // sacred ground while the god is at open war.
                    ghost = GHOST_WILD;
                    aux = 1;
                    side = -1;
                }
            } else {
                // A host already stands — aux 2 = sustain beat (the
                // siege continues; the audit trail wants the pulse,
                // not silence between changes).
                aux = 2;
                side = ghost == GHOST_WILD ? -1 : ghost;
            }
            SimEvent e;
            e.kind = SimEvent::Kind::MythInvasion;
            e.tick = tick;
            e.param = static_cast<int>(s.region);
            e.aux = aux;
            e.side = side;
            out.push_back(e);
        }
        int claimant = ExclusiveSide(squads, s.region);
        // Ghost garrisons are myth-layer presence: ghosts alone hold
        // a shrine's ground for their side; flesh and spirit of
        // different banners contest it. GHOST_WILD contests ALL —
        // the gate below already reads 2 as no-claim, but wild
        // presence must also deny a flesh claimant.
        if (ghost == GHOST_WILD) {
            claimant = -1;
        } else if (ghost >= 0) {
            claimant = (claimant == -1) ? ghost
                     : (claimant == ghost) ? claimant
                                           : -1; // contested
        }
        // claimant is a raw squad.side — gate to the two-side domain
        // before it can index stance[] (Squads are publicly mutable;
        // a stray side must read as no-claim, never UB).
        if (claimant != 0 && claimant != 1) claimant = -1;
        if (claimant != s.claimant) {
            s.claimant = claimant;
            s.dwell = 0;
        }
        // Saturating — see the village dwell rationale; nothing
        // reads past the threshold and the counter can't overflow.
        if (s.claimant >= 0 && s.dwell < SHRINE_DEDICATION_TICKS) {
            ++s.dwell;
        }
        // Losing exclusive control frees the latch — recapture
        // re-dwells and re-emits (every dedication is a recorded
        // event; ledger pricing lands in DeedBook at Story 5.3).
        if (s.owner >= 0 && s.owner != s.claimant) {
            s.owner = -1;
        }
        if (s.owner < 0 && s.claimant >= 0 &&
            s.dwell >= SHRINE_DEDICATION_TICKS) {
            s.owner = s.claimant;
            // The god's mood flips with the ground: favorable to
            // the dedicatee, wrathful toward the despoiler.
            s.stance[s.owner] = 1;
            s.stance[1 - s.owner] = -1;
            SimEvent e;
            e.kind = SimEvent::Kind::ShrineCaptured;
            e.tick = tick;
            e.squadIndex = FirstAt(squads, s.owner, s.region);
            e.param = static_cast<int>(s.region);
            e.side = s.owner;
            out.push_back(e);
        }
    }
    return out;
}

const ShrineTrack* MythField::ShrineAt(std::size_t region) const {
    for (const ShrineTrack& s : shrines_) {
        if (s.region == region) return &s;
    }
    return nullptr;
}

ShrineTrack* MythField::ShrineAtMut(std::size_t region) {
    for (ShrineTrack& s : shrines_) {
        if (s.region == region) return &s;
    }
    return nullptr;
}

GodStance MythField::StanceAt(std::size_t region, int side) const {
    if (side != 0 && side != 1) return GodStance::Neutral;
    const ShrineTrack* s = ShrineAt(region);
    if (!s) return GodStance::Neutral;
    return static_cast<GodStance>(s->stance[side]);
}

} // namespace Potato::Gameplay
