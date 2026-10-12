#include "Gameplay/AI/BattleAI.h"

#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Squad/Squad.h"

#include <algorithm>
#include <queue>
#include <utility>

namespace Potato::Gameplay {

namespace {

// BFS distance in hops; -1 = unreachable.
int HopDistance(const BattleMap& map, std::size_t from, std::size_t to) {
    if (from == to) return 0;
    if (from >= map.RegionCount() || to >= map.RegionCount()) return -1;
    std::vector<int> dist(map.RegionCount(), -1);
    std::queue<std::size_t> q;
    dist[from] = 0;
    q.push(from);
    while (!q.empty()) {
        const std::size_t cur = q.front();
        q.pop();
        for (std::size_t n : map.Neighbors(cur)) {
            if (dist[n] != -1) continue;
            dist[n] = dist[cur] + 1;
            if (n == to) return dist[n];
            q.push(n);
        }
    }
    return -1;
}

} // namespace

BattleAI::BattleAI(std::uint64_t seed, int side, Prior prior,
                   const BattleMap& map, const DoctrineLibrary& cards)
    : side_(side), prior_(prior), map_(map), cards_(cards),
      rng_(seed ^ (0x9E3779B97F4A7C15ull * static_cast<std::uint64_t>(
                      side + 1))),
      probed_(map.RegionCount(), 0) {}

int BattleAI::ScoreCard(const DoctrineCard& c, Prior p) {
    int s = 0;
    switch (p) {
    case Prior::Aggressive:
        if (c.modifier == ModifierKind::AttackBoost) s += 3;
        if (c.action == ActionKind::Move) s += 2;
        if (c.trigger == TriggerKind::EnemyAdjacent) s += 2;
        if (c.action == ActionKind::Retreat) s -= 3;
        break;
    case Prior::Defensive:
        if (c.action == ActionKind::Brace) s += 3;
        if (c.trigger == TriggerKind::CohesionBelow) s += 2;
        if (c.action == ActionKind::Hold) s += 1;
        if (c.action == ActionKind::Move) s -= 2;
        break;
    case Prior::Cunning:
        if (c.trigger == TriggerKind::EnemyInRegion) s += 3; // ambush
        if (c.condition == ConditionKind::CpAtLeast) s += 1;
        if (c.action == ActionKind::Hold) s += 1;
        if (c.action == ActionKind::Retreat) s -= 1;
        break;
    }
    return s;
}

std::vector<std::size_t> BattleAI::ShortestPath(const BattleMap& map,
                                              std::size_t from,
                                              std::size_t to) {
    if (from >= map.RegionCount() || to >= map.RegionCount()) return {};
    if (from == to) return {from};
    std::vector<std::size_t> parent(map.RegionCount(), ~std::size_t{0});
    std::queue<std::size_t> q;
    q.push(from);
    parent[from] = from;
    while (!q.empty()) {
        const std::size_t cur = q.front();
        q.pop();
        for (std::size_t n : map.Neighbors(cur)) {
            if (parent[n] != ~std::size_t{0}) continue;
            parent[n] = cur;
            if (n == to) {
                std::vector<std::size_t> path;
                for (std::size_t at = to;; at = parent[at]) {
                    path.push_back(at);
                    if (at == from) break;
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
            q.push(n);
        }
    }
    return {};
}

bool BattleAI::Plan(BattleController& bc,
                    const std::vector<SquadTemplate>& troops,
                    const std::vector<std::size_t>& startRegions,
                    const std::vector<std::string>& cardPool,
                    std::size_t objective) {
    if (bc.Beat() != BattleBeat::Planning) return false;
    if (side_ != 0 && side_ != 1) return false;
    if (troops.empty() || startRegions.empty()) return false;
    if (objective >= map_.RegionCount()) return false;
    for (std::size_t r : startRegions) {
        if (r >= map_.RegionCount()) return false;
    }
    objective_ = objective;
    probed_.assign(map_.RegionCount(), 0); // reset on reuse
    entangleTick_ = -1;

    // 1) Deployment order by prior (stable, canonical ties).
    std::vector<std::size_t> order(startRegions.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    const bool closest = prior_ != Prior::Defensive;
    // Unreachable regions (-1) sort LAST under "closest" — they make
    // poor vanguards; defensive already prefers far regions anyway.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a,
                                                     std::size_t b) {
        int da = HopDistance(map_, startRegions[a], objective);
        int db = HopDistance(map_, startRegions[b], objective);
        if (da < 0) da = 1 << 20;
        if (db < 0) db = 1 << 20;
        return closest ? da < db : da > db;
    });

    // 2) Prior-scored sheet from the card pool (top MIN_SLOTS,
    //    ties by pool order — canonical). Remember preferred slots.
    std::vector<std::size_t> ranked(cardPool.size());
    for (std::size_t i = 0; i < ranked.size(); ++i) ranked[i] = i;
    std::stable_sort(ranked.begin(), ranked.end(),
                     [&](std::size_t a, std::size_t b) {
                         const DoctrineCard* ca =
                             cards_.Find(cardPool[a]);
                         const DoctrineCard* cb =
                             cards_.Find(cardPool[b]);
                         const int sa = ca ? ScoreCard(*ca, prior_) : -100;
                         const int sb = cb ? ScoreCard(*cb, prior_) : -100;
                         return sa > sb;
                     });
    // Drop unresolvable ids BEFORE truncation — one bogus id would
    // otherwise fail SquadSheet::Build and zero out every sheet even
    // when >= MIN_SLOTS valid cards exist.
    std::vector<std::size_t> valid;
    for (std::size_t i : ranked) {
        if (cards_.Find(cardPool[i]) != nullptr) valid.push_back(i);
    }
    if (valid.size() > SquadSheet::MAX_SLOTS)
        valid.resize(SquadSheet::MAX_SLOTS);
    std::vector<std::string> sheetIds;
    for (std::size_t i : valid) sheetIds.push_back(cardPool[i]);
    auto sheetRes = SquadSheet::Build(cards_, sheetIds);
    overrideSlot_ = braceSlot_ = -1;
    if (sheetRes.ok()) {
        for (std::size_t s = 0; s < sheetRes.value.slots.size(); ++s) {
            const DoctrineCard& c =
                cards_.At(sheetRes.value.slots[s].cardIndex);
            if (overrideSlot_ < 0 &&
                (c.modifier == ModifierKind::AttackBoost ||
                 c.action == ActionKind::Move))
                overrideSlot_ = static_cast<int>(s);
            if (braceSlot_ < 0 && c.action == ActionKind::Brace)
                braceSlot_ = static_cast<int>(s);
        }
    }

    // 3) Deploy + arrows + feints. Squad indices are return-order
    //    dense: track them for sheet/feint addressing.
    const int enemySide = side_ == 0 ? 1 : 0;
    const std::size_t squadBase = bc.Squads().size();
    for (std::size_t t = 0; t < troops.size(); ++t) {
        const std::size_t region =
            startRegions[order[t % order.size()]];
        if (!bc.DeploySquad(troops[t], region, side_)) return false;
        const std::size_t idx = squadBase + t;
        if (sheetRes.ok()) bc.SetSheet(idx, sheetRes.value);

        // Arrow: aggressive/cunning march the full BFS path;
        // defensive takes only the first hop (cautious).
        std::vector<std::size_t> path =
            ShortestPath(map_, region, objective);
        if (prior_ == Prior::Defensive && path.size() > 2)
            path.resize(2);
        if (path.size() >= 2) bc.DrawArrow(side_, static_cast<int>(idx), path);

        // Cunning feint: plant a confident false signature in the
        // enemy's fog — believedRegion = a neighbor NOT on the arrow.
        if (prior_ == Prior::Cunning) {
            for (std::size_t n : map_.Neighbors(region)) {
                bool onPath = false;
                for (std::size_t p : path)
                    if (p == n) { onPath = true; break; }
                if (!onPath) {
                    bc.SetCloudIntel(enemySide,
                                     static_cast<int>(idx),
                                     static_cast<int>(n), 70);
                    break;
                }
            }
        }
    }
    return true;
}

void BattleAI::Act(BattleController& bc) {
    if (bc.Beat() != BattleBeat::Execution) return;
    const QuantumFog& fog = bc.Fog(side_);

    // Cunning side-level intel ops run once per call, before the
    // per-squad pass — probes and entanglements aren't squad-bound.
    if (prior_ == Prior::Cunning) {
        // Probe: buy certainty where the field is weakest (once each,
        // budget-gated by IssueProbe itself).
        const int thresh = fog.Config().detectThreshold;
        for (std::size_t r = 0; r < probed_.size(); ++r) {
            if (!probed_[r] && fog.CertaintyAt(r) < thresh) {
                if (bc.IssueProbe(side_, r).ok()) probed_[r] = 1;
                else break; // budget exhausted or out of phase
            }
        }
        // Entangle the two lowest-certainty live clouds. Select by
        // POSITION but submit by ID — ids stay dense only until the
        // first RemoveCloud (order-preserving erase leaves them
        // sparse), so clouds_[id] is NOT a valid lookup after that.
        if (bc.CpPool(side_) >= CostOf(InterventionKind::Entangle)) {
            int ia = -1, ib = -1; // positional indices
            const auto& clouds = fog.Clouds();
            for (int i = 0; i < static_cast<int>(clouds.size()); ++i) {
                const CloudEntity& e = clouds[i];
                if (e.entangledWith >= 0) continue;
                if (ia < 0 || e.certainty < clouds[ia].certainty) {
                    ib = ia;
                    ia = i;
                } else if (ib < 0 ||
                           e.certainty < clouds[ib].certainty) {
                    ib = i;
                }
            }
                if (ia >= 0 && ib >= 0 &&
                entangleTick_ != static_cast<int>(
                                     bc.GetSim().TickCount())) {
                if (bc.IssueEntangle(side_, clouds[ia].id,
                                     clouds[ib].id)
                        .ok())
                    entangleTick_ =
                        static_cast<int>(bc.GetSim().TickCount());
            }
        }
    }

    // Canonical order: squad index; one command per squad per call —
    // the dup-guard makes a second attempt a wasted rejection anyway.
    const std::vector<Squad>& squads = bc.Squads();
    for (std::size_t i = 0; i < squads.size(); ++i) {
        const Squad& sq = squads[i];
        // TRUTH BOUNDARY: own-side squad fields only. Enemy positions
        // come exclusively from Fog(side_) belief.
        if (sq.side != side_ || !sq.IsEffective() ||
            sq.state == SquadState::Routing)
            continue;
        ActOnSquad(bc, i);
    }
}

void BattleAI::ActOnSquad(BattleController& bc, std::size_t index) {
    const Squad& sq = bc.Squads()[index];
    const QuantumFog& fog = bc.Fog(side_);
    const int cp = bc.CpPool(side_);
    const std::size_t at = sq.regionIndex;

    switch (prior_) {
    case Prior::Aggressive:
        // Push toward believed enemy contact; else force the punch card.
        if (sq.state == SquadState::Holding &&
            at != BattleMap::NO_REGION &&
            cp >= CostOf(InterventionKind::Redirect)) {
            for (std::size_t n : map_.Neighbors(at)) {
                if (fog.VisibleAt(n) > 0) {
                    bc.IssueRedirect(side_, static_cast<int>(index), n);
                    return;
                }
            }
        }
        if (overrideSlot_ >= 0 &&
            cp >= CostOf(InterventionKind::Override)) {
            bc.IssueOverride(side_, static_cast<int>(index),
                             overrideSlot_);
        }
        break;
    case Prior::Defensive:
        // Withdraws earlier than the rout threshold ever forces.
        if (sq.cohesion < 45 && cp >= CostOf(InterventionKind::Retreat)) {
            bc.IssueRetreat(side_, static_cast<int>(index));
            return;
        }
        if (braceSlot_ >= 0 && cp >= CostOf(InterventionKind::Override) &&
            fog.VisibleAt(at) > 0) {
            bc.IssueOverride(side_, static_cast<int>(index),
                             braceSlot_);
        }
        break;
    case Prior::Cunning:
        // Replan squads standing off their arrow (anchor semantics:
        // the new path contains the squad's position by construction).
        if (sq.state == SquadState::Holding &&
            at != BattleMap::NO_REGION) {
            const PlanArrow& a = bc.ArrowOf(index);
            if (a.active) {
                bool onPath = false;
                for (std::size_t p : a.path)
                    if (p == at) { onPath = true; break; }
                if (!onPath) {
                    auto path = ShortestPath(map_, at, objective_);
                    // size<2 means we're standing on the objective —
                    // a 1-node arrow buys nothing for 2 CP.
                    if (path.size() >= 2) {
                        bc.IssueReplan(side_, static_cast<int>(index),
                                       std::move(path));
                    }
                }
            }
        }
        break;
    }
}

} // namespace Potato::Gameplay
