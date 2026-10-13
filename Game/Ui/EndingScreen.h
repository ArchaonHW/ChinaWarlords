#pragma once

#include "Campaign/Narrative/EndingPage.h" // EndingVoice

#include <string>

namespace Potato::Campaign {
class Ledger;
}

namespace Potato::Game {

// Ending folio screen (Story 13.9 — GAME_UI_UX_DESIGN.md S4):
// the campaign's last page. One read of the ledger's final
// state — four voices, no selection surface — plus the lone
// commit back to the title screen. The verdict is a pure fold
// of ledger truth; this view-model only owns the chrome.

// CJK folio title per voice (信史/疑史/偽史/絕筆) — display
// vocabulary; EndingVoiceName stays the wire id.
const char* EndingVoiceTitle(Campaign::EndingVoice v);

struct EndingPage {
    Campaign::EndingVoice voice =
        Campaign::EndingVoice::Believed;
    std::string title;   // EndingVoiceTitle output
    std::string text;    // RenderEndingPage output, verbatim
    // Bottom-right return — 回標題.
    int returnX = 0, returnY = 0, returnW = 0, returnH = 0;
    bool HitReturn(int x, int y) const {
        return x >= returnX && x < returnX + returnW &&
               y >= returnY && y < returnY + returnH;
    }
};

// One fold → one folio + the return widget.
EndingPage BuildEndingPage(const Campaign::Ledger& l,
                           int w, int h);

} // namespace Potato::Game
