#pragma once

#include "Game/Ui/TitleScreen.h" // TitleSlotInfo — the slot-row model

#include <cstdint>
#include <span>
#include <vector>

namespace Potato::Game {

// Pause overlay (Story 13.8 — GAME_UI_UX_DESIGN.md 全域覆層):
// Esc floats this over ANY screen — it is an overlay, never a
// ScreenId (ScreenFlow keeps the flag; this is the widget model
// the shell paints and hit-tests).
//
//   ┌─ overlay ──────────┐
//   │  繼續              │
//   │  存檔 (dimmed off  │   save is legal only where the flow
//   │  S2/S3a — CanSave) │   says state is quiescent: Overworld
//   │  讀檔              │   or Battle-Planning — the tick path
//   │    slot 0 — …      │   never writes files (NFR).
//   │    slot 3 — …      │
//   │  設定              │
//   │  回標題            │
//   │  離開              │
//   └────────────────────┘

enum class PauseAction : std::uint8_t {
    Resume = 0,     // close the overlay
    Save,           // CanSave-gated — dims, never vanishes
    LoadSlot,       // aux = save slot index
    Settings,
    QuitToTitle,    // abandon — the door, not a path
    Quit,
};

struct PauseWidget {
    PauseAction action = PauseAction::Resume;
    int x = 0, y = 0, w = 0, h = 0;
    bool enabled = true;
    int aux = 0; // slot index for LoadSlot rows
};

struct PauseLayout {
    int w = 0, h = 0;
    std::vector<PauseWidget> widgets;
    // Last widget wins — paint order = z order.
    const PauseWidget* Hit(int x, int y) const;
};

struct PauseInput {
    int w = 960, h = 600;
    bool canSave = false; // the host feeds ScreenFlow::CanSave()
    // Present save slots (TitleSlots' scan — same model the
    // title screen shows; a corrupt slot keeps its row).
    std::span<const TitleSlotInfo> slots;
};

// Pure layout — one widget per action + one LoadSlot row per
// present slot. Save dims when the flow forbids it (Execution,
// Aftermath, Settlement, Title, Creation, Ending all dim; only
// Overworld and Battle-Planning light it).
PauseLayout BuildPauseLayout(const PauseInput& in);

} // namespace Potato::Game
