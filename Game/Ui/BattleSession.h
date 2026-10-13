#pragma once

#include "Campaign/World/Encounter.h" // EncounterAssembly
#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Sim/Sim.h" // TICK_RATE_HZ

#include <cstdint>

namespace Potato::Gameplay {
class BattleAI;
class BattleMap;
class DoctrineLibrary;
struct BattleResult;
} // namespace Potato::Gameplay

namespace Potato::Game {

// BattleSession (Story 13.6 — GAME_UI_UX_DESIGN.md S3): the
// shell's owner object for one battle. Thin — the sim keeps all
// rules; the session only owns what presentation needs:
//
//   - the BattleController's lifetime (map/cards stay borrowed
//     from the caller's registries — same contract)
//   - the real-time → 20 Hz tick pump for the Execution beat
//     (pause = the caller stops pumping; the sim never sees a
//     pause flag — presentation-only, ScreenFlow's rule)
//   - the deploy/beat-commit seam into Planning input
//
//   marshal (Campaign) → session.Deploy → RequestExecution →
//   PumpMs per frame → Beat()==Aftermath → Outcome/Events hand
//   to the report + settle ceremonies (13.7).

// Whole-tick quantum — 20 Hz presentation pump.
constexpr std::int64_t TICK_MS = 1000 / Gameplay::TICK_RATE_HZ;

class BattleSession {
public:
    // `map`/`cards` must outlive the session (borrowed refs —
    // BattleController's contract). Fog/plan/eval configs ride
    // the assembly's marshaled fog.
    BattleSession(std::uint64_t seed,
                  const Gameplay::BattleMap& map,
                  const Gameplay::DoctrineLibrary& cards,
                  const Gameplay::FogConfig& fog = {},
                  const Gameplay::PlanConfig& plan = {},
                  const Gameplay::EvalConfig& eval = {});

    Gameplay::BattleController& Controller() { return bc_; }
    const Gameplay::BattleController& Controller() const {
        return bc_;
    }

    Gameplay::BattleBeat Beat() const { return bc_.Beat(); }

    // Planning: apply a marshaled encounter's deploy rows
    // (DeployAssembly — must run in Planning; the assembly was
    // prevalidated by MarshalEncounter, so a false return is a
    // caller bug).
    bool Deploy(const Campaign::EncounterAssembly& a) {
        return Campaign::DeployAssembly(bc_, a);
    }

    // The 開戰 commit — Planning→Execution (rejected elsewhere;
    // the shell should gate this behind a confirm step — the
    // transition is irreversible).
    bool RequestExecution() {
        return bc_.RequestBeat(Gameplay::BattleBeat::Execution);
    }

    // Real-time pump: converts elapsed presentation milliseconds
    // into whole 20 Hz ticks, carrying sub-tick remainder
    // forward. Runs zero ticks outside Execution and DISCARDS
    // the carry — a beat change never backdates stale time.
    // Returns the ticks run (0 while Planning/Aftermath/paused).
    int PumpMs(std::int64_t elapsedMs);

    // Symmetric opponent hook (Gameplay/AI/BattleAI): Act() runs
    // INSIDE the tick loop — once every `periodTicks` of sim
    // time — so the AI's cadence is canonical game-time, never
    // frame-rate. A 200 ms frame and four 50 ms frames drive the
    // identical call sequence; presentation speed can never move
    // a command across a tick boundary. Borrowed pointer; the
    // host owns the AI. periodTicks <= 0 detaches.
    void AttachOpponent(Gameplay::BattleAI* ai,
                        int periodTicks) {
        foe_ = ai;
        foePeriod_ = periodTicks;
    }

    // Aftermath read surface — the report/settle seam's input.
    const Gameplay::BattleResult& Outcome() const {
        return bc_.Outcome();
    }
    const std::vector<Gameplay::SimEvent>& Events() const {
        return bc_.Events();
    }

private:
    Gameplay::BattleController bc_;
    Gameplay::BattleAI* foe_ = nullptr;
    int foePeriod_ = 0;
    std::int64_t carryMs_ = 0;
};

} // namespace Potato::Game
