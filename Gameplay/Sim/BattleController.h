#pragma once

#include "Gameplay/Command/Intervention.h"
#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Eval/WinEval.h"
#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Governance/GovernanceField.h"
#include "Gameplay/Myth/Infiltration.h"
#include "Gameplay/Plan/BattlePlan.h"
#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Potato::Gameplay {

class BattleMap;
struct Squad;
struct SquadTemplate;

// Three-beat battle loop — the state table IS the spec (arch pattern:
// enum class + explicit transition table, unit-testable).
enum class BattleBeat : std::uint8_t { Planning, Execution, Aftermath };

constexpr bool CanTransition(BattleBeat a, BattleBeat b) {
    return (a == BattleBeat::Planning && b == BattleBeat::Execution) ||
           (a == BattleBeat::Execution && b == BattleBeat::Aftermath);
}

// The Aftermath verdict (`BattleResult`, `CloseReason`, `EvalConfig`)
// lives in Gameplay/Eval/WinEval.h — the battle reports the result;
// Epic 2's ledger posts the emitted ledger-event list verbatim.

// Ticks a Routing squad needs to leave the field (2 s at 20 Hz).
constexpr int ROUT_TICKS = 2 * TICK_RATE_HZ;

// Owns one battle: sim kernel (seed, tick counter, checksum, the PRNG
// stream), live squads, their doctrine sheets, and the append-only
// SimEvent log (record-is-truth — the recorder consumes exactly this).
// Beats: Planning accepts deploy/sheet input and never ticks;
// Execution ticks deterministically; Aftermath is terminal.
// `map` and `cards` must outlive the controller (borrowed refs).
class BattleController {
public:
    BattleController(std::uint64_t seed, const BattleMap& map,
                     const DoctrineLibrary& cards,
                     const FogConfig& fogConfig = FogConfig{},
                     const PlanConfig& planConfig = PlanConfig{},
                     const EvalConfig& evalConfig = EvalConfig{});
    ~BattleController();
    // Copy-constructible (tests snapshot whole controllers); copies share
    // the borrowed map_/cards_ refs. Assignment deleted — refs can't reseat.
    BattleController(const BattleController&);
    BattleController& operator=(const BattleController&) = delete;

    BattleBeat Beat() const { return beat_; }

    // Table-gated transition; illegal moves (e.g. Planning->Aftermath)
    // rejected with no side effects. Emits BeatChanged into the log.
    bool RequestBeat(BattleBeat target);

    // --- Planning input (rejected outside Planning) ---
    bool DeploySquad(const SquadTemplate& t, std::size_t regionIndex,
                     int side);
    bool SetSheet(std::size_t squadIndex, SquadSheet sheet);
    // Test/harness hook — the live economy (regen, cap) runs itself.
    bool SetCpPool(int side, int cp);

    // --- CP interventions (Execution only) ---
    // Deduct-at-issue; the command applies at the start of the next tick
    // (before doctrine eval). Rejections carry a reason and never deduct.
    Result<bool> IssueRedirect(int side, int squadIndex,
                               std::size_t region);   // 1 CP
    Result<bool> IssueOverride(int side, int squadIndex,
                               int slotIndex);        // 2 CP
    Result<bool> IssueRetreat(int side, int squadIndex); // 3 CP
    // Fog ops: probe raises a region's certainty field (budgeted,
    // no CP); entangle links two of this side's clouds to share fate.
    Result<bool> IssueProbe(int side, std::size_t region);   // 0 CP, 3/battle
    Result<bool> IssueEntangle(int side, int cloudIdA, int cloudIdB); // 2 CP
    // Replan: rewrite a squad's arrow mid-execution; bonus recomputes
    // from CURRENT certainty at apply time. Costs planConfig.replanCost.
    Result<bool> IssueReplan(int side, int squadIndex,
                             std::vector<std::size_t> path);
    // Refuse a routing enemy's surrender (Epic 4.2): the victim must
    // be a Routing ENEMY squad and the refusing side needs an
    // effective non-routing squad in the victim's region (checked at
    // issue AND apply — mercy by absence). 0 CP: cruelty is free;
    // the atrocity posting is the price. On apply the victim goes
    // Routing -> Destroyed and a SquadExecuted deed event records it.
    Result<bool> IssueExecute(int side, int squadIndex);   // 0 CP
    // Myth actions (Epic 5.4) — 0 CP: paid in 天命 through the
    // campaign ledger (Campaign/Myth/Mandate.h) before the host
    // issues the verb; the record carries the deed and unpaid
    // miracles are an audit finding downstream, not a sim error.
    // Pacify backs infiltration off a region and releases the
    // shrine's latch; possession blesses an OWN effective squad
    // (once per battle per squad); ghost army raises a spirit
    // garrison on a region where infiltration >= Whispered.
    Result<bool> IssueMythPacify(int side, std::size_t region);
    Result<bool> IssueMythPossession(int side, int squadIndex);
    Result<bool> IssueGhostArmy(int side, std::size_t region);

    // Planning-only intel hook (briefing/tests): overwrite a cloud's
    // believed region + certainty in `fogSide`'s view for `squadIndex`
    // (which must be hostile to fogSide — own squads aren't clouds).
    bool SetCloudIntel(int fogSide, int squadIndex,
                       int region, int certainty);

    // --- BattlePlan (Planning only) ---
    // Draw an arrow: `path` starts at the squad's current region,
    // adjacent pairs; replaces any existing arrow for that squad.
    bool DrawArrow(int side, int squadIndex,
                   const std::vector<std::size_t>& path);
    const PlanArrow& ArrowOf(std::size_t squadIndex) const;

    // --- GovernanceField (Planning only) ---
    // Spawn a noncombatant convoy on a fixed route. Path rules match
    // DrawArrow's (in-bounds, adjacent, no revisits, len >= 2); the
    // convoy's march/raid/arrival emits during Execution ticks.
    bool SpawnConvoy(int side, std::vector<std::size_t> path);
    const GovernanceField& Field() const { return field_; }

    // --- MythField (Planning only) ---
    // Seed a region's infiltration level carried in from campaign
    // state (journaled as "mythseed") — an initial condition, not a
    // transition, so it emits no event. `level` is the raw int; the
    // InfiltrationLevel wire gate validates 0..3. Rejected when the
    // write would change nothing — journaled ops must move state.
    bool SeedInfiltration(std::size_t region, int level);
    // Apply a myth-layer event through the transition table
    // (journaled as "myth"). Emits InfiltrationChanged when the level
    // actually moves; a saturating no-op REJECTS (journaling only
    // accepts state-changing calls). Planning-phase input only —
    // mid-Execution myth drivers are the IssueMyth* verbs (5.4).
    bool ApplyMythEvent(std::size_t region, MythEventKind kind);
    const MythField& Myth() const { return myth_; }

    // --- Execution tick ---
    // One deterministic tick: doctrine eval (snapshot semantics) ->
    // apply pending deltas -> squads advance edge traversal -> append
    // events -> sim housekeeping draw -> side-wipe check auto-closes.
    // No-op returning false outside Execution.
    bool Tick();

    // --- Read-only surface ---
    const Sim& GetSim() const { return sim_; }
    const std::vector<Squad>& Squads() const { return squads_; }
    const std::vector<SimEvent>& Events() const { return events_; }
    const BattleResult& Outcome() const { return result_; }
    int CpPool(int side) const {
        assert(side == 0 || side == 1);
        return cpPool_[side == 1 ? 1 : 0];
    }
    // Belief views (UI/doctrine/intel debugging) — never truth.
    const QuantumFog& Fog(int side) const {
        assert(side == 0 || side == 1);
        return fog_[side == 1 ? 1 : 0];
    }

    // End-to-end determinism fingerprint: sim stream checksum folded
    // with per-squad numeric state, beat, CP pools, and the pending
    // command queue (committed-but-unapplied state). Strings are
    // excluded (Squad is non-POD — hash field values, never object
    // memory).
    std::uint64_t Checksum() const;

private:
    int CountEffective(int side) const;
    void EmitBeatChanged(int tick, BattleBeat target);
    // Shared gate for the IssueMyth* verbs: Execution + side domain
    // + one queued act per (kind, target).
    Result<bool> CheckMythVerb(int side, InterventionKind kind,
                               int key) const;
    // stampTick = the tick index the close belongs to (producing tick
    // inside Tick(), current TickCount when conceded between ticks).
    void CloseBattle(CloseReason reason, int stampTick);
    void ApplyInterventions(int tick);
    // Per-tick fog maintenance: prune off-field clouds, auto-observe
    // (collapse enemy clouds sharing/adjoining a friendly region,
    // entangled partners collapse to their own truth), then decay.
    void SyncFog();
    // Execution-start plan bonuses: attack += attack*mean/100 (capped).
    void ApplyPlanBonuses();
    // Arrow march step: on-path Holding squads advance one leg.
    void MarchArrows();

    const BattleMap& map_;
    const DoctrineLibrary& cards_;
    Sim sim_;
    BattleBeat beat_ = BattleBeat::Planning;
    std::vector<Squad> squads_;
    std::vector<SquadSheet> sheets_;     // index-aligned with squads_
    std::vector<int> routTimers_;        // index-aligned; Routing squads age out
    std::vector<SimEvent> events_;       // append-only battle log
    std::vector<Intervention> pendingCommands_; // applied at next tick start
    BattleResult result_;
    std::array<int, 2> cpPool_ = {CP_START, CP_START};
    int cpRegen_ = 0;
    // QuantumFog: per-observing-side belief views. cloudOf_[s][i] =
    // side-s fog's cloud id tracking squad i (-1 = none: own squads,
    // removed). The truth<->cloud mapping lives HERE (Sim) — the fog
    // itself is belief-only.
    std::array<QuantumFog, 2> fog_;
    std::array<std::vector<int>, 2> cloudOf_;
    std::array<int, 2> probeBudget_ = {PROBE_BUDGET, PROBE_BUDGET};
    PlanConfig planConfig_;
    EvalConfig evalConfig_;
    std::vector<PlanArrow> arrows_; // index-aligned; active==false=none
    GovernanceField field_; // village tracks + convoys (Epic 4)
    MythField myth_;        // per-region infiltration 0-3 (Epic 5)
};

} // namespace Potato::Gameplay
