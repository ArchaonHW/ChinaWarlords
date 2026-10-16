# Deterministic hierarchical simulation LOD

## Audit

The production battle runtime is `Gameplay::BattleController`. It owns the
20 Hz tick, battle state, deterministic events, checksum, fog synchronization,
and replay-facing behavior. `Gameplay::Sim::Sim` is a small seed/tick/PRNG
kernel used by the controller and several shared timing constants; it is not
a second battle runtime.

The campaign, game shell, doctrine, battle AI, and replay layers already
exist. The current `BattleAI` is a side-level battle policy. The gameplay
layer now has a pure LOD assignment policy and integer REDUCED/ABSTRACT macro
kernels, but no live hierarchical agent scheduler.

## LOD assignment policy

`Gameplay::Sim::SimulationLodPolicy` is a pure, headless planning function.
It scores six normalized integer inputs:

```
importance = distance*wDistance
           + combat*wCombat
           + narrative*wNarrative
           + strategic*wStrategic
           + playerInterest*wPlayerInterest
           + mythicImportance*wMythicImportance
```

Each factor is in the range 0..1000. The distance factor means normalized
proximity or relevance (1000 is near/relevant, 0 is remote), not a raw world
coordinate. Weights are bounded nonnegative integers; default thresholds map
scores to FULL, REDUCED, ABSTRACT, AGGREGATE, or DORMANT.

`SimulationBudget` limits FULL, REDUCED, ABSTRACT, and AGGREGATE assignments.
Defaults support 100 FULL entities and 100,000 ABSTRACT entities in one plan.
Candidates are considered by requested tier, then descending importance;
agent ID is the stable tie-break. When a tier is full, overflow degrades to
the next tier. Remaining entities become DORMANT. Results are sorted by ID,
so input container order cannot change the plan.

Invalid weights, thresholds, factor ranges, or duplicate IDs reject the whole
plan. The policy does not mutate inputs or inspect hidden battle state.

## Campaign bridge

`Campaign::WorldAgentState` now gives each campaign world agent a stable
64-bit ID, a campaign-visible map node, and the six normalized public LOD
factors. `WorldState::SetAgents` validates the complete replacement against
the bound map and duplicate IDs before committing it. Agent records are sorted
by ID and persisted in the optional `agents` array of `potato.worldstate/1`,
which is already carried inside `potato.campaign/2` saves.

`WorldState::PlanAgentLod` projects those records into the pure Gameplay policy.
The target assignments remain derived from policy and budget. Campaign code
may apply a complete canonical plan with `WorldState::ApplyAgentLodPlan`; this
atomically stores the current tier and appends one event for each changed
agent. Reapplying an unchanged plan adds no events. The optional `lod_runtime`
field in `potato.worldstate/1` persists both the current tiers and transition
history. Older saves without that field load their current agents as DORMANT.
Malformed or partial runtime data rejects the entire document.

AGGREGATE summaries are derived from agents currently assigned that tier.
Agents combine only when they share a public map node and the same nonzero
authored `aggregateGroupId`; a zero group ID keeps an agent isolated. A summary
contains stable member IDs and sums of the six public importance factors.
DORMANT campaign agents keep their campaign-visible identity and factors, so a
later policy plan can reactivate them when those inputs change. Campaign still
does not own evolving battle state; it must not become a channel for hidden
enemy state. Gameplay snapshot projection is defined separately below.
Campaign callers must provide only locations and factors that are public or
observed; these records are not a channel for hidden enemy position or battle
truth.

## REDUCED and ABSTRACT macro kernels

`Gameplay/Sim/SimulationLodKernels.h` defines two pure state transitions. They
share the 0..1000 integer scale and accept one summarized interval at a time.
The interval's deltas represent its complete effect; they are not per-tick
rates, and `elapsedTicks` only advances the state's clock. Neither function
loops through the 20 Hz battle ticks or draws from the battle PRNG.

REDUCED stores one representative agent's strength, morale, and supply. It
adds each bounded signed delta and clamps each result to 0..1000.

ABSTRACT stores a cohort population and exact totals for the same measures.
For each total, losses remove `floor(total * losses / population)` using
overflow-safe integer arithmetic, so the integer remainder stays with the
survivors. Per-survivor deltas then adjust the remaining totals, clamped to
0..`survivors * 1000`. A zero-population cohort can remain in the clocked
state with zero totals. Both functions reject invalid states, zero-length or
overflowing intervals, out-of-range deltas, and excessive losses without
changing the caller's output value.

Reference tests pin a REDUCED pressure and recovery sequence, an ABSTRACT
two-of-ten attrition scenario, deterministic replay of each input, and parity
between both kernels for a one-member cohort. These kernels remain isolated
from campaign persistence, event history, and the live battle tick; connecting
them to a live scheduler requires the lifecycle contract below.

## Agent snapshot projection

`SimulationRosterSnapshot` is a member-level simulation snapshot with stable
agent IDs, bounded strength/morale/supply values, and one shared elapsed-tick
clock. `ProjectToAbstract` sorts members by unsigned ID, rejects duplicates or
out-of-range values, and emits an `AbstractSimulationSnapshot` with the sorted
membership and exact integer totals. Reordering the input roster cannot change
the result.

`RehydrateFromAbstract` restores members in that same canonical order. For
each measure it divides the total evenly; the remainder rotates from the ID at
`elapsedTicks % population` and wraps through ascending IDs. This preserves
population, all three totals, and elapsed ticks exactly, and repeated
rehydration of the same snapshot produces identical member values. Individual
pre-aggregation values are not recoverable; rehydrated values are the explicit
even distribution implied by this contract. Invalid membership or totals
leave the output unchanged. These allocation and sorting operations run at an
LOD transition boundary, not in the 20 Hz tick.

The snapshot types live in `Gameplay::Sim`, where simulation truth belongs.
They are not serialized into Campaign's public agent projection. The
transition contract is to retain a DORMANT roster snapshot and its clock
without advancing it, so reactivation can use that exact member roster. A
roster has one shared clock; a dormant roster with a different clock must stay
separate until the live scheduler defines how to align it. This PR supplies
the snapshot transform but does not schedule or execute those transitions.
AGGREGATE province-level combat state and the live
FULL/REDUCED/ABSTRACT/AGGREGATE/DORMANT scheduler are still not connected.

## Scope boundary and next work

The opt-in `potato_benchmark_simulation_lod` executable measures the CPU
reference planner at 100, 1,000, 10,000, and 100,000 subjects. It reports the
median of seven independent planning runs and a deterministic output checksum.
Configure with `-DPOTATO_BENCHMARKS=ON`, then build and run that target. The
elapsed time is host-dependent measurement data; it is not used to assign LODs
or change replay behavior. Record results alongside the CPU and build
configuration before selecting a CPU compute budget. Only add GPU/NPU paths
after parity and replay behavior are specified.

Next, connect snapshots to live world ticks only after the scheduler and
public-versus-hidden state boundaries are tested end to end.

The unit test also plans the specification's mixed scale of 100 FULL and
100,000 ABSTRACT entities. This verifies classification and allocation
capacity, not the performance of 100,000 simulation kernels.
