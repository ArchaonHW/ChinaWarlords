#include "Game/Ui/PauseOverlay.h"

namespace Potato::Game {

const PauseWidget* PauseLayout::Hit(int x, int y) const {
    for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
        if (x >= it->x && x < it->x + it->w && y >= it->y &&
            y < it->y + it->h) {
            return &*it;
        }
    }
    return nullptr;
}

PauseLayout BuildPauseLayout(const PauseInput& in) {
    PauseLayout l;
    l.w = in.w;
    l.h = in.h;

    // Centered panel — the overlay covers the screen; the panel
    // is the interactive band.
    const int panelW = 320;
    const int x0 = (in.w - panelW) / 2;
    const int rowH = 36;
    const int gap = 8;
    int y = 120;

    auto add = [&l, &y, x0, panelW, rowH, gap](
                   PauseAction a, bool enabled, int aux) {
        PauseWidget w;
        w.action = a;
        w.x = x0;
        w.y = y;
        w.w = panelW;
        w.h = rowH;
        w.enabled = enabled;
        w.aux = aux;
        l.widgets.push_back(w);
        y += rowH + gap;
    };

    add(PauseAction::Resume, true, 0);
    add(PauseAction::Save, in.canSave, 0);
    // Load rows — present slots only, indented under 讀檔.
    for (const TitleSlotInfo& s : in.slots) {
        add(PauseAction::LoadSlot, true, s.slot);
    }
    add(PauseAction::Settings, true, 0);
    add(PauseAction::QuitToTitle, true, 0);
    add(PauseAction::Quit, true, 0);
    return l;
}

} // namespace Potato::Game
