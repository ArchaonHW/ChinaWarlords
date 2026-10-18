#include "Game/Ui/TitleScreen.h"

#include "Campaign/Save/SaveSystem.h"
#include "Campaign/State/CampaignState.h"

namespace Potato::Game {

const TitleWidget* TitleLayout::Hit(int x, int y) const {
    // Last widget wins — paint order = z order.
    for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
        const TitleWidget& w = *it;
        if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) {
            return &w;
        }
    }
    return nullptr;
}

namespace {

constexpr int kItemW = 200;
constexpr int kItemH = 36;
constexpr int kSlotH = 26;
constexpr int kGap = 8;

} // namespace

TitleLayout BuildTitleLayout(const TitleInput& in) {
    TitleLayout lay;
    lay.w = in.w;
    lay.h = in.h;

    // Masthead band: the presentation layer paints ChinaWarlords,
    // 民國史詩, and the subtitle inside this rect.
    lay.widgets.push_back(
        {TitleCell::Masthead, 0, in.h / 12, in.w, 96, true, 0});

    const int cx = in.w / 2 - kItemW / 2;
    int y = in.h * 2 / 5;

    // 繼續戰役 — the whole item dims when nothing can resume
    // (design rule: disabled, not hidden).
    const bool canContinue =
        in.selectedSlot >= 0 &&
        [&in] {
            for (const TitleSlotInfo& s : in.slots) {
                if (s.occupied && s.slot == in.selectedSlot) {
                    return true;
                }
            }
            return false;
        }();
    lay.widgets.push_back({TitleCell::MenuItem, cx, y, kItemW,
                           kItemH, canContinue,
                           static_cast<int>(TitleAction::Continue)});
    y += kItemH + 2;

    // Save rows nest under the menu item, indented — same enable
    // rule (a row for a missing slot can't be picked).
    for (const TitleSlotInfo& s : in.slots) {
        lay.widgets.push_back(
            {TitleCell::SlotRow, cx + 20, y, kItemW - 20, kSlotH,
             s.occupied, s.slot});
        y += kSlotH + 2;
    }
    y += kGap;

    for (const TitleAction a :
         {TitleAction::NewGame, TitleAction::Settings,
          TitleAction::Quit}) {
        lay.widgets.push_back({TitleCell::MenuItem, cx, y, kItemW,
                               kItemH, true, static_cast<int>(a)});
        y += kItemH + kGap;
    }
    return lay;
}

std::vector<TitleSlotInfo> TitleSlots(
    const Campaign::SaveSystem& saves) {
    std::vector<TitleSlotInfo> out;
    for (const int s : saves.ListSlots()) {
        TitleSlotInfo info;
        info.slot = s;
        info.occupied = true;
        const auto st = saves.Load(s);
        if (st.ok()) {
            std::string label =
                st.value.HasCharacter()
                    ? st.value.GetCharacter().name
                    : std::string("—");
            if (st.value.HasWorld()) {
                label += " · Day " +
                         std::to_string(st.value.GetWorld().Day());
            }
            info.label = std::move(label);
        } else {
            // Corrupt saves keep their row — vanishing silently
            // would hide the only signal the file is bad.
            info.label = "（檔案損毀）";
        }
        out.push_back(std::move(info));
    }
    return out;
}

} // namespace Potato::Game
