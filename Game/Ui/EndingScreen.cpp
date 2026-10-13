#include "Game/Ui/EndingScreen.h"

#include "Campaign/Ledger/Ledger.h"

namespace Potato::Game {

const char* EndingVoiceTitle(Campaign::EndingVoice v) {
    switch (v) {
        case Campaign::EndingVoice::Believed:  return "信史";
        case Campaign::EndingVoice::Doubted:   return "疑史";
        case Campaign::EndingVoice::Forged:    return "偽史";
        case Campaign::EndingVoice::Abandoned: return "絕筆";
    }
    return "?";
}

EndingPage BuildEndingPage(const Campaign::Ledger& l,
                           int w, int h) {
    EndingPage page;
    page.voice = Campaign::FoldEndingVoice(l);
    page.title = EndingVoiceTitle(page.voice);
    page.text = Campaign::RenderEndingPage(l);
    page.returnW = 120;
    page.returnH = 28;
    page.returnX = w - page.returnW - 16;
    page.returnY = h - page.returnH - 16;
    return page;
}

} // namespace Potato::Game
