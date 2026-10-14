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

## Scope boundary and next work

This slice defines deterministic level selection and capacity enforcement. It
does not yet run different update kernels, schedule CPU/GPU/NPU work, change
campaign state, or alter the battle tick/replay format. Keeping that boundary
avoids introducing a second tick system before reduced and abstract state
representations exist.

Recommended sequence:

1. Add persistent world-agent identities and decide which campaign data may
   supply the six public importance factors.
2. Define REDUCED and ABSTRACT integer update kernels and compare their
   deterministic outputs against explicit reference scenarios.
3. Define AGGREGATE and DORMANT activation/rehydration rules, with state
   transitions represented in the campaign event history.
4. Add measured per-backend compute budgets. Keep CPU as the deterministic
   reference; only add GPU/NPU paths after parity and replay behavior are
   specified.
5. Add an authored end-to-end scenario that exercises all LOD levels before
   connecting the policy to live world ticks.

The unit test also plans the specification's mixed scale of 100 FULL and
100,000 ABSTRACT entities. This verifies classification and allocation
capacity, not the performance of 100,000 simulation kernels.
