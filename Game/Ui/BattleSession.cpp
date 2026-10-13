#include "Game/Ui/BattleSession.h"

#include "Gameplay/AI/BattleAI.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Eval/WinEval.h"

namespace Potato::Game {

BattleSession::BattleSession(std::uint64_t seed,
                             const Gameplay::BattleMap& map,
                             const Gameplay::DoctrineLibrary& cards,
                             const Gameplay::FogConfig& fog,
                             const Gameplay::PlanConfig& plan,
                             const Gameplay::EvalConfig& eval)
    : bc_(seed, map, cards, fog, plan, eval) {}

int BattleSession::PumpMs(std::int64_t elapsedMs) {
    if (bc_.Beat() != Gameplay::BattleBeat::Execution) {
        carryMs_ = 0;
        return 0;
    }
    carryMs_ += elapsedMs;
    int ticks = static_cast<int>(carryMs_ / TICK_MS);
    carryMs_ -= static_cast<std::int64_t>(ticks) * TICK_MS;
    int ran = 0;
    for (; ran < ticks; ++ran) {
        if (!bc_.Tick()) break; // battle closed mid-pump
        // The opponent's turn — sim-time cadence: N pump splits
        // of the same elapsed span land on the same ticks.
        if (foe_ != nullptr && foePeriod_ > 0 &&
            bc_.Beat() == Gameplay::BattleBeat::Execution &&
            bc_.GetSim().TickCount() %
                    static_cast<std::uint64_t>(foePeriod_) ==
                0) {
            foe_->Act(bc_);
        }
    }
    if (bc_.Beat() != Gameplay::BattleBeat::Execution) {
        carryMs_ = 0;
    }
    return ran;
}

} // namespace Potato::Game
