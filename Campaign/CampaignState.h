#pragma once

// 戰役狀態（E-7）：跨章節狀態的 facade 聚合。
//
// 架構（game-architecture.md §Campaign State）：各子系統自持資料
// 與序列化，CampaignState 只在章節邊界聚合成單一
// potato.campaign/1 檔——原子寫（tmp+rename），壞檔大聲拒絕。
// 子系統互不探入；跨章節協調全經本類。
//
// 已掛載：RefitCamp（整補營）、Roster（名冊）、
// CampaignLedger（N-4 敵將處置/稱號/章節序帳本）、ChapterState
// （弧/章節id/已收戰線）。預留段 governance/god_stance/
// intel_ledger 由 E-5/E-6/E-11 後續填充（缺段載入不報錯）。
//
// 章節序由帳本管（AdvanceChapter 同步 ChapterState），
// 存檔只發生在章節邊界——戰中不存檔（架構§Save/Load）。

#include "Campaign/ChapterState.h"
#include "Campaign/CooperationDynamics.h"
#include "Campaign/Governance.h"
#include "Campaign/MythLayer.h"
#include "Campaign/PeaceHazard.h"
#include "Campaign/PlayerDeck.h"
#include "Gameplay/CampaignLedger.h"
#include "Gameplay/RefitCamp.h"
#include "Gameplay/Roster.h"

#include <string>

namespace Potato {
namespace Campaign {

class CampaignState {
public:
    // ---- 子系統掛載 ----
    Gameplay::RefitCamp& Camp() { return camp; }
    const Gameplay::RefitCamp& Camp() const { return camp; }
    Gameplay::Roster& NamedRoster() { return roster; }
    const Gameplay::Roster& NamedRoster() const { return roster; }
    // N-4 帳本：敵將處置記錄 + 稱號軌跡 + 章節序
    Gameplay::CampaignLedger& Ledger() { return ledger; }
    const Gameplay::CampaignLedger& Ledger() const { return ledger; }
    // C-2 治理帳：跨章節民心/秩序累加 + 動亂級
    Governance& Gov() { return governance; }
    const Governance& Gov() const { return governance; }
    // D-1 神話滲透層：區域滲透等級 + 神明好感表
    MythLayer& Myths() { return myth; }
    const MythLayer& Myths() const { return myth; }
    // G-2 玩家牌庫：deck=科技樹，掠奪敵將 signature 卡成長
    PlayerDeck& Deck() { return deck; }
    const PlayerDeck& Deck() const { return deck; }
    // PPAH 和平/崩壞競爭風險層：章節邊界抽樣首達吸收態
    PeaceHazard& Peace() { return peace; }
    const PeaceHazard& Peace() const { return peace; }
    // UGSP 合作演化層：複製方程驅動的合作人口比例
    CooperationDynamics& Coop() { return coop; }
    const CooperationDynamics& Coop() const { return coop; }

    // UGSP §8：戰鬥層把當場情報不對稱（Jaccard 距離 0..1）
    // 存入——下次 AdvanceChapter 消費後清零（逐場逐清）
    void SetIntelAsymmetry(float a) {
        intelAsymmetry = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    }

    // 弧/章節id/已收服戰線（章節序由帳本管，AdvanceChapter 同步）
    ChapterState chapter;

    // 章節邊界推進：同步帳本章節序與 ChapterState
    void AdvanceChapter(int arc, int ch, const std::string& chapterId);

    // ---- 存檔：potato.campaign/1，tmp+rename 原子寫 ----
    bool SaveToFile(const std::string& path) const;
    // 壞檔/異版：回 false 且不更動現況（先驗證再置換）
    bool LoadFromFile(const std::string& path);

private:
    Gameplay::RefitCamp camp;
    Gameplay::Roster roster;
    Gameplay::CampaignLedger ledger;
    Governance governance;      // C-2 戰役治理帳（governance 段）
    MythLayer myth;             // D-1 滲透狀態機（myth_layer 段）
    PlayerDeck deck;            // G-2 牌庫掠奪（player_deck 段）
    PeaceHazard peace;          // PPAH 首達時層（peace_hazard 段）
    CooperationDynamics coop;   // UGSP 合作層（coop_dynamics 段）
    float intelAsymmetry = 0.0f; // 逐場情報差（runtime，非存檔）
};

} // namespace Campaign
} // namespace Potato
