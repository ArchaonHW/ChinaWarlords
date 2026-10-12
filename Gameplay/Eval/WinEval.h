#pragma once

#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h" // TICK_RATE_HZ

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Gameplay {

class JsonValue;

// Win Evaluation — the Aftermath verdict (GDD "Win/Loss Conditions",
// E0 Story 1.12). When Execution closes, CloseBattle evaluates the
// final squad states into a BattleResult: victor, casualties, and a
// ledger-event list Epic 2's five-account ledger posts verbatim.
// The battle layer reports; defeat conversion is the campaign's call.

// Why the battle ended — wire-format stable ordinals, carried as
// SimEvent::aux on ResultDeclared events.
enum class CloseReason : std::uint8_t {
    Wipe = 0,      // a side ran out of effective squads
    Concede = 1,   // host requested Aftermath mid-Execution
    Stalemate = 2, // timer fired with both sides still standing
};

// A typed posting derived from the outcome — Epic 2's double-entry
// ledger consumes these (Casualty/Rout per squad, verdict last).
struct LedgerEvent {
    enum class Type : std::uint8_t { Victory, Draw, Casualty, Rout };

    Type type = Type::Draw;
    int side = -1;
    int squadIndex = -1;   // unique key within the battle (deploy order)
    std::string squadId;   // display/template id — NOT unique (same
                           // template may deploy twice)
};

// Emitted once when Execution -> Aftermath closes the battle.
// `forced` distinguishes a manual RequestBeat(Aftermath) concede;
// `stalemate` marks the timer-driven draw evaluation. winnerSide==-1
// means no victor. NOTE: a concede can still report a victor — the
// battle reports the field truthfully (e.g. conceding into an
// already-empty enemy side yields winnerSide set + forced); the
// campaign layer decides what a conceded victory means.
struct BattleResult {
    CloseReason closeReason = CloseReason::Wipe;
    int winnerSide = -1;
    bool forced = false;            // == (closeReason == Concede)
    bool stalemate = false;         // == (closeReason == Stalemate)
    std::uint64_t elapsedTicks = 0; // ticks actually executed
    int effective[2] = {0, 0};      // includes Routing (still on-field)
    int routed[2] = {0, 0};         // survived — RefitCamp candidates
    int destroyed[2] = {0, 0};      // casualties
    std::vector<LedgerEvent> ledger;
};

// "eval" object inside potato.balance/1.
struct EvalConfig {
    // Execution auto-closes as a draw evaluation at this tick with both
    // sides alive — the safety valve against unbounded battles.
    // 0 disables the timer. Default 6 minutes at TICK_RATE_HZ.
    int stalemateTicks = 6 * 60 * TICK_RATE_HZ;

    static Result<EvalConfig> FromJson(const JsonValue& root);
};

} // namespace Potato::Gameplay
