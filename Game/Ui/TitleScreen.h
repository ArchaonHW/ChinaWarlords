#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Potato::Campaign {
class SaveSystem;
}

namespace Potato::Game {

// Title screen (Story 13.3 — GAME_UI_UX_DESIGN.md S0): the game's
// front door. Same two-phase convention as HudView — BuildTitleLayout
// is the pure, headless-testable widget model (hit rects + enabled
// flags); the shell paints it and maps hits to actions.
//
//   ┌──────────── 民國史詩 ────────────┐
//   │        (masthead band)          │
//   ├──────────────────────────────────┤
//   │  繼續戰役        (disabled when  │
//   │    slot 0 — 指揮官 · Day 12      │
//   │    slot 3 — 某某    · Day 4      │
//   │  新戰役                          │
//   │  設定                            │
//   │  離開                            │
//   └──────────────────────────────────┘
//
// The ambient backdrop (a RenderOverworld of the newest save's
// world — the world's first impression) is the SHELL's concern;
// this layout only owns the foreground chrome.

enum class TitleCell : std::uint8_t {
    Masthead,  // decorative title band — presentation paints text
    MenuItem,  // aux = TitleAction ordinal
    SlotRow,   // aux = save slot index
};

enum class TitleAction : std::uint8_t {
    Continue = 0, // resumes the selected save slot
    NewGame,
    Settings,
    Quit,
};

struct TitleWidget {
    TitleCell kind = TitleCell::MenuItem;
    int x = 0, y = 0, w = 0, h = 0; // hit rect
    bool enabled = true;            // dims, never vanishes
    int aux = 0;                    // action ordinal / slot index
};

// One row per PRESENT slot (SaveSystem::ListSlots only reports
// occupied slots — the title screen never shows empty slots).
struct TitleSlotInfo {
    int slot = 0;
    std::string label;   // "指揮官 · Day N", or "—" unlabeled
    bool occupied = false;
};

struct TitleLayout {
    int w = 0, h = 0;
    std::vector<TitleWidget> widgets;
    // Last widget wins — paint order = z order (HudLayout rule).
    const TitleWidget* Hit(int x, int y) const;
};

struct TitleInput {
    int w = 960, h = 600;
    std::span<const TitleSlotInfo> slots;
    // Which slot Continue resumes — the caller's selection state;
    // the menu item is enabled only while a slot is selected.
    int selectedSlot = -1;
};

// Pure layout: masthead + the four menu items + one SlotRow per
// present slot, nested under 繼續戰役. 繼續戰役 dims entirely when
// no slot is selected (the design's "disabled, not hidden" rule).
TitleLayout BuildTitleLayout(const TitleInput& in);

// The data seam for the slots: enumerate the save dir, load each
// present slot, and label it "指揮官 · Day N" (commander or world
// absent → the "—" placeholders). A slot that fails to load keeps
// its row — a corrupt save must not silently vanish, so the row
// labels itself 檔案損毀 and stays selectable for inspection/
// overwrite decisions downstream.
std::vector<TitleSlotInfo> TitleSlots(
    const Campaign::SaveSystem& saves);

} // namespace Potato::Game
