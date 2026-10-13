#pragma once

#include "Gameplay/Sim/BattleController.h" // Gameplay::BattleBeat

#include <cstdint>

namespace Potato::Game {

// Screen flow (Story 13.2 — GAME_UI_UX_DESIGN.md §2): the top-level
// navigation state machine. Same convention as Gameplay::BattleBeat/SquadState —
// the enum + transition table IS the spec, and this header is pure/
// headless so the whole flow is potato_test_*-checkable.
//
//   S0 Title ──new──▶ S1 Creation ──▶ S2 Overworld ◀──┐
//     ▲  └──load──▶ S2 direct          │  ▲          │
//     │                                ▼  │settle    │
//     │                             S3 Battle        │
//     │                             (a Planning      │
//     │                              b Execution     │
//     │                              c Aftermath)    │
//     │                                │  ▼          │
//     │                          S3d Settlement ─────┘
//     │                                │final chapter
//     └────────── S4 Ending ◀──────────┘
//
// The battle's three beats are NOT screens — they ride inside S3 as
// the mirrored Gameplay::BattleBeat the host reports via NoteBattleBeat.
// The Esc pause is an overlay flag, not a screen: it never changes
// the flow position, only presentation.
//
// 回標題 is deliberately NOT a table move: it's an abandon (like
// RequestBeat(Aftermath)'s concede) — always legal from anywhere,
// resets beat + pause. The story moves are the table; the reset is
// a door, not a path.
enum class ScreenId : std::uint8_t {
    Title = 0,
    Creation,
    Overworld,
    Battle,
    Settlement,
    Ending,
};

// Narrative transitions — illegal moves are rejected with no side
// effects (same rule as CanTransition(Gameplay::BattleBeat)).
constexpr bool CanTransition(ScreenId a, ScreenId b) {
    switch (a) {
        case ScreenId::Title:
            return b == ScreenId::Creation || b == ScreenId::Overworld;
        case ScreenId::Creation:
            return b == ScreenId::Overworld || b == ScreenId::Title;
        case ScreenId::Overworld:
            return b == ScreenId::Battle || b == ScreenId::Ending;
        case ScreenId::Battle:
            return b == ScreenId::Settlement;
        case ScreenId::Settlement:
            return b == ScreenId::Overworld || b == ScreenId::Ending;
        case ScreenId::Ending:
            return b == ScreenId::Title;
    }
    return false;
}

// Overlay-independent save rule (design §「全域覆層」): the tick
// path does zero I/O, so saving is only legal where no sim ticks
// run — the overworld, or the battle's planning beat. Execution
// and the aftermath/report screens refuse.
class ScreenFlow {
public:
    ScreenId Screen() const { return screen_; }
    // Meaningful only while Screen() == Battle — the host mirrors
    // the controller's beat each frame via NoteBattleBeat.
    Gameplay::BattleBeat BattleBeatIn() const { return battleBeat_; }
    bool Paused() const { return paused_; }

    // Table-gated narrative move. Closes the pause overlay — a
    // screen change always resumes presentation. Illegal moves
    // reject with no side effects (pause untouched).
    bool Request(ScreenId to) {
        if (!CanTransition(screen_, to)) return false;
        screen_ = to;
        paused_ = false;
        if (to != ScreenId::Battle) {
            battleBeat_ = Gameplay::BattleBeat::Planning; // stale beat must not
        }                                       // leak into the next
        return true;                            // battle's read
    }

    // The reset door — always legal, from every screen. Abandoning
    // the run is a host decision, not a narrative move.
    void AbandonToTitle() {
        screen_ = ScreenId::Title;
        battleBeat_ = Gameplay::BattleBeat::Planning;
        paused_ = false;
    }

    // Host mirrors the battle controller's beat while on S3 — the
    // flow doesn't own sim state, it only records enough to gate
    // the save rule and drive screen transitions.
    void NoteBattleBeat(Gameplay::BattleBeat b) {
        if (screen_ == ScreenId::Battle) battleBeat_ = b;
    }

    void SetPaused(bool p) { paused_ = p; }

    bool CanSave() const {
        return screen_ == ScreenId::Overworld ||
               (screen_ == ScreenId::Battle &&
                battleBeat_ == Gameplay::BattleBeat::Planning);
    }

private:
    ScreenId screen_ = ScreenId::Title;
    Gameplay::BattleBeat battleBeat_ = Gameplay::BattleBeat::Planning;
    bool paused_ = false;
};

} // namespace Potato::Game
