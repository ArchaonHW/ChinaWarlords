#include "Game/Ui/AftermathScreen.h"

#include "Campaign/Roster/Roster.h"
#include "Campaign/World/Encounter.h"
#include "Campaign/World/EncounterSettle.h"
#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Squad/Squad.h"

#include <algorithm>

namespace Potato::Game {

const char* ResolutionLabel(Campaign::ChapterResolution r) {
    switch (r) {
        case Campaign::ChapterResolution::BattleVictory:
            return "戰勝";
        case Campaign::ChapterResolution::GovernanceVictory:
            return "治勝";
        case Campaign::ChapterResolution::Defeat:
            return "敗績";
        case Campaign::ChapterResolution::Subversion:
            return "屈敵";
    }
    return "?";
}

std::vector<Campaign::AftermathRow> CasualtyReport(
    const Gameplay::BattleController& bc,
    const Campaign::EncounterAssembly& a, int playerSide) {
    std::vector<Campaign::AftermathRow> rows;
    const auto& squads = bc.Squads();
    const std::size_t n =
        std::min(a.rows.size(), squads.size());
    for (std::size_t i = 0; i < n; ++i) {
        const Campaign::DeployRow& d = a.rows[i];
        if (d.side != playerSide || d.rosterName.empty()) {
            continue;
        }
        const Gameplay::Squad& s = squads[i];
        Campaign::AftermathRow r;
        r.name = d.rosterName;
        const int maxHp = d.tmpl != nullptr ? d.tmpl->hp : 0;
        r.casualties =
            std::max<std::int64_t>(0, maxHp - s.hp);
        r.wiped = s.state == Gameplay::SquadState::Destroyed;
        rows.push_back(std::move(r));
    }
    return rows;
}

SettlementStrip BuildSettlementStrip(
    const Campaign::EncounterSettlement& s) {
    SettlementStrip strip;
    strip.verdict = ResolutionLabel(s.resolution);
    auto row = [&strip](const char* label,
                        const std::string& value) {
        strip.rows.push_back({label, value});
    };
    row("名冊", std::to_string(s.roster.rows) + " 隊已記 / " +
                    std::to_string(s.roster.buried) + " 隊埋葬 / " +
                    std::to_string(s.roster.veterans) + " 隊歷戰");
    row("事蹟", std::to_string(s.deedsPosted) + " 則入帳");
    if (s.mythLogged > 0) {
        row("異聞", std::to_string(s.mythLogged) + " 則入誌");
    }
    if (s.controlFlipped) {
        row("控制", "旗幟易幟");
    }
    return strip;
}

ReportPage BuildReportPage(std::string chronicle,
                           bool settleReady, int w, int h) {
    ReportPage page;
    page.text = std::move(chronicle);
    // Bottom-right commit — 結算 → (returns to the overworld;
    // the settlement strip rides between).
    page.settleW = 120;
    page.settleH = 28;
    page.settleX = w - page.settleW - 16;
    page.settleY = h - page.settleH - 16;
    page.settleEnabled = settleReady;
    return page;
}

bool PresentSettlementResult(
    ScreenFlow& flow, ActionNotice& notice,
    const Gameplay::Result<Campaign::EncounterSettlement>& result) {
    if (flow.Screen() != ScreenId::Battle) {
        const auto rejected = Gameplay::Fail<
            Campaign::EncounterSettlement>(
                "screen", "目前畫面不可結算戰役");
        ShowActionResult(notice, rejected, "結算");
        return false;
    }

    ShowActionResult(notice, result, "結算");
    if (!result.ok()) return false;
    return flow.Request(ScreenId::Settlement);
}

} // namespace Potato::Game
