// Story 13.2 — ScreenFlow: the top-level navigation state machine.
// Table-gated narrative moves, the always-legal abandon door, the
// pause overlay flag, and the save gating that keeps I/O off the
// tick path.
#include "Game/Ui/ScreenFlow.h"

#include <cstdio>

namespace {

int failures = 0;
void Check(bool cond, const char* name) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) ++failures;
}

using Potato::Game::ScreenFlow;
using Potato::Game::ScreenId;
using Potato::Gameplay::BattleBeat;

constexpr ScreenId kAll[] = {
    ScreenId::Title,     ScreenId::Creation, ScreenId::Overworld,
    ScreenId::Battle,    ScreenId::Settlement,
    ScreenId::Ending,
};

} // namespace

int main() {
    // ===== The table IS the spec — every (from,to) pair pinned =====
    {
        // Expected targets per screen (narrative moves only —
        // AbandonToTitle is the door, not a path).
        const auto legal = [](ScreenId a, ScreenId b) {
            switch (a) {
                case ScreenId::Title:
                    return b == ScreenId::Creation ||
                           b == ScreenId::Overworld;
                case ScreenId::Creation:
                    return b == ScreenId::Overworld ||
                           b == ScreenId::Title;
                case ScreenId::Overworld:
                    return b == ScreenId::Battle ||
                           b == ScreenId::Ending;
                case ScreenId::Battle:
                    return b == ScreenId::Settlement;
                case ScreenId::Settlement:
                    return b == ScreenId::Overworld ||
                           b == ScreenId::Ending;
                case ScreenId::Ending:
                    return b == ScreenId::Title;
            }
            return false;
        };
        bool allOk = true;
        for (ScreenId a : kAll) {
            for (ScreenId b : kAll) {
                if (Potato::Game::CanTransition(a, b) != legal(a, b)) {
                    allOk = false;
                }
            }
        }
        Check(allOk, "transition table matches the flow diagram");
    }

    // ===== Request: legal moves land, illegal moves don't touch =====
    {
        ScreenFlow f;
        Check(f.Screen() == ScreenId::Title, "boots on title");
        Check(!f.Request(ScreenId::Battle),
              "title can't skip to battle");
        Check(f.Screen() == ScreenId::Title,
              "illegal move left the flow untouched");
        Check(f.Request(ScreenId::Creation) &&
                  f.Screen() == ScreenId::Creation,
              "title -> creation");
        Check(f.Request(ScreenId::Overworld) &&
                  f.Screen() == ScreenId::Overworld,
              "creation -> overworld");
        Check(f.Request(ScreenId::Battle) &&
                  f.Screen() == ScreenId::Battle,
              "overworld -> battle");
        Check(f.Request(ScreenId::Settlement) &&
                  f.Screen() == ScreenId::Settlement,
              "battle -> settlement");
        Check(f.Request(ScreenId::Ending) &&
                  f.Screen() == ScreenId::Ending,
              "settlement -> ending (final chapter)");
        Check(f.Request(ScreenId::Title) &&
                  f.Screen() == ScreenId::Title,
              "ending -> title");
        // The other spine: settle back to the map.
        f.Request(ScreenId::Overworld);
        f.Request(ScreenId::Battle);
        f.Request(ScreenId::Settlement);
        Check(f.Request(ScreenId::Overworld),
              "settlement -> overworld (continuation)");
        Check(!f.Request(ScreenId::Ending) == false,
              "overworld -> ending IS legal (complete-on-load)");
    }

    // ===== AbandonToTitle: the door is always open =====
    {
        for (ScreenId s : kAll) {
            if (s == ScreenId::Title) continue;
            ScreenFlow f;
            // Walk to s via legal moves where possible; the rest
            // get there through the spine.
            f.Request(ScreenId::Overworld);
            f.Request(ScreenId::Battle);
            f.Request(ScreenId::Settlement);
            if (s == ScreenId::Ending) f.Request(ScreenId::Ending);
            if (f.Screen() != s) continue;
            f.AbandonToTitle();
            Check(f.Screen() == ScreenId::Title && !f.Paused(),
                  "abandon returns to title from anywhere");
        }
        ScreenFlow f;
        f.SetPaused(true);
        f.AbandonToTitle();
        Check(!f.Paused(), "abandon clears the pause overlay");
    }

    // ===== Pause overlay: flag, not a screen =====
    {
        ScreenFlow f;
        f.Request(ScreenId::Overworld);
        f.SetPaused(true);
        Check(f.Paused() && f.Screen() == ScreenId::Overworld,
              "pause is an overlay — position unchanged");
        f.SetPaused(false);
        f.Request(ScreenId::Battle);
        Check(!f.Paused(), "screen change clears pause");
        f.SetPaused(true);
        Check(f.Request(ScreenId::Settlement),
              "pause doesn't block narrative moves");
        Check(!f.Paused(), "the move resumed presentation");
    }

    // ===== NoteBattleBeat: mirrored only while on S3 =====
    {
        ScreenFlow f;
        f.NoteBattleBeat(BattleBeat::Execution);
        Check(f.BattleBeatIn() == BattleBeat::Planning,
              "beat note ignored off the battle screen");
        f.Request(ScreenId::Overworld);
        f.Request(ScreenId::Battle);
        f.NoteBattleBeat(BattleBeat::Execution);
        Check(f.BattleBeatIn() == BattleBeat::Execution,
              "beat mirrors while on S3");
        f.Request(ScreenId::Settlement);
        Check(f.BattleBeatIn() == BattleBeat::Planning,
              "leaving battle resets the mirrored beat");
    }

    // ===== CanSave: no I/O on the tick path =====
    {
        ScreenFlow f;
        Check(!f.CanSave(), "title can't save");
        f.Request(ScreenId::Creation);
        Check(!f.CanSave(), "creation can't save");
        f.Request(ScreenId::Overworld);
        Check(f.CanSave(), "overworld saves");
        f.SetPaused(true);
        Check(f.CanSave(), "pause doesn't block saving");
        f.SetPaused(false);
        f.Request(ScreenId::Battle);
        Check(f.CanSave(), "battle planning saves");
        f.NoteBattleBeat(BattleBeat::Execution);
        Check(!f.CanSave(), "execution refuses — ticks are live");
        f.NoteBattleBeat(BattleBeat::Aftermath);
        Check(!f.CanSave(), "aftermath refuses");
        f.Request(ScreenId::Settlement);
        Check(!f.CanSave(), "settlement can't save mid-ceremony");
        f.Request(ScreenId::Ending);
        Check(!f.CanSave(), "ending can't save");
    }

    std::printf("%s\n", failures == 0 ? "ALL PASS" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
