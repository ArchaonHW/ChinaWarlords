#include "Gameplay/Sim/BattleController.h"

#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Squad/Squad.h"

#include <climits>
#include <utility>

namespace Potato::Gameplay {

namespace {

// Numeric fold — deterministic mix of a 64-bit value into an accumulator.
std::uint64_t Fold(std::uint64_t acc, std::uint64_t v) {
    acc ^= v + 0x9E3779B97F4A7C15ull + (acc << 6) + (acc >> 2);
    return acc;
}

// Normalize width-dependent sentinels (NO_REGION is ~size_t{0}) so the
// checksum is identical on 32- and 64-bit targets.
std::uint64_t RegionKey(std::size_t r) {
    return r == Squad::NO_REGION ? ~std::uint64_t{0}
                                 : static_cast<std::uint64_t>(r);
}

} // namespace

BattleController::BattleController(std::uint64_t seed, const BattleMap& map,
                                   const DoctrineLibrary& cards,
                                   const FogConfig& fogConfig,
                                   const PlanConfig& planConfig,
                                   const EvalConfig& evalConfig)
    : map_(map), cards_(cards), sim_(seed),
      fog_{QuantumFog(map, fogConfig), QuantumFog(map, fogConfig)},
      planConfig_(planConfig), evalConfig_(evalConfig) {
    field_.Init(map);
    myth_.Init(map);
}

BattleController::~BattleController() = default;
BattleController::BattleController(const BattleController&) = default;

void BattleController::EmitBeatChanged(int tick, BattleBeat target) {
    events_.push_back({SimEvent::Kind::BeatChanged, tick, -1, -1, -1,
                       static_cast<int>(target), -1, {}, {}});
}

bool BattleController::RequestBeat(BattleBeat target) {
    if (!CanTransition(beat_, target)) return false;
    // BeatChanged.tick = index of the tick the transition followed:
    // P->E fires before any tick (0); the auto-close path inside Tick()
    // stamps the producing tick via EmitBeatChanged directly.
    beat_ = target;
    // BeatChanged is emitted by CloseBattle for the Aftermath
    // transition so BeatChanged -> ResultDeclared adjacency is
    // structural, not caller discipline.
    if (target != BattleBeat::Aftermath) {
        EmitBeatChanged(static_cast<int>(sim_.TickCount()), target);
    }
    if (target == BattleBeat::Execution) {
        // Deployment footprint IS initial intel: run one observation
        // pass before bonuses so arrows over covered ground score.
        SyncFog();
        ApplyPlanBonuses();
    }
    if (target == BattleBeat::Aftermath) {
        CloseBattle(CloseReason::Concede,
                    static_cast<int>(sim_.TickCount()));
    }
    return true;
}

bool BattleController::DeploySquad(const SquadTemplate& t,
                                   std::size_t regionIndex, int side) {
    if (beat_ != BattleBeat::Planning) return false;
    if (regionIndex >= map_.RegionCount()) return false; // incl. NO_REGION
    if (side != 0 && side != 1) return false;            // invisible faction
    squads_.push_back(Squad::Instantiate(t, regionIndex, side));
    sheets_.push_back(SquadSheet{}); // empty sheet = no doctrine
    routTimers_.push_back(0);
    // Intel: the opposing side's fog gains a cloud for this squad at
    // initial certainty; own fog tracks nothing (own truth is visible).
    cloudOf_[side].push_back(-1);
    const int enemyFog = 1 - side;
    cloudOf_[enemyFog].push_back(
        fog_[enemyFog].AddCloud(static_cast<int>(regionIndex),
                                fog_[enemyFog].Config().initialIntel));
    arrows_.push_back(PlanArrow{});
    return true;
}

bool BattleController::SetSheet(std::size_t squadIndex, SquadSheet sheet) {
    if (beat_ != BattleBeat::Planning) return false;
    if (squadIndex >= squads_.size()) return false;
    sheets_[squadIndex] = std::move(sheet);
    return true;
}

bool BattleController::SetCpPool(int side, int cp) {
    if (beat_ != BattleBeat::Planning) return false;
    if (side != 0 && side != 1) return false;
    cpPool_[side] = cp < 0 ? 0 : (cp > CP_CAP ? CP_CAP : cp);
    return true;
}

// --- CP interventions ------------------------------------------------

namespace {

Result<bool> CheckCommand(const BattleBeat beat, const int side,
                          const std::vector<Squad>& squads,
                          const std::vector<Intervention>& pending,
                          const int squadIndex, const int kindCost,
                          const int cpPool) {
    if (beat != BattleBeat::Execution) {
        return Fail<bool>("command", "interventions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("command", "side must be 0 or 1");
    }
    if (cpPool < kindCost) {
        return Fail<bool>("command", "insufficient CP");
    }
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads.size()) {
        return Fail<bool>("command", "invalid squad index");
    }
    const Squad& sq = squads[static_cast<std::size_t>(squadIndex)];
    if (sq.side != side) {
        return Fail<bool>("command", "cannot command the other side");
    }
    if (!sq.IsEffective()) {
        return Fail<bool>("command", "squad is off-field");
    }
    // Routing is "effective" (still on-field) but accepts no commands —
    // otherwise the CP is spent on a guaranteed apply-time no-op.
    if (sq.state == SquadState::Routing) {
        return Fail<bool>("command", "squad is routing off-field");
    }
    // One pending command per squad — a second would be a paid no-op
    // at apply (e.g. IssueMove is Holding-gated, forceNext is a bool).
    // Fog ops skip the check: Entangle repurposes squadIndex as a
    // cloud id (dense 0..n-1 — would falsely collide with squad
    // indices); Probe stores -1 anyway.
    for (const Intervention& c : pending) {
        if (c.kind == InterventionKind::Probe ||
            c.kind == InterventionKind::Entangle) continue;
        if (c.squadIndex == squadIndex) {
            return Fail<bool>("command",
                              "squad already has a pending command");
        }
    }
    return Ok(true);
}

} // namespace

Result<bool> BattleController::IssueRedirect(int side, int squadIndex,
                                             std::size_t region) {
    auto chk = CheckCommand(beat_, side, squads_, pendingCommands_,
                            squadIndex, CostOf(InterventionKind::Redirect),
                            cpPool_[side == 1 ? 1 : 0]);
    if (!chk.ok()) return chk;
    const Squad& sq = squads_[static_cast<std::size_t>(squadIndex)];
    if (sq.state != SquadState::Holding) {
        return Fail<bool>("command", "squad is not Holding");
    }
    if (region >= map_.RegionCount()) {
        return Fail<bool>("command", "target region out of bounds");
    }
    if (sq.regionIndex >= map_.RegionCount()) {
        return Fail<bool>("command", "squad region out of bounds");
    }
    bool adjacent = false;
    for (std::size_t n : map_.Neighbors(sq.regionIndex)) {
        if (n == region) { adjacent = true; break; }
    }
    if (!adjacent) {
        return Fail<bool>("command", "target not adjacent to squad");
    }
    cpPool_[side] -= CostOf(InterventionKind::Redirect);
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Redirect,
                                squadIndex, static_cast<int>(region), tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       static_cast<int>(region),
                       static_cast<int>(InterventionKind::Redirect),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueOverride(int side, int squadIndex,
                                             int slotIndex) {
    auto chk = CheckCommand(beat_, side, squads_, pendingCommands_,
                            squadIndex, CostOf(InterventionKind::Override),
                            cpPool_[side == 1 ? 1 : 0]);
    if (!chk.ok()) return chk;
    if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >=
            sheets_[static_cast<std::size_t>(squadIndex)].slots.size()) {
        return Fail<bool>("command", "invalid slot index");
    }
    cpPool_[side] -= CostOf(InterventionKind::Override);
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Override,
                                squadIndex, slotIndex, tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       slotIndex,
                       static_cast<int>(InterventionKind::Override),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueRetreat(int side, int squadIndex) {
    auto chk = CheckCommand(beat_, side, squads_, pendingCommands_,
                            squadIndex, CostOf(InterventionKind::Retreat),
                            cpPool_[side == 1 ? 1 : 0]);
    if (!chk.ok()) return chk;
    cpPool_[side] -= CostOf(InterventionKind::Retreat);
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Retreat,
                                squadIndex, -1, tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       -1,
                       static_cast<int>(InterventionKind::Retreat),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueProbe(int side, std::size_t region) {
    if (beat_ != BattleBeat::Execution) {
        return Fail<bool>("command", "interventions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("command", "side must be 0 or 1");
    }
    if (region >= map_.RegionCount()) {
        return Fail<bool>("command", "target region out of bounds");
    }
    if (probeBudget_[side] <= 0) {
        return Fail<bool>("command", "probe budget exhausted");
    }
    --probeBudget_[side];
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Probe, -1,
                                static_cast<int>(region), tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, -1, -1,
                       static_cast<int>(region),
                       static_cast<int>(InterventionKind::Probe),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueEntangle(int side, int cloudIdA,
                                             int cloudIdB) {
    if (beat_ != BattleBeat::Execution) {
        return Fail<bool>("command", "interventions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("command", "side must be 0 or 1");
    }
    if (cpPool_[side] < CostOf(InterventionKind::Entangle)) {
        return Fail<bool>("command", "insufficient CP");
    }
    if (cloudIdA == cloudIdB || !fog_[side].HasCloud(cloudIdA) ||
        !fog_[side].HasCloud(cloudIdB)) {
        return Fail<bool>("command", "invalid cloud pair");
    }
    // Clouds of already-off-field squads are a paid no-op (pruned at
    // the next SyncFog) — check the truth map first.
    const auto liveCloud = [&](int cid) {
        for (std::size_t i = 0; i < cloudOf_[side].size(); ++i) {
            if (cloudOf_[side][i] == cid) return squads_[i].IsEffective();
        }
        return false;
    };
    if (!liveCloud(cloudIdA) || !liveCloud(cloudIdB)) {
        return Fail<bool>("command", "cloud target is off-field");
    }
    if (fog_[side].EntangledWith(cloudIdA) >= 0 ||
        fog_[side].EntangledWith(cloudIdB) >= 0) {
        return Fail<bool>("command", "cloud already entangled");
    }
    cpPool_[side] -= CostOf(InterventionKind::Entangle);
    const int tick = static_cast<int>(sim_.TickCount());
    // squadIndex field carries cloud A's id (see Intervention comment);
    // the event mirrors the same layout so the log records both ids.
    pendingCommands_.push_back({side, InterventionKind::Entangle,
                                cloudIdA, cloudIdB, tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, cloudIdA, -1,
                       cloudIdB,
                       static_cast<int>(InterventionKind::Entangle),
                       side, {}, {}});
    return Ok(true);
}

bool BattleController::SetCloudIntel(int fogSide, int squadIndex,
                                     int region, int certainty) {
    if (beat_ != BattleBeat::Planning) return false;
    if (fogSide != 0 && fogSide != 1) return false;
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads_.size()) {
        return false;
    }
    if (region < 0 || static_cast<std::size_t>(region) >= map_.RegionCount()) {
        return false;
    }
    const int cid = cloudOf_[fogSide][static_cast<std::size_t>(squadIndex)];
    if (cid < 0) return false; // can't brief about your own squads
    fog_[fogSide].SetCloudBelief(cid, region, certainty);
    return true;
}

// --- BattlePlan ------------------------------------------------------------

namespace {

// Validate a drawn path shape: in-bounds, pairwise-adjacent, and no
// region revisits (cycles would let authors inflate MeanCertainty and
// make cursor-seek ambiguous — also bounds length to RegionCount).
bool ValidPathShape(const BattleMap& map,
                    const std::vector<std::size_t>& path) {
    if (path.empty() || path.size() > map.RegionCount()) return false;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] >= map.RegionCount()) return false;
        for (std::size_t j = i + 1; j < path.size(); ++j) {
            if (path[i] == path[j]) return false; // revisit
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

// Where the squad will next be stationary: mid-transit that's the
// edge destination, otherwise its current region.
std::size_t AnchorRegion(const Squad& sq) {
    return sq.state == SquadState::Moving ? sq.edgeTarget
                                          : sq.regionIndex;
}

// Find `region` in `path`; returns index or path.size().
std::size_t SeekPath(const std::vector<std::size_t>& path,
                     std::size_t region) {
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == region) return i;
    }
    return path.size();
}

} // namespace

bool BattleController::DrawArrow(int side, int squadIndex,
                                 const std::vector<std::size_t>& path) {
    if (beat_ != BattleBeat::Planning) return false;
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads_.size()) return false;
    const Squad& sq = squads_[static_cast<std::size_t>(squadIndex)];
    if (sq.side != side) return false;
    // Planning squads stand still: anchor is the current region and the
    // arrow must start there (path >= 2 nodes to mean anything).
    if (path.size() < 2 || !ValidPathShape(map_, path) ||
        path[0] != sq.regionIndex) return false;
    PlanArrow& a = arrows_[static_cast<std::size_t>(squadIndex)];
    a.active = true;
    a.path = path;
    a.cursor = 0;
    a.bonusApplied = 0;
    return true;
}

const PlanArrow& BattleController::ArrowOf(std::size_t squadIndex) const {
    static const PlanArrow EMPTY{};
    return squadIndex < arrows_.size() ? arrows_[squadIndex] : EMPTY;
}

bool BattleController::SpawnConvoy(int side,
                                   std::vector<std::size_t> path) {
    if (beat_ != BattleBeat::Planning) return false;
    return field_.SpawnConvoy(side, std::move(path));
}

bool BattleController::SeedInfiltration(std::size_t region,
                                        int level) {
    if (beat_ != BattleBeat::Planning) return false;
    InfiltrationLevel l;
    if (!InfiltrationLevelFromInt(level, l)) return false;
    return myth_.Seed(region, l);
}

bool BattleController::ApplyMythEvent(std::size_t region,
                                      MythEventKind kind) {
    if (beat_ != BattleBeat::Planning) return false;
    // Tick 0 — Planning precedes the tick counter's domain. A
    // saturating no-op (or invalid input) rejects like any other
    // failed verb: journaled ops must have an observable effect, so
    // a forged no-op can never ride a valid record.
    auto ev = myth_.Apply(region, kind, 0);
    if (!ev) return false;
    events_.push_back(*ev);
    return true;
}

void BattleController::ApplyPlanBonuses() {
    for (std::size_t i = 0; i < squads_.size(); ++i) {
        PlanArrow& a = arrows_[i];
        if (!a.active) continue;
        Squad& sq = squads_[i];
        // Squads already streaming off-field never march — no bonus.
        if (sq.state == SquadState::Routing) continue;
        const int mean = MeanCertainty(fog_[sq.side], a.path);
        const int bonus = mean < planConfig_.bonusPercentCap
                              ? mean : planConfig_.bonusPercentCap;
        // int64 intermediate — attack is clamped to INT_MAX by doctrine
        // deltas, so attack*bonus can overflow int32 (UB in tick path).
        std::int64_t grant = static_cast<std::int64_t>(sq.attack) *
                                 bonus / 100;
        std::int64_t next = static_cast<std::int64_t>(sq.attack) + grant;
        if (next > INT_MAX) next = INT_MAX;
        a.bonusApplied = static_cast<int>(next - sq.attack);
        sq.attack = static_cast<int>(next);
    }
}

Result<bool> BattleController::IssueReplan(int side, int squadIndex,
                                           std::vector<std::size_t> path) {
    auto chk = CheckCommand(beat_, side, squads_, pendingCommands_,
                            squadIndex, planConfig_.replanCost,
                            cpPool_[side == 1 ? 1 : 0]);
    if (!chk.ok()) return chk;
    // Issue-time path validation: the path must be a valid chain AND
    // contain the squad's anchor (edgeTarget mid-transit, else the
    // current region) — an arrow that never visits where the squad is
    // going can only stall.
    const Squad& sq = squads_[static_cast<std::size_t>(squadIndex)];
    if (!ValidPathShape(map_, path) ||
        SeekPath(path, AnchorRegion(sq)) == path.size()) {
        return Fail<bool>("command", "invalid replan path");
    }
    cpPool_[side] -= planConfig_.replanCost;
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Replan,
                                squadIndex,
                                static_cast<int>(path.size()), tick,
                                std::move(path)});
    // `path` moved into the queued command; the event copies it back —
    // the recorded stream must carry the full payload for replay (1.9
    // deferral closed: path, not just length).
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       static_cast<int>(pendingCommands_.back().path.size()),
                       static_cast<int>(InterventionKind::Replan),
                       side, pendingCommands_.back().path, {}});
    return Ok(true);
}

namespace {

// A coherent squad of `side` STANDING in `region` — the proximity
// precondition for refusing a surrender there, and the reach
// precondition for turning one (Epic 7): an envoy must stand on the
// ground it persuades. Holding only: a squad mid-leg has already
// left (or hasn't arrived); routing ones are fleeing, not envoys.
bool EnvoyPresent(const std::vector<Squad>& squads, int side,
                  std::size_t region) {
    for (const Squad& s : squads) {
        if (s.side == side && s.state == SquadState::Holding &&
            s.regionIndex == region) {
            return true;
        }
    }
    return false;
}

} // namespace

Result<bool> BattleController::IssueExecute(int side, int squadIndex) {
    // CheckCommand can't carry this shape: the target is the OTHER
    // side's squad and Routing is the required (not forbidden) state.
    if (beat_ != BattleBeat::Execution) {
        return Fail<bool>("command", "interventions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("command", "side must be 0 or 1");
    }
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads_.size()) {
        return Fail<bool>("command", "invalid squad index");
    }
    const Squad& vic = squads_[static_cast<std::size_t>(squadIndex)];
    if (vic.side == side) {
        return Fail<bool>("command", "cannot execute your own squad");
    }
    if (vic.state != SquadState::Routing) {
        return Fail<bool>("command", "victim is not routing");
    }
    if (!EnvoyPresent(squads_, side, vic.regionIndex)) {
        return Fail<bool>("command", "no squad in reach to refuse");
    }
    for (const Intervention& c : pendingCommands_) {
        if (c.kind == InterventionKind::Execute &&
            c.squadIndex == squadIndex) {
            return Fail<bool>("command", "victim already marked");
        }
    }
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Execute,
                                squadIndex,
                                static_cast<int>(vic.regionIndex), tick,
                                {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       static_cast<int>(vic.regionIndex),
                       static_cast<int>(InterventionKind::Execute),
                       side, {}, {}});
    return Ok(true);
}

// Shared preamble for the myth-action verbs: Execution gate, side
// domain, pending-queue dedup by (kind, key) — the same target may
// be marked only once (journal contract: no stacked acts).
Result<bool> BattleController::CheckMythVerb(
    int side, InterventionKind kind, int key) const {
    if (beat_ != BattleBeat::Execution) {
        return Fail<bool>("myth", "myth actions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("myth", "side must be 0 or 1");
    }
    for (const Intervention& c : pendingCommands_) {
        if (c.kind == kind &&
            (kind == InterventionKind::MythPossession
                 ? c.squadIndex == key
                 : c.target == key)) {
            return Fail<bool>("myth", "target already marked");
        }
    }
    return Ok(true);
}

Result<bool> BattleController::IssueMythPacify(int side,
                                               std::size_t region) {
    auto chk = CheckMythVerb(side, InterventionKind::MythPacify,
                             static_cast<int>(region));
    if (!chk.ok()) return chk;
    if (region >= myth_.RegionCount()) {
        return Fail<bool>("myth", "region out of bounds");
    }
    // Nothing to pacify on quiet unconsecrated ground — rejected
    // pre-queue so a no-op can never ride the journal.
    const bool infiltrated =
        myth_.LevelAt(region) != InfiltrationLevel::None;
    const ShrineTrack* sh = myth_.ShrineAt(region);
    const bool consecrated = sh != nullptr && sh->owner >= 0;
    if (!infiltrated && !consecrated) {
        return Fail<bool>("myth", "nothing to pacify here");
    }
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::MythPacify, -1,
                                static_cast<int>(region), tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, -1, -1,
                       static_cast<int>(region),
                       static_cast<int>(InterventionKind::MythPacify),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueMythPossession(int side,
                                                   int squadIndex) {
    auto chk = CheckMythVerb(side, InterventionKind::MythPossession,
                             squadIndex);
    if (!chk.ok()) return chk;
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads_.size()) {
        return Fail<bool>("myth", "invalid squad index");
    }
    const Squad& sq = squads_[static_cast<std::size_t>(squadIndex)];
    if (sq.side != side) {
        return Fail<bool>("myth", "a god rides only its own host");
    }
    if (!sq.IsEffective() || sq.state == SquadState::Routing) {
        return Fail<bool>("myth", "the host must still stand");
    }
    if (sq.possessed) {
        return Fail<bool>("myth", "already possessed");
    }
    // GodStance gate (Story 5.7): a wrathful deity refuses the host —
    // the god does not ride the army it hates.
    if (sq.regionIndex != Squad::NO_REGION &&
        myth_.StanceAt(sq.regionIndex, side) == GodStance::Wrathful) {
        return Fail<bool>("myth", "the god refuses this ground");
    }
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::MythPossession,
                                squadIndex,
                                static_cast<int>(sq.regionIndex), tick,
                                {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       static_cast<int>(sq.regionIndex),
                       static_cast<int>(InterventionKind::MythPossession),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueGhostArmy(int side,
                                              std::size_t region) {
    auto chk = CheckMythVerb(side, InterventionKind::MythGhostArmy,
                             static_cast<int>(region));
    if (!chk.ok()) return chk;
    if (region >= myth_.RegionCount()) {
        return Fail<bool>("myth", "region out of bounds");
    }
    // The veil must be thin — ghosts don't rise on quiet land; and
    // a region holds one garrison (RaiseGhost re-checks at apply).
    if (myth_.LevelAt(region) == InfiltrationLevel::None) {
        return Fail<bool>("myth", "the land is too quiet to rise");
    }
    if (myth_.GhostAt(region) != -1) {
        return Fail<bool>("myth", "spirits already hold this ground");
    }
    // GodStance gate (Story 5.7): the god lends no dead to a side
    // he is wrathful toward.
    if (myth_.StanceAt(region, side) == GodStance::Wrathful) {
        return Fail<bool>("myth", "the god lends no dead here");
    }
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::MythGhostArmy,
                                -1, static_cast<int>(region), tick, {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, -1, -1,
                       static_cast<int>(region),
                       static_cast<int>(InterventionKind::MythGhostArmy),
                       side, {}, {}});
    return Ok(true);
}

Result<bool> BattleController::IssueSubvert(int side, int squadIndex) {
    // CheckCommand can't carry this shape either (same reason as
    // Execute): the target is the OTHER side's squad and Routing is
    // acceptable — a plea-in-flight is exactly who turns.
    if (beat_ != BattleBeat::Execution) {
        return Fail<bool>("command", "interventions only during Execution");
    }
    if (side != 0 && side != 1) {
        return Fail<bool>("command", "side must be 0 or 1");
    }
    if (squadIndex < 0 ||
        static_cast<std::size_t>(squadIndex) >= squads_.size()) {
        return Fail<bool>("command", "invalid squad index");
    }
    const Squad& vic = squads_[static_cast<std::size_t>(squadIndex)];
    if (vic.side == side) {
        return Fail<bool>("command", "cannot subvert your own squad");
    }
    if (!vic.IsEffective()) {
        return Fail<bool>("command", "target is off-field");
    }
    // Only cracks take the wedge — a coherent enemy squad shrugs the
    // envoy off. Routing counts: a plea-in-flight is turnable (and
    // beats Execute to it — queue order decides).
    if (vic.state != SquadState::Routing &&
        vic.cohesion >= SUBVERT_COHESION) {
        return Fail<bool>("command", "target is not wavering");
    }
    if (!EnvoyPresent(squads_, side, vic.regionIndex)) {
        return Fail<bool>("command", "no squad in reach to turn it");
    }
    for (const Intervention& c : pendingCommands_) {
        if (c.kind == InterventionKind::Subvert &&
            c.squadIndex == squadIndex) {
            return Fail<bool>("command", "target already marked");
        }
    }
    const int tick = static_cast<int>(sim_.TickCount());
    pendingCommands_.push_back({side, InterventionKind::Subvert,
                                squadIndex,
                                static_cast<int>(vic.regionIndex), tick,
                                {}});
    events_.push_back({SimEvent::Kind::Intervention, tick, squadIndex, -1,
                       static_cast<int>(vic.regionIndex),
                       static_cast<int>(InterventionKind::Subvert),
                       side, {}, {}});
    return Ok(true);
}

void BattleController::MarchArrows() {
    for (std::size_t i = 0; i < squads_.size(); ++i) {
        PlanArrow& a = arrows_[i];
        if (!a.active) continue;
        Squad& sq = squads_[i];
        if (sq.state != SquadState::Holding) continue;
        // Cursor resync: if the squad is standing on the path but not at
        // the cursor (e.g. a redirect landed it ahead), seek its region
        // and continue from there. Off-path squads stall the arrow
        // until replanned.
        if (a.cursor >= a.path.size() ||
            sq.regionIndex != a.path[a.cursor]) {
            a.cursor = SeekPath(a.path, sq.regionIndex);
            if (a.cursor == a.path.size()) continue; // off-path: stall
        }
        if (a.cursor + 1 >= a.path.size()) continue; // path complete
        if (sq.IssueMove(a.path[a.cursor + 1])) ++a.cursor;
    }
}

void BattleController::ApplyInterventions(int tick) {
    for (const Intervention& cmd : pendingCommands_) {
        const int side = (cmd.side == 1) ? 1 : 0;
        // Fog ops don't address squads — handle before the squad guard
        // (Entangle repurposes squadIndex as cloud id A).
        if (cmd.kind == InterventionKind::Probe) {
            if (cmd.target >= 0) {
                fog_[side].Probe(static_cast<std::size_t>(cmd.target));
            }
            continue;
        }
        if (cmd.kind == InterventionKind::Entangle) {
            fog_[side].Entangle(cmd.squadIndex, cmd.target);
            continue;
        }
        if (cmd.kind == InterventionKind::Execute) {
            // The victim IS the routing enemy — handle before the
            // squad guard (which would skip Routing). Re-gate at
            // apply: nothing can alter the victim between issue and
            // apply (rout aging runs later this tick), but the
            // REFUSER may have broken — an earlier-queued Retreat on
            // the refusing squad flips presence off, and queue order
            // decides it (Retreat-then-Execute no-ops; Execute-first
            // kills). That asymmetry is deterministic, authored
            // issue order — same rule as doctrine slots.
            if (cmd.squadIndex >= 0 &&
                static_cast<std::size_t>(cmd.squadIndex) <
                    squads_.size()) {
                Squad& vic =
                    squads_[static_cast<std::size_t>(cmd.squadIndex)];
                if (vic.state == SquadState::Routing &&
                    vic.side != side &&
                    EnvoyPresent(squads_, side, vic.regionIndex)) {
                    // The kill shot zeroes hp — a Destroyed corpse
                    // must not keep routing victims' full hp
                    // (ApplyHit is the only other path and zeroes).
                    vic.hp = 0;
                    if (!vic.ApplyEvent(SquadEvent::HpZero)) continue;
                    events_.push_back(
                        {SimEvent::Kind::SquadExecuted, tick,
                         cmd.squadIndex, -1,
                         static_cast<int>(vic.regionIndex), 0, side,
                         {}, {}});
                }
            }
            continue;
        }
        if (cmd.kind == InterventionKind::Subvert) {
            // The target IS the wavering enemy — handle before the
            // squad guard (which would skip Routing). Re-gated at
            // apply like Execute: queue order decides contested
            // marks (Subvert-first turns the squad Execute was
            // aimed at — mercy by presence).
            if (cmd.squadIndex >= 0 &&
                static_cast<std::size_t>(cmd.squadIndex) <
                    squads_.size()) {
                Squad& vic =
                    squads_[static_cast<std::size_t>(cmd.squadIndex)];
                if (vic.IsEffective() && vic.side != side &&
                    (vic.state == SquadState::Routing ||
                     vic.cohesion < SUBVERT_COHESION) &&
                    EnvoyPresent(squads_, side, vic.regionIndex)) {
                    // The banner flips, the squad stands down where
                    // it stands — a mid-leg move never completes.
                    vic.side = side;
                    vic.ApplyEvent(SquadEvent::Defected);
                    // Fog remap follows the flip: the persuader's
                    // fog drops the cloud (now its own truth); the
                    // betrayed side gains a cloud AT the witnessed
                    // spot — they watched it walk over, so the
                    // belief lands collapsed (certainty 100).
                    int& mine =
                        cloudOf_[side][static_cast<std::size_t>(
                            cmd.squadIndex)];
                    if (mine >= 0) {
                        fog_[side].RemoveCloud(mine);
                        mine = -1;
                    }
                    int& theirs =
                        cloudOf_[1 - side][static_cast<std::size_t>(
                            cmd.squadIndex)];
                    theirs = fog_[1 - side].AddCloud(
                        static_cast<int>(vic.regionIndex), 100);
                    events_.push_back(
                        {SimEvent::Kind::SquadDefected, tick,
                         cmd.squadIndex, -1,
                         static_cast<int>(vic.regionIndex), 0, side,
                         {}, {}});
                }
            }
            continue;
        }
        // Myth actions address regions or own squads — handle before
        // the squad guard (pacify/ghost have no squad). Apply-time
        // re-validation keeps replay honest: conditions may have
        // shifted between issue and apply (a queued Retreat can pull
        // the refuser; a prior pacify can drain the level).
        if (cmd.kind == InterventionKind::MythPacify) {
            if (cmd.target >= 0) {
                auto evs = myth_.PacifyRegion(
                    static_cast<std::size_t>(cmd.target), tick);
                if (evs.has_value()) {
                    events_.push_back({SimEvent::Kind::MythActionInvoked,
                                       tick, -1, -1, cmd.target,
                                       static_cast<int>(
                                           MythActionKind::PacifyShrine),
                                       side, {}, {}});
                    for (const SimEvent& e : *evs) {
                        events_.push_back(e);
                    }
                }
            }
            continue;
        }
        if (cmd.kind == InterventionKind::MythGhostArmy) {
            if (cmd.target >= 0 &&
                myth_.RaiseGhost(static_cast<std::size_t>(cmd.target),
                                 side)) {
                events_.push_back({SimEvent::Kind::MythActionInvoked,
                                   tick, -1, -1, cmd.target,
                                   static_cast<int>(
                                       MythActionKind::RaiseGhostArmy),
                                   side, {}, {}});
            }
            continue;
        }
        if (cmd.kind == InterventionKind::MythPossession) {
            // Own effective non-routing squad — re-gated at apply;
            // a host that broke between issue and apply releases
            // the god unspent (mercy by absence, same rule as
            // Execute's refuser check).
            if (cmd.squadIndex >= 0 &&
                static_cast<std::size_t>(cmd.squadIndex) <
                    squads_.size()) {
                Squad& host =
                    squads_[static_cast<std::size_t>(cmd.squadIndex)];
                if (host.side == side && host.IsEffective() &&
                    host.state != SquadState::Routing &&
                    !host.possessed) {
                    host.possessed = true;
                    // The war-god's blessing: sharper, steadier —
                    // saturating adds (fields are int; clamped).
                    host.attack = host.attack >= INT_MAX - 2
                                      ? INT_MAX
                                      : host.attack + 2;
                    host.RestoreCohesion(25);
                    events_.push_back({SimEvent::Kind::MythActionInvoked,
                                       tick, cmd.squadIndex, -1,
                                       static_cast<int>(host.regionIndex),
                                       static_cast<int>(
                                           MythActionKind::
                                               InvokePossession),
                                       side, {}, {}});
                }
            }
            continue;
        }
        if (cmd.squadIndex < 0 ||
            static_cast<std::size_t>(cmd.squadIndex) >= squads_.size())
            continue;
        Squad& sq = squads_[static_cast<std::size_t>(cmd.squadIndex)];
        // Routing accepts no commands either — a squad that broke
        // between issue and apply skips silently (CP already spent).
        if (!sq.IsEffective() || sq.state == SquadState::Routing) continue;
        switch (cmd.kind) {
            case InterventionKind::Redirect: {
                // Re-validate adjacency at apply — issue-time approval
                // stays valid only while the squad hasn't moved (the
                // pending-command guard allows one per squad, but keep
                // the gate for future command kinds that relocate).
                bool adjacent = false;
                const std::size_t target =
                    static_cast<std::size_t>(cmd.target);
                if (sq.regionIndex < map_.RegionCount() &&
                    target < map_.RegionCount()) {
                    for (std::size_t n : map_.Neighbors(sq.regionIndex)) {
                        if (n == target) { adjacent = true; break; }
                    }
                }
                if (adjacent) sq.IssueMove(target);
                break;
            }
            case InterventionKind::Override:
                if (cmd.target >= 0 && static_cast<std::size_t>(cmd.target) <
                        sheets_[static_cast<std::size_t>(cmd.squadIndex)]
                            .slots.size()) {
                    sheets_[static_cast<std::size_t>(cmd.squadIndex)]
                        .slots[static_cast<std::size_t>(cmd.target)]
                        .forceNext = true;
                }
                break;
            case InterventionKind::Retreat:
                // -> Routing; ApplyEvent clears in-flight edge state.
                sq.ApplyEvent(SquadEvent::CohesionBreak);
                break;
            case InterventionKind::Replan: {
                // Re-validate at apply: path shape plus the squad's
                // anchor must appear in the path — cursor seeks it, so
                // a squad mid-leg continues from where it will stand
                // (issue-time approval stays valid in the synchronous
                // API; this guards future async/replay paths).
                PlanArrow& a = arrows_[static_cast<std::size_t>(
                    cmd.squadIndex)];
                const std::size_t anchor =
                    SeekPath(cmd.path, AnchorRegion(sq));
                if (!ValidPathShape(map_, cmd.path) ||
                    anchor == cmd.path.size()) break;
                // Reverse the old grant (clamped — doctrine deltas may
                // have cut attack below the recorded bonus).
                if (a.bonusApplied > sq.attack) {
                    sq.attack = 0;
                } else {
                    sq.attack -= a.bonusApplied;
                }
                a.active = true;
                a.path = cmd.path;
                a.cursor = anchor;
                // Real-time scaling: bonus = current certainty mean.
                const int mean = MeanCertainty(fog_[side], a.path);
                const int bonus = mean < planConfig_.bonusPercentCap
                                      ? mean : planConfig_.bonusPercentCap;
                std::int64_t grant = static_cast<std::int64_t>(sq.attack) *
                                         bonus / 100;
                std::int64_t next =
                    static_cast<std::int64_t>(sq.attack) + grant;
                if (next > INT_MAX) next = INT_MAX;
                a.bonusApplied = static_cast<int>(next - sq.attack);
                sq.attack = static_cast<int>(next);
                break;
            }
            case InterventionKind::Probe:
            case InterventionKind::Entangle:
            case InterventionKind::Execute:
            case InterventionKind::MythPacify:
            case InterventionKind::MythPossession:
            case InterventionKind::MythGhostArmy:
            case InterventionKind::Subvert:
                break; // handled above — fog/myth ops + Execute/Subvert
                       // skip the squad guard
        }
    }
    pendingCommands_.clear();
}

bool BattleController::Tick() {
    if (beat_ != BattleBeat::Execution) return false;

    const int tick = static_cast<int>(sim_.TickCount());
    // 0) queued interventions apply at tick start (part of snapshot state)
    ApplyInterventions(tick);
    // 0.5) fog sync: prune dead clouds, auto-observe, decay — doctrine
    // reads this tick's belief, not truth.
    SyncFog();
    // CP regen: +1 per 60 s to each side, capped
    if (++cpRegen_ >= CP_REGEN_TICKS) {
        cpRegen_ = 0;
        for (int side = 0; side < 2; ++side) {
            if (cpPool_[side] < CP_CAP) ++cpPool_[side];
        }
    }
    // 1) doctrine eval on tick-start state (EvalTick is const on squads_)
    EvalOutcome out = Doctrine::EvalTick(map_, squads_, sheets_, cards_,
                                         sim_.Rng(), cpPool_, fog_, tick);
    // 2) commit pending deltas post-eval — IN LIST ORDER. Burn deltas
    //    route to the GovernanceField (a Move earlier in the list can
    //    preempt the arson); squad mutators go through ApplyDeltas.
    std::vector<SimEvent> burnEvents;
    for (const PendingDelta& d : out.deltas) {
        if (d.kind == PendingDelta::Kind::Burn) {
            if (d.squadIndex >= 0 &&
                static_cast<std::size_t>(d.squadIndex) < squads_.size()) {
                auto ev = field_.TryBurn(
                    static_cast<std::size_t>(d.squadIndex),
                    squads_[static_cast<std::size_t>(d.squadIndex)], tick);
                if (ev.has_value()) burnEvents.push_back(*ev);
            }
        } else {
            Doctrine::ApplyDeltas(squads_,
                                  std::span<const PendingDelta>(&d, 1));
        }
    }
    // 2.5) plan arrows: on-path Holding squads advance one leg
    MarchArrows();
    // 3) squads advance edge traversal; Routing squads age off-field
    for (std::size_t i = 0; i < squads_.size(); ++i) {
        Squad& s = squads_[i];
        s.TickMove();
        if (s.state == SquadState::Routing) {
            if (++routTimers_[i] >= ROUT_TICKS) {
                s.ApplyEvent(SquadEvent::RetreatComplete); // -> Routed
            }
        } else {
            routTimers_[i] = 0;
        }
    }
    // 3.5) governance scan on post-move truth: village dwell/latch,
    //      convoy march -> raid -> arrival.
    std::vector<SimEvent> govEvents = field_.Tick(squads_, tick);
    // 3.6) myth scan: shrine dedication dwell/latch -> capture.
    std::vector<SimEvent> mythEvents = myth_.Tick(squads_, tick);
    // 4) append this tick's events to the battle log (doctrine ->
    //    burn -> governance -> myth — canonical emission order)
    for (const SimEvent& e : out.events) events_.push_back(e);
    for (const SimEvent& e : burnEvents) events_.push_back(e);
    for (const SimEvent& e : govEvents) events_.push_back(e);
    for (const SimEvent& e : mythEvents) events_.push_back(e);
    // 5) sim housekeeping: tick counter + stream draw + stream checksum
    sim_.Tick();
    // 6) a wiped side closes the battle — same tick, after all commits;
    //    BeatChanged is stamped with the producing tick's index so every
    //    event of the closing tick shares one tick value. The stalemate
    //    timer is the same mechanism with a different verdict.
    if (CountEffective(0) == 0 || CountEffective(1) == 0) {
        beat_ = BattleBeat::Aftermath;
        CloseReason reason = CloseReason::Wipe;
        // Epic 7 — 不戰而屈人之兵: the wipe qualifies as Subversion
        // when the loser's ENTIRE force turned — no squad still
        // carries its banner (defectors flipped sides; corpses and
        // routed stragglers keep theirs) and at least one SquadDefected
        // deed records the turning. An ambiguous double-wipe stays
        // Wipe.
        const int eff0 = CountEffective(0), eff1 = CountEffective(1);
        if (eff0 != eff1) {
            const int winner = eff0 == 0 ? 1 : 0;
            const int loser = 1 - winner;
            bool bannerStands = false;
            for (const Squad& s : squads_) {
                if (s.side == loser) { bannerStands = true; break; }
            }
            bool turned = false;
            for (const SimEvent& e : events_) {
                if (e.kind == SimEvent::Kind::SquadDefected &&
                    e.side == winner) {
                    turned = true;
                    break;
                }
            }
            if (!bannerStands && turned) {
                reason = CloseReason::Subversion;
            }
        }
        CloseBattle(reason, tick);
    } else if (evalConfig_.stalemateTicks > 0 &&
               sim_.TickCount() >= static_cast<std::uint64_t>(
                                       evalConfig_.stalemateTicks)) {
        beat_ = BattleBeat::Aftermath;
        CloseBattle(CloseReason::Stalemate, tick);
    }
    return true;
}

void BattleController::SyncFog() {
    // Regions a side can actually see this tick: every effective,
    // non-routing squad's own region plus its neighbors.
    std::array<std::vector<std::size_t>, 2> seen;
    for (int s = 0; s < 2; ++s) {
        for (const Squad& o : squads_) {
            if (o.side != s || !o.IsEffective() ||
                o.state == SquadState::Routing ||
                o.regionIndex >= map_.RegionCount()) continue; // covers
                // NO_REGION too — assert-only guards aren't enough here
            seen[s].push_back(o.regionIndex);
            for (std::size_t n : map_.Neighbors(o.regionIndex)) {
                seen[s].push_back(n);
            }
        }
    }
    const auto visible = [&seen](int s, std::size_t r) {
        for (std::size_t x : seen[s]) if (x == r) return true;
        return false;
    };

    for (int s = 0; s < 2; ++s) {
        // 1) Prune clouds of squads that left the field.
        for (std::size_t i = 0; i < squads_.size(); ++i) {
            int& cid = cloudOf_[s][i];
            if (cid >= 0 && !squads_[i].IsEffective()) {
                fog_[s].RemoveCloud(cid);
                cid = -1;
            }
        }
    }
    // 2) Decay ~10/min first — then observe/collapse, so a freshly
    //    observed value lands at 100 post-decay (decay-after-collapse
    //    would make detect_threshold=100 permanently blind).
    // 3) Observe: physically-seen regions get full field certainty.
    // 4) Auto-observe: tracked enemy squads standing in a visible
    //    region collapse to truth; entangled partners resolve too
    //    (each to its OWN truth — shared fate, separate entities).
    for (int s = 0; s < 2; ++s) {
        fog_[s].TickDecay();
        for (std::size_t r : seen[s]) fog_[s].Observe(r);
        for (std::size_t i = 0; i < squads_.size(); ++i) {
            const Squad& e = squads_[i];
            const int cid = cloudOf_[s][i];
            if (cid < 0 || !e.IsEffective() ||
                e.regionIndex == Squad::NO_REGION) continue;
            if (!visible(s, e.regionIndex)) continue;
            // Mid-transit squads report their departure region —
            // regionIndex only updates on edge completion (Squad).
            fog_[s].Collapse(cid, static_cast<int>(e.regionIndex));
            const int partner = fog_[s].EntangledWith(cid);
            if (partner < 0 || !fog_[s].HasCloud(partner)) continue;
            for (std::size_t j = 0; j < squads_.size(); ++j) {
                if (cloudOf_[s][j] == partner && squads_[j].IsEffective() &&
                    squads_[j].regionIndex != Squad::NO_REGION) {
                    fog_[s].Collapse(
                        partner,
                        static_cast<int>(squads_[j].regionIndex));
                    break;
                }
            }
        }
    }
}

int BattleController::CountEffective(int side) const {
    int n = 0;
    for (const Squad& s : squads_) {
        if (s.side == side && s.IsEffective()) ++n;
    }
    return n;
}

void BattleController::CloseBattle(CloseReason reason, int stampTick) {
    pendingCommands_.clear(); // after Aftermath nothing ever applies
    result_ = BattleResult{};
    EmitBeatChanged(stampTick, BattleBeat::Aftermath); // always adjacent
    result_.closeReason = reason;
    result_.forced = (reason == CloseReason::Concede);
    result_.stalemate = (reason == CloseReason::Stalemate);
    result_.subverted = (reason == CloseReason::Subversion);
    result_.elapsedTicks = sim_.TickCount();
    for (int side = 0; side < 2; ++side) {
        for (const Squad& s : squads_) {
            if (s.side != side) continue;
            switch (s.state) {
                case SquadState::Routed:    ++result_.routed[side]; break;
                case SquadState::Destroyed: ++result_.destroyed[side]; break;
                default:                    ++result_.effective[side]; break;
            }
        }
    }
    const int eff0 = result_.effective[0], eff1 = result_.effective[1];
    result_.winnerSide = (eff0 > 0 && eff1 == 0) ? 0
                       : (eff1 > 0 && eff0 == 0) ? 1 : -1;
    // Draw evaluation never picks a winner — the campaign layer decides
    // defeat conversion; the battle only reports the verdict + data.
    // Ledger postings, canonical squad order; verdict entry last.
    for (std::size_t i = 0; i < squads_.size(); ++i) {
        const Squad& s = squads_[i];
        if (s.state == SquadState::Destroyed) {
            result_.ledger.push_back({LedgerEvent::Type::Casualty,
                                      s.side, static_cast<int>(i), s.id});
        } else if (s.state == SquadState::Routed) {
            result_.ledger.push_back({LedgerEvent::Type::Rout, s.side,
                                      static_cast<int>(i), s.id});
        }
    }
    result_.ledger.push_back(
        {result_.winnerSide >= 0 ? LedgerEvent::Type::Victory
                                 : LedgerEvent::Type::Draw,
         result_.winnerSide, -1, {}});
    // Self-describing journal: the verdict is itself an event, so a
    // record's event stream carries the verdict alongside the inputs
    // that produced it (the checksum seals sim state, not the result).
    events_.push_back({SimEvent::Kind::ResultDeclared,
                       stampTick, -1, -1,
                       result_.winnerSide, static_cast<int>(reason),
                       -1, {}, {}});
}

std::uint64_t BattleController::Checksum() const {
    std::uint64_t h = sim_.Checksum();
    h = Fold(h, sim_.TickCount());
    h = Fold(h, static_cast<std::uint64_t>(beat_));
    h = Fold(h, static_cast<std::uint64_t>(cpPool_[0]));
    h = Fold(h, static_cast<std::uint64_t>(cpPool_[1]));
    h = Fold(h, static_cast<std::uint64_t>(squads_.size()));
    for (std::size_t i = 0; i < squads_.size(); ++i) {
        const Squad& s = squads_[i];
        h = Fold(h, static_cast<std::uint64_t>(s.maxHp));
        h = Fold(h, static_cast<std::uint64_t>(s.hp));
        h = Fold(h, static_cast<std::uint64_t>(s.attack));
        h = Fold(h, static_cast<std::uint64_t>(s.speedMilli));
        h = Fold(h, static_cast<std::uint64_t>(s.cohesion));
        h = Fold(h, static_cast<std::uint64_t>(s.side));
        h = Fold(h, static_cast<std::uint64_t>(s.state));
        h = Fold(h, static_cast<std::uint64_t>(s.possessed));
        h = Fold(h, RegionKey(s.regionIndex));
        h = Fold(h, RegionKey(s.edgeTarget));
        h = Fold(h, static_cast<std::uint64_t>(s.edgeProgress));
        h = Fold(h, static_cast<std::uint64_t>(routTimers_[i]));
        for (const CardSlot& slot : sheets_[i].slots) {
            h = Fold(h, static_cast<std::uint64_t>(slot.cardIndex));
            h = Fold(h, static_cast<std::uint64_t>(slot.cooldownRemaining));
            h = Fold(h, static_cast<std::uint64_t>(slot.forceNext));
        }
    }
    // Committed-but-unapplied commands are sim state too — two
    // controllers with different queued orders must fingerprint apart.
    h = Fold(h, static_cast<std::uint64_t>(pendingCommands_.size()));
    for (const Intervention& c : pendingCommands_) {
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.side)));
        h = Fold(h, static_cast<std::uint64_t>(c.kind));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.squadIndex)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.target)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.issueTick)));
        h = Fold(h, static_cast<std::uint64_t>(c.path.size()));
        for (std::size_t r : c.path) {
            h = Fold(h, RegionKey(r));
        }
    }
    // Plan arrows (battle state).
    for (const PlanArrow& a : arrows_) {
        h = Fold(h, static_cast<std::uint64_t>(a.active));
        h = Fold(h, static_cast<std::uint64_t>(a.cursor));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(a.bonusApplied)));
        h = Fold(h, static_cast<std::uint64_t>(a.path.size()));
        for (std::size_t r : a.path) h = Fold(h, RegionKey(r));
    }
    // Fog state: region fields + cloud beliefs + the truth<->cloud map
    // + probe budgets: decayAccum is derived from TickCount — skipped.
    for (int s = 0; s < 2; ++s) {
        for (const CloudEntity& c : fog_[s].Clouds()) {
            h = Fold(h, static_cast<std::uint64_t>(
                            static_cast<std::uint32_t>(c.id)));
            h = Fold(h, static_cast<std::uint64_t>(
                            static_cast<std::uint32_t>(c.believedRegion)));
            h = Fold(h, static_cast<std::uint64_t>(
                            static_cast<std::uint32_t>(c.certainty)));
            h = Fold(h, static_cast<std::uint64_t>(
                            static_cast<std::uint32_t>(c.entangledWith)));
        }
        for (int cid : cloudOf_[s]) {
            h = Fold(h, static_cast<std::uint64_t>(
                            static_cast<std::uint32_t>(cid)));
        }
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(probeBudget_[s])));
    }
    // Region certainty fields — same length on both fogs.
    for (std::size_t r = 0; r < map_.RegionCount(); ++r) {
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(fog_[0].CertaintyAt(r))));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(fog_[1].CertaintyAt(r))));
    }
    // GovernanceField: village tracks + convoys are sim state too.
    h = Fold(h, static_cast<std::uint64_t>(field_.Villages().size()));
    for (const VillageTrack& v : field_.Villages()) {
        h = Fold(h, RegionKey(v.region));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(v.claimant)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(v.dwell)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(v.occupier)));
        h = Fold(h, static_cast<std::uint64_t>(v.burned));
    }
    h = Fold(h, static_cast<std::uint64_t>(field_.Convoys().size()));
    for (const Convoy& c : field_.Convoys()) {
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.side)));
        h = Fold(h, static_cast<std::uint64_t>(c.cursor));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(c.legProgress)));
        h = Fold(h, static_cast<std::uint64_t>(c.active));
        h = Fold(h, static_cast<std::uint64_t>(c.path.size()));
        for (std::size_t r : c.path) h = Fold(h, RegionKey(r));
    }
    // MythField: per-region infiltration levels are sim state.
    h = Fold(h, static_cast<std::uint64_t>(myth_.RegionCount()));
    for (std::uint8_t l : myth_.Levels()) {
        h = Fold(h, static_cast<std::uint64_t>(l));
    }
    // Shrine tracks: region, latch state, deity stance — all folded.
    h = Fold(h, static_cast<std::uint64_t>(myth_.Shrines().size()));
    for (const ShrineTrack& s : myth_.Shrines()) {
        h = Fold(h, RegionKey(s.region));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(s.claimant)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(s.dwell)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(s.owner)));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint8_t>(s.stance[0])));
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint8_t>(s.stance[1])));
        // The invasion cadence timer is sim state — two runs
        // differing only in when the deity last struck must diverge.
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(s.invasionTick)));
    }
    // Ghost garrisons are sim state — 陰兵守土 is fingerprinted.
    for (std::int8_t g : myth_.Ghosts()) {
        h = Fold(h, static_cast<std::uint64_t>(
                        static_cast<std::uint8_t>(g)));
    }
    return h;
}

} // namespace Potato::Gameplay
