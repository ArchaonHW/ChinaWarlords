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
The assignments remain derived: they are not stored, so loading the same save
and applying the same policy and budget reproduces the plan. Campaign callers
must provide only locations and factors that are public or observed; this
record is not a channel for hidden enemy position or battle truth.

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
them requires the activation and rehydration contract below.

## Scope boundary and next work

Recommended sequence:

1. Define AGGREGATE and DORMANT activation/rehydration rules, with state
   transitions represented in the campaign event history.
2. Add measured per-backend compute budgets. Keep CPU as the deterministic
   reference; only add GPU/NPU paths after parity and replay behavior are
   specified.
3. Add an authored end-to-end scenario that exercises all LOD levels before
   connecting the policy to live world ticks.

The unit test also plans the specification's mixed scale of 100 FULL and
100,000 ABSTRACT entities. This verifies classification and allocation
capacity, not the performance of 100,000 simulation kernels.
