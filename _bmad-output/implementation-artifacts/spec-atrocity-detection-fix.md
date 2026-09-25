---
title: 'Fix unreachable GovernanceEvent::Atrocity branch (routed-enemy massacre)'
type: 'bugfix'
created: '2026-09-25'
status: 'in-review'
review_loop_iteration: 0
followup_review_recommended: false
context:
  - '{project-root}/_bmad-output/implementation-artifacts/deferred-work.md'
warnings: []
deferred: []
---

<intent-contract>

## Intent

**Problem:** `BattleController::DetectGovernanceEvents` contains an
`Atrocity` ledger branch (routing enemy squad loses members = massacring
surrendered troops) that is dead code: `ResolveCombat` skips routing
targets entirely (`b->IsRouting() → continue`), and `Intervene(Engage)`
rejects routing targets, so a routing squad can never lose members.
Additionally a killing blow clears `Squad::routing` before detection
reads it, so even a wiped-out router would be skipped. The ledger
therefore always records mercy (`SurrenderAccepted`) and can never
record atrocity — 治平者無勝's core accountability is severed.

**Approach:** Allow one-way damage onto routing defenders in the combat
pair loop (they already return 0 DPS), consult the rout-member ledger
before the `IsRouting` early-continue so eliminated-after-rout still
counts once, and permit `Intervene(Engage)` on routing targets so
pursuit is a deliberate, attributable player act.

## Boundaries & Constraints

**Always:** keep routing squads' own damage output at 0; do not set the
`engaged` flag on a routing defender (a fleeing unit is not "in combat"
for `anyAllyEngaged` context); each squad contributes at most one
`Atrocity` event (existing `govAtrocityDone` set); non-routing pairs
behave byte-identically (mutual damage, flags, combat width).

**Never:** no auto-retargeting of `AttackNearest`/`AttackWeakest` onto
routers (sparing must stay the default); no `team` convention refactor
(player=0/enemy=1 stays — separate deferred item); no balance tuning of
rout-pursuit lethality; no changes outside `Gameplay/` + one test file.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| PURSUIT_DAMAGE | enemy in range of routing squad, ticks advance | router loses members; one `Atrocity` recorded after `SurrenderAccepted` | none |
| KILLING_BLOW | pursuit damage eliminates router outright | `Atrocity` recorded exactly once despite `routing` flag cleared | none |
| MERCY | routing squad, no enemy in range | zero `Atrocity` events; `SurrenderAccepted` still recorded once | none |
| MANUAL_ENGAGE | `Intervene(s, Engage, routingTarget)` | returns true, spends 1 CP | none |
| REPEATED_TICKS | router damaged over several ticks | `Atrocity` fires exactly once (govAtrocityDone) | none |
| THIRD_PARTY | team>=2 routing squad takes losses | no governance events (existing `team != 1` guard) | none |

</intent-contract>

## Code Map

- `Gameplay/BattleController.cpp` ~L812-853 — `ResolveCombat` pair loop;
  `b->IsRouting()` at ~L819 is the dead-branch gate. Attacker `a` is
  already non-routing (outer skip ~L814).
- `Gameplay/BattleController.cpp` ~L311-332 — `DetectGovernanceEvents`;
  reorder so `govRoutMembers` lookup precedes the `IsRouting` continue.
- `Gameplay/BattleController.cpp` ~L148-173 — `Intervene(Engage, target)`;
  `target->IsRouting()` rejection at ~L154.
- `Gameplay/Squad.cpp` ~L58-62, L84-99 — `GetAttackDPS()` returns 0 when
  routing; `ApplyCasualties` sets `routing = members > 0` (clears flag on
  elimination — the reason detection must be map-first).
- `Gameplay/BattleController.h` — `govRoutMembers`/`govAtrocityDone`
  members, `GovernanceEvent` enum + `GovernanceEventName`.
- `Examples/BattleControllerTest.cpp` — check for existing rout/combat
  fixtures to extend (fallback: `BattleSandboxTest` pattern).

## Tasks & Acceptance

**Execution:**
- `Gameplay/BattleController.cpp` -- ResolveCombat: when `b` routing and
  `a` is a healthy visible enemy in range, buffer a's DPS onto b
  (width-capped), `b->SetUnderAttack(true)`, skip mutual-engage flags --
  makes pursuit damage possible.
- `Gameplay/BattleController.cpp` -- DetectGovernanceEvents: map-first
  ordering so members-drop and post-rout elimination both fire Atrocity
  once -- restores the branch.
- `Gameplay/BattleController.cpp` -- Intervene(Engage): remove
  `target->IsRouting()` rejection -- deliberate pursuit stays legal.
- `Examples/BattleControllerTest.cpp` -- add the six I/O matrix cases --
  regression guard for the ledger path.

**Acceptance Criteria:**
- Given a routing enemy with a healthy foe in engage range, when ticks
  run, then the router loses members and exactly one Atrocity event is
  recorded (after its SurrenderAccepted).
- Given a router eliminated outright by pursuit, when detection runs,
  then Atrocity is still recorded exactly once.
- Given a router with no enemy in range, when ticks run, then no
  Atrocity is recorded.
- Given `Intervene(s, Engage, routingTarget)`, it returns true and
  spends a command point.
- Given all of the above under fixed dt, results are deterministic and
  non-routing combat behavior is unchanged.

## Design Notes

One-way damage is modeled by keeping the mutual-engage block for
non-routing pairs and adding a routing-defender early branch: `a` still
accumulates damage onto `b`, but `b`'s return-fire write is skipped
implicitly (its DPS is already 0) and neither `engaged` flag is set on
`b`. Engage orders on routers survive the fog-expiry sweep because the
target stays revealed-relevant only when visible; acceptable — pursuit
of a fleeing unit needs sightline anyway.

## Verification

**Commands:**
- `mingw32-make BattleControllerTest -C build-mingw -j4 && ./build-mingw/bin/BattleControllerTest.exe` (from bin dir) -- ALL PASS
- `cmake --build build --config Release --target BattleControllerTest && cd build && ctest -C Release -R BattleController` -- pass
- `grep -nE '\b(gets|strcpy|strcat|sprintf|vsprintf|scanf)\s*\(' Gameplay/BattleController.cpp` -- no hits
