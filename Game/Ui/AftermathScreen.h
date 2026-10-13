#pragma once

#include "Campaign/Governance/Victory.h" // ChapterResolution

#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Campaign {
struct AftermathRow;
struct EncounterAssembly;
struct EncounterSettlement;
} // namespace Potato::Campaign

namespace Potato::Gameplay {
class BattleController;
} // namespace Potato::Gameplay

namespace Potato::Game {

// Aftermath screen (Story 13.7 — GAME_UI_UX_DESIGN.md S3c/S3d):
// the two pieces of chrome around the settle ceremony.
//
//   S3c — the report page owns the whole screen (史官體
//   chronicle, ImGui text layer) with ONE commit: 結算.
//   S3d — a summary strip between report and overworld: the
//   ledger's verdict, roster cost, deeds posted. No "game over"
//   — a lost field still settles (4.5 converted defeat).
//
// Everything here is a fold of objects the settle ceremony
// already produced — presentation reads outcomes, never
// recomputes them.

// CJK verdict for the strip header — ResolutionName stays the
// wire spelling; this is the display vocabulary.
const char* ResolutionLabel(Campaign::ChapterResolution r);

// The "caller maps index → name at deployment" contract
// (Roster.h): folds the marshaled deploy rows against the
// controller's closing squad states into roster AftermathRows.
// Deploy order IS squad-index order; defender rows and any
// nameless deploy produce no roster row. Casualties = template
// hp minus closing hp (clamped at 0); wiped = Destroyed.
std::vector<Campaign::AftermathRow> CasualtyReport(
    const Gameplay::BattleController& bc,
    const Campaign::EncounterAssembly& a, int playerSide = 0);

// The S3d strip — verdict header + one row per settle outcome.
// Values are preformatted strings (the strip is a label list,
// not a table).
struct SettleRow {
    std::string label;
    std::string value;
};
struct SettlementStrip {
    std::string verdict;           // ResolutionLabel output
    std::vector<SettleRow> rows;   // roster/deeds/myth/control
};
SettlementStrip BuildSettlementStrip(
    const Campaign::EncounterSettlement& s);

// The S3c page — chronicle text + the lone commit widget (hit
// rect, same convention as TitleWidget/HudWidget). The settle
// button dims until the host has the settle call staged (a
// report you can't close shouldn't pretend you can).
struct ReportPage {
    std::string text;    // RenderBattleReport output, verbatim
    int settleX = 0, settleY = 0, settleW = 0, settleH = 0;
    bool settleEnabled = false;
    bool HitSettle(int x, int y) const {
        return settleEnabled && x >= settleX &&
               x < settleX + settleW && y >= settleY &&
               y < settleY + settleH;
    }
};
ReportPage BuildReportPage(std::string chronicle,
                           bool settleReady, int w, int h);

} // namespace Potato::Game
