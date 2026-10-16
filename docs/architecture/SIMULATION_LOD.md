# Deterministic hierarchical simulation LOD

## Audit

The production battle runtime is `Gameplay::BattleController`. It owns the
20 Hz tick, battle state, deterministic events, checksum, fog synchronization,
and replay-facing behavior. `Gameplay::Sim::Sim` is a small seed/tick/PRNG
kernel used by the controller and several shared timing constants; it is not
a second battle runtime.

The campaign, game shell, doctrine, battle AI, and replay layers already
exist. The current `BattleAI` is a side-level battle policy. There is no
hierarchical agent simulation, LOD assignment policy, or shared
`SimulationBudget` in the gameplay layer.

## First implementation slice

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

The bridge does not yet run different update kernels, schedule CPU/GPU/NPU
work, or alter the battle tick/replay format. It adds no second tick system;
REDUCED and ABSTRACT state representations must be specified first.

## Scope boundary and next work

This slice defines deterministic level selection, capacity enforcement, and
the campaign's public-data bridge. It does not yet run different update
kernels, schedule CPU/GPU/NPU work, or change the battle tick/replay format.
Keeping that boundary avoids introducing a second tick system before reduced
and abstract state representations exist.

Recommended sequence:

1. Define REDUCED and ABSTRACT integer update kernels and compare their
   deterministic outputs against explicit reference scenarios.
2. Define AGGREGATE and DORMANT activation/rehydration rules, with state
   transitions represented in the campaign event history.
3. Add measured per-backend compute budgets. Keep CPU as the deterministic
   reference; only add GPU/NPU paths after parity and replay behavior are
   specified.
4. Add an authored end-to-end scenario that exercises all LOD levels before
   connecting the policy to live world ticks.

The unit test also plans the specification's mixed scale of 100 FULL and
100,000 ABSTRACT entities. This verifies classification and allocation
capacity, not the performance of 100,000 simulation kernels.
