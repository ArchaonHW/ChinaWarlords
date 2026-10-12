#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {

class BattleMap;
class QuantumFog;
struct Squad;

// Data-driven doctrine cards — the player's authorship surface.
// trigger → condition → action → modifier; per-card cooldown.
// Both sides run the identical interpreter (symmetric AI).

enum class TriggerKind : std::uint8_t {
    Always,
    CohesionBelow,  // param: percent
    EnemyInRegion,  // enemy side squad in my region (snapshot)
    EnemyAdjacent,  // enemy side squad in a neighboring region (snapshot)
};

enum class ConditionKind : std::uint8_t {
    Always,
    CohesionAbove, // param: percent
    CohesionBelow, // param: percent
    CpAtLeast,     // param: CP points
};

enum class ActionKind : std::uint8_t {
    Hold,            // record only — still fires + cooldowns
    Brace,           // param: cohesion restored
    Move,            // param: region index (pending move order)
    Retreat,         // move to lowest-index neighbor (map-edge proxy)
    Burn,            // torch the village the squad stands on
                     // (GovernanceField gates at apply)
};

enum class ModifierKind : std::uint8_t {
    None,
    AttackBoost, // param: attack delta
};

struct DoctrineCard {
    std::string id;
    std::string name;
    TriggerKind trigger = TriggerKind::Always;
    std::int64_t triggerParam = 0;
    ConditionKind condition = ConditionKind::Always;
    std::int64_t conditionParam = 0;
    ActionKind action = ActionKind::Hold;
    std::int64_t actionParam = 0;
    ModifierKind modifier = ModifierKind::None;
    std::int64_t modifierParam = 0;
    int cooldownTicks = 100; // 5 s default at 20 Hz
};

// Runtime slot on a squad's doctrine sheet.
struct CardSlot {
    std::size_t cardIndex = ~std::size_t{0}; // index into DoctrineLibrary
    int cooldownRemaining = 0;               // ticks until fireable
    // CP override (Story 1.7): fire once bypassing trigger/condition;
    // cleared on the slot's next visit whether or not it can act.
    bool forceNext = false;
};

// 3–5 slotted cards per squad sheet.
struct SquadSheet {
    static constexpr std::size_t MIN_SLOTS = 3;
    static constexpr std::size_t MAX_SLOTS = 5;

    std::vector<CardSlot> slots;

    // Build a sheet from card ids; all must resolve in the library.
    static Result<SquadSheet> Build(const class DoctrineLibrary& cards,
                                    const std::vector<std::string>& cardIds);
};

// Sim event — the recorded truth, append-only into the battle log.
struct SimEvent {
    enum class Kind : std::uint8_t {
        CardFired,
        BeatChanged,
        Intervention,
        // Emitted once at close after the final BeatChanged:
        // param = winnerSide (-1 draw), aux = CloseReason ordinal
        // (0=Wipe, 1=Concede, 2=Stalemate — wire-format stable).
        ResultDeclared,
        // GovernanceField deeds (Epic 4) — truth-layer outcomes
        // destined for ledger posting. The four field kinds emit
        // post-movement; SquadExecuted emits during intervention
        // apply (step 0). param = region in all five; side =
        // the perpetrator.
        VillageOccupied, // squadIndex = occupier witness
        VillageBurned,   // squadIndex = the arsonist
        ConvoyArrived,   // aux = convoy index; squadIndex = escort
                         // of record at destination (-1 unescorted)
        ConvoyRaided,    // aux = convoy index; squadIndex = raider
        // Refused rout-surrender (Epic 4.2): emitted when a pending
        // Execute intervention applies to a still-Routing victim.
        // param = region, squadIndex = victim, side = refuser.
        SquadExecuted,
        // Myth-layer infiltration transition (Epic 5.1): emitted when
        // a myth event changes a region's level. param = region,
        // aux = new InfiltrationLevel ordinal (0..3); squadIndex and
        // side = -1 — the myth layer is side-less.
        InfiltrationChanged,
        // Shrine allegiance flip (Epic 5.2): a shrine region held by
        // exclusive presence for SHRINE_DEDICATION_TICKS latches to
        // that side. param = region, squadIndex = witness, side =
        // new owner, aux unused (0).
        ShrineCaptured,
        // Myth action applied (Epic 5.4): emitted when a queued
        // MythPacify/MythPossession/MythGhostArmy intervention lands.
        // aux = MythActionKind ordinal (0..2), param = region,
        // squadIndex = target squad (possession) or -1, side = caster.
        MythActionInvoked,
        // Deity counterattack (Epic 5.5): a shrine region at
        // InfiltrationLevel::Invaded pushes back on an
        // INVASION_PERIOD_TICKS cadence. param = region; aux = form
        // (0 = pushback rise for the occupier's enemy, 1 = wild
        // haunting contesting all claims, 2 = sustain beat); side =
        // the ghost host's banner (-1 for wild); squadIndex = -1 —
        // the myth layer is side-less as an actor.
        MythInvasion,
    };

    Kind kind = Kind::CardFired;
    int tick = 0;
    int squadIndex = -1;
    int slotIndex = -1;
    // Intervention: param = target region (Redirect, Probe) or slot
    // index (Override) or cloud id B (Entangle — squadIndex carries
    // cloud id A) or path length (Replan — the path itself lives in
    // the queued Intervention, which the checksum folds) or the
    // victim's region (Execute — squadIndex carries the victim);
    // MythPacify/MythGhostArmy: param = target region; MythPossession:
    // param = the squad's region at issue (journal/checksum only —
    // apply re-derives). aux = InterventionKind ordinal (0=Redirect,
    // 1=Override, 2=Retreat, 3=Probe, 4=Entangle, 5=Replan, 6=Execute,
    // 7=MythPacify, 8=MythPossession, 9=MythGhostArmy — wire stable).
    int param = -1;
    // BeatChanged: new BattleBeat ordinal — wire-format stable
    // (Planning=0, Execution=1, Aftermath=2; recorder/replay depends on it).
    int aux = 0;
    // Issuing side for Intervention events (CardFired stamps the acting
    // squad's side); -1 where side-less. Replay needs this to re-issue
    // commands — squadIndex alone can't carry it (Probe uses -1,
    // Entangle repurpose it as a cloud id).
    int side = -1;
    // Replan path payload — completes the command journal so a recorded
    // Intervention event fully re-drives IssueReplan (Story 1.9 deferral).
    std::vector<std::size_t> path;
    std::string cardId;
};

// Pending write from an action/modifier — applied AFTER all slots eval
// (snapshot isolation: same-tick writes are never visible to triggers).
struct PendingDelta {
    // Burn mutates no squad — it targets the village the squad
    // stands on. ApplyDeltas skips it; the controller routes it to
    // the GovernanceField (ordered with the rest, so a Move delta
    // earlier in the list can preempt the arson).
    enum class Kind : std::uint8_t { Cohesion, Move, Attack, Burn };

    Kind kind = Kind::Cohesion;
    int squadIndex = -1;
    int amount = 0;                  // Cohesion/Attack
    std::size_t region = ~std::size_t{0}; // Move target
};

struct EvalOutcome {
    std::vector<SimEvent> events;
    std::vector<PendingDelta> deltas;
};

// potato.doctrine_cards/1 — a card library file.
class DoctrineLibrary {
public:
    static Result<DoctrineLibrary> Load(std::string_view path);
    static Result<DoctrineLibrary> FromJson(const JsonValue& root);

    std::size_t Count() const { return cards_.size(); }
    // Precondition: index < Count().
    const DoctrineCard& At(std::size_t index) const {
        assert(index < cards_.size());
        return cards_[index];
    }
    // nullptr on miss; valid for the library's lifetime.
    const DoctrineCard* Find(std::string_view id) const;
    std::size_t IndexOf(std::string_view id) const { // Count() on miss
        for (std::size_t i = 0; i < cards_.size(); ++i) {
            if (cards_[i].id == id) return i;
        }
        return cards_.size();
    }

private:
    std::vector<DoctrineCard> cards_;
};

// The interpreter. EvalTick is pure w.r.t. squads — reads the caller's
// tick-start state (pass a snapshot copy if deltas must not leak between
// calls), writes events + pending deltas; ApplyDeltas commits them.
// Cooldowns live on the (mutable) sheets and tick once per occupied
// slot per EvalTick — even for squads that can't act.
// Routing/Routed/Destroyed squads evaluate nothing. Enemy-detection
// triggers read the acting side's QuantumFog belief — never truth.
namespace Doctrine {

// snapshot: squads' state at tick start; sheets are per-squad, aligned
// by index with snapshot (asserted equal; a shorter sheets vector
// silently skips trailing squads in release builds — don't do that).
// cpPools is the per-side CP pool — cp_at_least reads the acting
// squad's side (index 0 = player, 1 = enemy; out-of-range sides read 0).
// fog is the per-side QuantumFog view — enemy_in_region/enemy_adjacent
// read the ACTING side's belief (VisibleAt), never true positions
// (the truth boundary — Story 1.8 closes the deferred seam).
// rng is reserved for future draws — vocab v0 draws nothing, so
// draw-order determinism is structural.
EvalOutcome EvalTick(const BattleMap& map,
                     const std::vector<Squad>& snapshot,
                     std::vector<SquadSheet>& sheets,
                     const DoctrineLibrary& cards,
                     Prng& rng,
                     const std::array<int, 2>& cpPools,
                     const std::array<QuantumFog, 2>& fog,
                     int tick);

// Apply pending deltas to live squads — call AFTER EvalTick.
// Terminal squads (Routed/Destroyed) ignore deltas. Burn deltas
// are skipped here — they belong to the GovernanceField; callers
// preserving interleaved ordering dispatch per-delta via
// std::span<const PendingDelta>(&d, 1).
void ApplyDeltas(std::vector<Squad>& squads,
                 std::span<const PendingDelta> deltas);

} // namespace Doctrine

} // namespace Potato::Gameplay
