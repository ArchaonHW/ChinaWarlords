#include "Gameplay/Governance/GovernanceField.h"

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Squad/Squad.h"

#include <utility>

namespace Potato::Gameplay {

namespace {

// Exclusive-presence scan: the side with >= 1 effective non-Routing
// squad at `region`, -1 when empty or contested. Routing squads are
// fleeing — they hold no ground (same reason commands reject them).
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

// Same path-shape contract as plan arrows: in-bounds,
// pairwise-adjacent, no revisits. Kept local — the field owns its
// own invariant rather than trusting the call site.
bool ValidConvoyPath(const BattleMap& map,
                     const std::vector<std::size_t>& path) {
    if (path.size() < 2 || path.size() > map.RegionCount()) {
        return false;
    }
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] >= map.RegionCount()) return false;
        for (std::size_t j = i + 1; j < path.size(); ++j) {
            if (path[i] == path[j]) return false;
        }
        if (i + 1 >= path.size()) break;
        bool adjacent = false;
        for (std::size_t n : map.Neighbors(path[i])) {
            if (n == path[i + 1]) { adjacent = true; break; }
        }
        if (!adjacent) return false;
    }
    return true;
}

} // namespace

void GovernanceField::Init(const BattleMap& map) {
    map_ = &map;
    villages_.clear();
    convoys_.clear();
    for (std::size_t r = 0; r < map.RegionCount(); ++r) {
        if (map.RegionAt(r).strategic & STRATEGIC_VILLAGE) {
            villages_.push_back(VillageTrack{r, -1, 0, -1, false});
        }
    }
}

bool GovernanceField::SpawnConvoy(int side,
                                  std::vector<std::size_t> path) {
    if (map_ == nullptr) return false;
    if (side != 0 && side != 1) return false;
    if (convoys_.size() >= MAX_CONVOYS) return false;
    if (!ValidConvoyPath(*map_, path)) return false;
    Convoy c;
    c.side = side;
    c.path = std::move(path);
    convoys_.push_back(std::move(c));
    return true;
}

std::optional<SimEvent> GovernanceField::TryBurn(std::size_t squadIndex,
                                                 const Squad& sq,
                                                 int tick) {
    if (map_ == nullptr) return std::nullopt;
    // The squad must still be standing on the village it targeted —
    // deltas apply in order, so an earlier Move this tick already
    // started it walking.
    if (!sq.IsEffective() || sq.state != SquadState::Holding) {
        return std::nullopt;
    }
    for (VillageTrack& v : villages_) {
        if (v.region != sq.regionIndex) continue;
        if (v.burned) return std::nullopt;
        v.burned = true;
        // Ash holds nothing — release any occupier latch so the
        // village can never read as held again.
        v.claimant = -1;
        v.dwell = 0;
        v.occupier = -1;
        SimEvent e;
        e.kind = SimEvent::Kind::VillageBurned;
        e.tick = tick;
        e.squadIndex = static_cast<int>(squadIndex);
        e.param = static_cast<int>(v.region);
        e.side = sq.side;
        return e;
    }
    return std::nullopt; // not standing on a village
}

int GovernanceField::FirstAt(const std::vector<Squad>& squads,
                             int side, std::size_t region) {
    for (std::size_t i = 0; i < squads.size(); ++i) {
        const Squad& s = squads[i];
        if (s.side == side && s.IsEffective() &&
            s.state != SquadState::Routing && s.regionIndex == region) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<SimEvent>
GovernanceField::Tick(const std::vector<Squad>& squads, int tick) {
    std::vector<SimEvent> out;
    if (map_ == nullptr) return out;

    // 1) Villages in map order: dwell -> latch -> emit.
    for (VillageTrack& v : villages_) {
        if (v.burned) continue; // ash holds nothing
        const int claimant = ExclusiveSide(squads, v.region);
        if (claimant != v.claimant) {
            v.claimant = claimant;
            v.dwell = 0;
        }
        // Saturating: nothing reads dwell past the threshold and an
        // unbounded battle (stalemate timer off) mustn't grow it into
        // signed overflow over geological tick counts.
        if (v.claimant >= 0 && v.dwell < OCCUPY_TICKS) ++v.dwell;
        // Losing exclusive control frees the latch — a recapture
        // re-dwells and re-emits (each capture is a deed).
        if (v.occupier >= 0 && v.occupier != v.claimant) {
            v.occupier = -1;
        }
        if (v.occupier < 0 && v.claimant >= 0 &&
            v.dwell >= OCCUPY_TICKS) {
            v.occupier = v.claimant;
            SimEvent e;
            e.kind = SimEvent::Kind::VillageOccupied;
            e.tick = tick;
            e.squadIndex = FirstAt(squads, v.claimant, v.region);
            e.param = static_cast<int>(v.region);
            e.side = v.claimant;
            out.push_back(e);
        }
    }

    // 2) Convoys in spawn order: march -> raid -> arrival.
    for (std::size_t ci = 0; ci < convoys_.size(); ++ci) {
        Convoy& c = convoys_[ci];
        if (!c.active) continue;
        if (++c.legProgress >= CONVOY_LEG_TICKS &&
            c.cursor + 1 < c.path.size()) {
            ++c.cursor;
            c.legProgress = 0;
        }
        const std::size_t region = c.path[c.cursor];
        // Raid is checked at the convoy's CURRENT node — including a
        // node it just reached, so a raider camped at the destination
        // catches it at the gate.
        const int raiderSide = 1 - c.side;
        const int raider = FirstAt(squads, raiderSide, region);
        const int guard = FirstAt(squads, c.side, region);
        if (raider >= 0 && guard < 0) {
            c.active = false;
            SimEvent e;
            e.kind = SimEvent::Kind::ConvoyRaided;
            e.tick = tick;
            e.squadIndex = raider;
            e.param = static_cast<int>(region);
            e.aux = static_cast<int>(ci);
            e.side = raiderSide;
            out.push_back(e);
            continue;
        }
        if (c.cursor + 1 == c.path.size()) {
            c.active = false;
            SimEvent e;
            e.kind = SimEvent::Kind::ConvoyArrived;
            e.tick = tick;
            // squadIndex = the escort of record: a friendly squad
            // standing at the destination on arrival (-1 = the
            // convoy slipped through unescorted).
            e.squadIndex = guard;
            e.param = static_cast<int>(region);
            e.aux = static_cast<int>(ci);
            e.side = c.side;
            out.push_back(e);
        }
    }
    return out;
}

} // namespace Potato::Gameplay
