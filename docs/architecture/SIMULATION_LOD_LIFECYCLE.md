# Hierarchical simulation LOD lifecycle contract

## Status

This document defines the safety boundary for a future live LOD scheduler. It
does not add a scheduler or change battle ticks, campaign saves, or replay
records. The existing policy, macro kernels, and snapshot transforms remain
standalone until the open integration decisions below are resolved.

## Invariants

- LOD planning is deterministic and uses stable unsigned agent IDs. Input
  container order must not affect an assignment or transition plan.
- LOD transitions run at an explicit boundary between simulation updates. They
  never run partway through a 20 Hz `BattleController` tick.
- All state needed for a transition is validated and prepared before the
  current state is replaced. A rejected transition leaves the current state,
  clocks, and event history unchanged.
- Campaign may provide only public or observed factors and locations. Battle
  simulation state and true enemy positions remain owned by `Gameplay/Sim/`.
- Simulation updates use integer state and the existing seeded PRNG contract.
  A scheduler does not add a second random stream or floating-point decisions.

## State and clock rules

Each active snapshot has one elapsed-tick clock. A roster may be projected to
ABSTRACT only when all members share that clock. Rosters with different clocks
remain separate; a scheduler must not silently synchronize or merge them.

DORMANT retains the last authoritative simulation snapshot and its clock. It
does not advance while dormant. Reactivation restores that retained snapshot;
it does not simulate missed ticks or apply an invented catch-up interval.

`ProjectToAbstract` and `RehydrateFromAbstract` define the existing conversion
rules: stable-ID ordering, exact totals, and deterministic even distribution
with rotating remainders. Rehydrated member values are reconstructed values,
not recovered originals. The lifecycle must preserve this distinction when
choosing which state becomes authoritative after reactivation.

## Transition boundaries

| Transition | Defined behavior | Integration still required |
| --- | --- | --- |
| Same tier → same tier | No state change and no transition event. | None. |
| Active tier → DORMANT | Retain the complete tier snapshot and clock; stop advancing it. | Define the owned snapshot for each live tier. |
| DORMANT → active tier | Restore the retained snapshot without catch-up ticks. | Define how changed policy factors select the target tier while retaining state. |
| REDUCED roster → ABSTRACT | Use the existing member snapshot projection and exact integer totals. | Bind stable IDs and one shared clock to the live REDUCED state. |
| ABSTRACT → REDUCED roster | Use the existing deterministic rehydration distribution. | Define when reconstructed values become authoritative. |
| FULL ↔ REDUCED/ABSTRACT | No conversion is specified yet. | Define a full battle-agent snapshot and loss/rounding semantics first. |
| Any tier → AGGREGATE | No battle-state conversion is specified. Campaign AGGREGATE summaries remain public-factor summaries only. | Define province-level combat semantics before simulating or settling aggregate combat. |

Budget overflow continues to use the pure policy's deterministic tier
degradation. Budget selection alone must never discard the only authoritative
snapshot for an agent.

## Replay and persistence boundary

The current LOD policy, kernels, and snapshot transforms are not part of the
battle tick or replay format. Keep them outside `BattleController` until a
transition can be represented by deterministic, replayable input and its
result is covered by the battle checksum. Campaign `lod_runtime` history is
not a substitute for simulation events and must not be used to reconstruct
hidden battle state.

No simulation snapshots are added to campaign JSON by this contract. Any
future save format change requires a versioned schema, rejection without
mutating live state, and atomic temp-file-plus-rename writes.

## Implementation gate

Before connecting snapshots to live ticks, resolve and test these decisions:

1. Map stable campaign world-agent IDs to live battle entities without
   exposing hidden enemy state to Campaign or presentation.
2. Define the FULL snapshot fields and the exact FULL ↔ macro-state projection,
   including any information loss and integer rounding.
3. Choose the scheduler boundary relative to battle ticks and specify which
   transitions become recorded replay events and checksum inputs.
4. Define AGGREGATE combat, attrition, and settlement behavior, or keep
   AGGREGATE as a campaign-only summary until those rules exist.
5. Add headless tests for every supported transition, input-order invariance,
   clock preservation, dormant freeze/reactivation, atomic rejection, replay
   reproduction, and unchanged state after invalid plans.

Only transitions whose source state, target state, timing, event recording,
and failure behavior are fully specified should enter the live scheduler.
