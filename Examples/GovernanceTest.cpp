// GovernanceTest — C-2 治理源驗證（headless）
// 覆蓋：BattleController 自動偵測（受降/暴行各一次）、
// RecordGovernanceEvent 計數+Emit、SageCommand delta 套用、
// Campaign::Governance 折帳/夾值/動亂逐級、存檔 roundtrip。

#include "Campaign/CampaignState.h"
#include "Campaign/Governance.h"
#include "Gameplay/BattleController.h"
#include "Gameplay/SageCommand.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Potato;
using namespace Potato::Gameplay;
using namespace Potato::Campaign;

static int failures = 0;
static void Check(bool cond, const char* name) {
    if (cond) {
        printf("  [PASS] %s\n", name);
    } else {
        printf("  [FAIL] %s\n", name);
        ++failures;
    }
}

// 缺鍵回 0 而非 at() 拋例外——斷言失敗應計 FAIL 不應崩潰
static int EvCount(
    const std::unordered_map<GovernanceEvent, int>& evs,
    GovernanceEvent k) {
    auto it = evs.find(k);
    return it == evs.end() ? 0 : it->second;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== Governance Tests (C-2) ===\n");

    // ---- [1] BattleController 自動偵測：受降/暴行 ----
    printf("\n[1] 自動偵測\n");
    {
        BattleController battle(10, 10, 1.0f);
        std::vector<std::string> emitted;
        battle.SetEventCallback(
            [&](const std::string& s) { emitted.push_back(s); });
        Squad* mine = battle.CreateSquad("我軍", 0, Vector2(1, 1), 10);
        Squad* foe = battle.CreateSquad("敵軍", 1, Vector2(8, 8), 10);
        // 留一隊未潰逃敵軍：foe 全潰會翻勝負讓 Update 提前 no-op
        Squad* foe2 = battle.CreateSquad("敵軍二隊", 1, Vector2(9, 9), 10);
        (void)mine;
        (void)foe2;
        battle.BeginExecution(); // 偵測只在執行階段跑
        battle.Update(0.1f);
        Check(battle.GetGovernanceEvents().empty(),
              "無潰逃無治理事件");

        foe->AdjustMorale(-1.0f); // 士氣歸零 → routing
        Check(foe->IsRouting(), "敵隊已潰逃");
        battle.Update(0.1f);
        auto& evs = battle.GetGovernanceEvents();
        Check(EvCount(evs, GovernanceEvent::SurrenderAccepted) == 1,
              "新潰逃 → SurrenderAccepted×1");
        // 治理事件進 recorder 事件流——只數「治理：」前綴，
        // 排除部署階段的其他 emit
        bool sawGovEmit = false;
        for (const auto& s : emitted) {
            if (s.rfind("治理：", 0) == 0) sawGovEmit = true;
        }
        Check(sawGovEmit, "治理事件進 recorder 事件流");

        battle.Update(0.1f);
        Check(EvCount(evs, GovernanceEvent::SurrenderAccepted) == 1,
              "同隊潰逃不重發");

        // 潰逃中損員 → 暴行（屠殺已降者）
        foe->ApplyCasualties(3);
        battle.Update(0.1f);
        Check(EvCount(evs, GovernanceEvent::Atrocity) == 1,
              "潰逃損員 → Atrocity×1");
        foe->ApplyCasualties(2);
        battle.Update(0.1f);
        Check(EvCount(evs, GovernanceEvent::Atrocity) == 1,
              "暴行同隊只記一次");

        // 我軍潰逃不算敵軍受降
        Squad* mine2 =
            battle.CreateSquad("我軍二隊", 0, Vector2(2, 2), 10);
        mine2->AdjustMorale(-1.0f);
        battle.Update(0.1f);
        Check(EvCount(evs, GovernanceEvent::SurrenderAccepted) == 1,
              "我軍潰逃不計受降");

        // 手動餵入：村莊/護輜（有地圖知識的呼叫端路徑）
        battle.RecordGovernanceEvent(GovernanceEvent::VillageOccupied);
        battle.RecordGovernanceEvent(GovernanceEvent::ConvoyProtected);
        Check(EvCount(evs, GovernanceEvent::VillageOccupied) == 1 &&
                  EvCount(evs, GovernanceEvent::ConvoyProtected) == 1,
              "RecordGovernanceEvent 計數");
    }

    // ---- [2] SageCommand 消費事件 ----
    printf("\n[2] SageCommand delta 套用\n");
    {
        SageCommand sage(0, 1);
        const float s0 = sage.GetPopularSupport();
        const float o0 = sage.GetCivilOrder();
        sage.RecordGovernanceEvent(GovernanceEvent::SurrenderAccepted);
        Check(sage.GetPopularSupport() > s0, "受降民心升");
        Check(sage.GetCivilOrder() > o0, "受降秩序升");
        sage.RecordGovernanceEvent(GovernanceEvent::Atrocity);
        sage.RecordGovernanceEvent(GovernanceEvent::Atrocity);
        sage.RecordGovernanceEvent(GovernanceEvent::Atrocity);
        Check(sage.GetPopularSupport() < s0, "暴行民心降");
        Check(sage.GetCivilOrder() < o0, "暴行秩序降");
        Check(sage.GetPopularSupport() >= 0.0f, "夾值不下零");
    }

    // ---- [3] Campaign::Governance 折帳 ----
    printf("\n[3] 戰役累加器\n");
    {
        Governance gov;
        Check(gov.PopularSupport() == 50.0f &&
                  gov.CivilOrder() == 50.0f,
              "初始 50/50");
        std::unordered_map<GovernanceEvent, int> evs;
        evs[GovernanceEvent::SurrenderAccepted] = 2; // +8,+3 ×2 ×0.5
        evs[GovernanceEvent::VillageOccupied] = 1;   // +5,+2 ×1 ×0.5
        gov.Accumulate(evs);
        // support: 50 + (8*2+5)*0.5 = 60.5；order: 50 + (3*2+2)*0.5 = 54
        Check(std::fabs(gov.PopularSupport() - 60.5f) < 1e-4f,
              "折帳民心正確");
        Check(std::fabs(gov.CivilOrder() - 54.0f) < 1e-4f,
              "折帳秩序正確");

        // 暴行壓低秩序觸發動亂
        std::unordered_map<GovernanceEvent, int> bad;
        bad[GovernanceEvent::Atrocity] = 7; // order -8*7*0.5 = -28 → 26
        gov.Accumulate(bad);
        Check(gov.CivilOrder() < Governance::kUnrestWarn,
              "秩序跌破警訊線");
        auto unrest = gov.PollUnrestEvents();
        Check(!unrest.empty(), "跨檻發動亂事件");
        Check(gov.PollUnrestEvents().empty(), "平級不重發");

        // 大量暴行到崩壞
        for (int i = 0; i < 10; ++i) {
            gov.Accumulate(bad);
        }
        Check(gov.UnrestLevel() == 3, "秩序崩壞級");
        auto more = gov.PollUnrestEvents();
        Check(more.size() >= 1, "崩壞跨級再發");
        Check(gov.CivilOrder() == 0.0f, "秩序夾到 0");
    }

    // ---- [4] 序列化 roundtrip ----
    printf("\n[4] 存檔 roundtrip\n");
    {
        Governance a;
        std::unordered_map<GovernanceEvent, int> evs;
        evs[GovernanceEvent::Atrocity] = 4;
        a.Accumulate(evs);
        a.PollUnrestEvents(); // 消耗掉已報級
        CampaignState cs;
        cs.Gov() = a;
        const std::string path = "governance_test_save.json";
        Check(cs.SaveToFile(path), "campaign 存檔");
        CampaignState cs2;
        Check(cs2.LoadFromFile(path), "campaign 讀檔");
        Check(std::fabs(cs2.Gov().PopularSupport() -
                        a.PopularSupport()) < 1e-4f,
              "民心 roundtrip");
        Check(std::fabs(cs2.Gov().CivilOrder() - a.CivilOrder()) <
                  1e-4f,
              "秩序 roundtrip");
        Check(cs2.Gov().PollUnrestEvents().empty(),
              "已報動亂級不跨檔重發");
        std::remove(path.c_str());
    }

    // ---- [5] 追殺暴行：Engage 潰逃隊的單向傷害路徑 ----
    // 修法前暴行支線是死碼：ResolveCombat 跳過潰逃目標、
    // Intervene 拒絕 Engage 潰兵——屠殺永遠不可能。
    printf("\n[5] 追殺暴行路徑\n");
    {
        BattleController battle(10, 10, 1.0f);
        battle.SetCommandPoints(0, 10);
        Squad* hunter =
            battle.CreateSquad("追兵", 0, Vector2(5, 5), 10);
        hunter->SetDamagePerMember(2.0f); // 20 dps
        hunter->SetSpeed(10.0f);          // 追上 1.2x 潰兵
        Squad* router =
            battle.CreateSquad("潰兵", 1, Vector2(5.2f, 5), 10);
        // 另一隊敵軍保持戰鬥力，防勝負提前翻讓 Update no-op
        Squad* anchor =
            battle.CreateSquad("敵主力", 1, Vector2(9, 9), 10);
        // 旁觀的友軍：未 Engage 潰兵，不該造成傷害（憐憫路徑）
        Squad* bystander =
            battle.CreateSquad("旁觀者", 0, Vector2(5.4f, 5), 10);
        bystander->SetDamagePerMember(50.0f);
        (void)anchor;
        battle.SetRallyPoint(1, Vector2(0, 0));
        battle.BeginExecution();

        router->AdjustMorale(-1.0f); // 士氣歸零 → routing
        battle.Update(0.1f);
        auto& evs = battle.GetGovernanceEvents();
        Check(EvCount(evs, GovernanceEvent::SurrenderAccepted) == 1,
              "潰逃先記受降");

        // 明示 Engage：追殺是合法可歸責指令
        Check(battle.Intervene(hunter, SquadOrder::Engage, router),
              "Engage 潰逃隊受理");
        Check(battle.GetCommandPoints(0) == 9, "追殺扣 1 CP");

        // 追兵單向輸出：潰兵損員 → Atrocity 恰一次
        for (int i = 0; i < 40 && !router->IsEliminated(); ++i) {
            battle.Update(0.1f);
        }
        Check(EvCount(evs, GovernanceEvent::Atrocity) == 1,
              "追殺損員 → Atrocity×1");
        Check(router->GetMembers() < 10, "潰兵確實損員");
        Check(bystander->GetMembers() == 10, "潰兵零輸出（無反擊）");
    }

    // ---- [6] killing blow：潰逃中被全滅仍記暴行 ----
    // ApplyCasualties 全滅時清 routing 旗標——偵測若先看
    // IsRouting 會漏掉最後一刀
    printf("\n[6] 全滅暴行\n");
    {
        BattleController battle(10, 10, 1.0f);
        battle.SetCommandPoints(0, 10);
        Squad* hunter =
            battle.CreateSquad("追兵", 0, Vector2(5, 5), 10);
        hunter->SetDamagePerMember(30.0f); // 300 dps，快速全滅
        hunter->SetSpeed(10.0f);
        Squad* router =
            battle.CreateSquad("潰兵", 1, Vector2(5.2f, 5), 6);
        Squad* anchor =
            battle.CreateSquad("敵主力", 1, Vector2(9, 9), 10);
        (void)anchor;
        battle.SetRallyPoint(1, Vector2(0, 0));
        battle.BeginExecution();

        router->AdjustMorale(-1.0f);
        battle.Update(0.1f);
        battle.Intervene(hunter, SquadOrder::Engage, router);
        for (int i = 0; i < 40 && !router->IsEliminated(); ++i) {
            battle.Update(0.1f);
        }
        auto& evs = battle.GetGovernanceEvents();
        Check(router->IsEliminated(), "潰兵被追殺至全滅");
        Check(EvCount(evs, GovernanceEvent::Atrocity) == 1,
              "全滅仍記 Atrocity×1（旗標已清也逮得到）");
    }

    // ---- [7] 憐憫路徑：無人點名追殺則潰兵不損員 ----
    printf("\n[7] 鄰接不追殺\n");
    {
        BattleController battle(10, 10, 1.0f);
        Squad* near_ =
            battle.CreateSquad("近敵", 0, Vector2(5.2f, 5), 10);
        near_->SetDamagePerMember(50.0f);
        Squad* router =
            battle.CreateSquad("潰兵", 1, Vector2(5.4f, 5), 10);
        Squad* anchor =
            battle.CreateSquad("敵主力", 1, Vector2(9, 9), 10);
        (void)anchor;
        battle.SetRallyPoint(1, Vector2(0, 0));
        battle.BeginExecution();
        router->AdjustMorale(-1.0f);
        // 近敵只站崗（未 Engage）：潰兵鄰接不受傷害
        for (int i = 0; i < 10; ++i) battle.Update(0.1f);
        auto& evs = battle.GetGovernanceEvents();
        Check(router->GetMembers() == 10, "潰兵鄰接零傷害");
        Check(EvCount(evs, GovernanceEvent::Atrocity) == 0,
              "無追殺指令無暴行");
        Check(EvCount(evs, GovernanceEvent::SurrenderAccepted) == 1,
              "受降仍記一次");
    }

    printf("\n=== 結果: %s ===\n", failures == 0 ? "全部 PASS" : "有 FAIL");
    return failures == 0 ? 0 : 1;
}
