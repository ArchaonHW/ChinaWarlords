// GroundStateTest — UGSP 第二層機制驗證（headless）
// 覆蓋：IntelAsymmetry Jaccard 距離與危害速率、
// CooperationDynamics 複製方程均衡/Lyapunov 條件/
// 秩序調變/持久化，以及 CampaignState::AdvanceChapter
// 整合（合作比例餵 PeaceHazard 特徵）。

#include "Campaign/CampaignState.h"
#include "Campaign/CooperationDynamics.h"
#include "Campaign/PeaceHazard.h"
#include "Gameplay/IntelAsymmetry.h"
#include "Serialization/JsonParser.h"

#include <cmath>
#include <cstdio>

using namespace Potato;
using namespace Potato::Campaign;
using namespace Potato::Gameplay;

static int failures = 0;
static void Check(bool cond, const char* name) {
    if (cond) {
        printf("  [PASS] %s\n", name);
    } else {
        printf("  [FAIL] %s\n", name);
        ++failures;
    }
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== GroundState Tests (UGSP §6-9) ===\n");

    // ---- [1] IntelAsymmetry：Jaccard 資訊距離 ----
    printf("\n[1] 情報不對稱\n");
    {
        Check(std::fabs(IntelAsymmetry::InfoAsymmetry(10, 10) -
                        0.0f) < 1e-6f,
              "全揭露 → A_info=0");
        Check(std::fabs(IntelAsymmetry::InfoAsymmetry(10, 0) -
                        1.0f) < 1e-6f,
              "全未揭露 → A_info=1（資訊壟斷）");
        Check(std::fabs(IntelAsymmetry::InfoAsymmetry(4, 3) -
                        0.25f) < 1e-6f,
              "4 實體藏 1 → 0.25");
        Check(IntelAsymmetry::InfoAsymmetry(0, 0) == 0.0f,
              "空全域 → 0");
        Check(IntelAsymmetry::InfoAsymmetry(5, 7) == 0.0f,
              "known>total 視為全揭露（防呆）");
        Check(std::fabs(IntelAsymmetry::JaccardDistance(10, 7) -
                        0.3f) < 1e-6f,
              "Jaccard 距離 = 1−|∩|/|∪|");
    }

    // ---- [2] HarmRate：γ·A + β·V ----
    printf("\n[2] 危害速率\n");
    {
        const float r =
            IntelAsymmetry::HarmRate(1.0f, 1.0f, 0.5f, 0.3f,
                                     0.2f, 0.1f);
        Check(std::fabs(r - (0.5f + 0.6f)) < 1e-6f,
              "harm = γ·A + β·ΣV");
        Check(IntelAsymmetry::HarmRate(0.0f, 0.0f, 1.0f, 1.0f,
                                       1.0f, 1.0f) == 0.0f,
              "γ=β=0 → 無危害");
        Check(IntelAsymmetry::HarmRate(1.0f, 1.0f, 0.5f, -1.0f,
                                       -1.0f, -1.0f) == 0.5f,
              "負危害分量不反向補血");
    }

    // ---- [3] 複製方程：均衡與 Lyapunov 條件 ----
    printf("\n[3] 複製方程均衡\n");
    {
        CooperationDynamics c;
        // r0=1, α=3, N=4 → x*₃=(4−1)/3=1.0；α>N−r0 ⇔ 3>3 不成立
        Check(!c.GlobalAttraction(), "α=N−r0 邊界不視為全局吸引");
        c.alpha = 3.5f; // α=3.5 > N−r0=3 → x*₃≈0.857
        Check(c.GlobalAttraction(), "α>N−r0 → 盆域非空");
        Check(std::fabs(c.BasinThreshold() - (4.0f - 1.0f) / 3.5f) <
                  1e-6f,
              "x*₃=(N−r0)/α");
    }

    // ---- [4] 盆域上→收斂 1；盆域下→歸零 ----
    printf("\n[4] 收斂方向\n");
    {
        CooperationDynamics up;
        up.alpha = 5.0f; // x*₃=0.6
        up.SetShare(0.8f);
        for (int i = 0; i < 400; ++i) up.Step(0.5f, 1.0f);
        Check(up.Share() > 0.99f, "x0>x*₃ → 收斂全合作");

        CooperationDynamics down;
        down.alpha = 5.0f;
        down.SetShare(0.3f); // <x*₃=0.6
        for (int i = 0; i < 400; ++i) down.Step(0.5f, 0.0f);
        Check(down.Share() == 0.0f, "x0<x*₃ → 合作崩解歸零");
        down.Step(0.5f, 1.0f);
        Check(down.Share() == 0.0f, "x=0 吸收均衡不再復活");
    }

    // ---- [5] 秩序調變 α_eff ----
    printf("\n[5] 秩序放大器\n");
    {
        CooperationDynamics a, b;
        a.alpha = 5.0f;
        b.alpha = 5.0f;
        a.SetShare(0.7f); // <x*₃(α_eff 半效→0.72)，>x*₃(滿效→0.48)
        b.SetShare(0.7f);
        a.Step(1.0f, 0.0f); // 秩序 0 → α_eff=2.5 → x*₃=1.2 → 應跌
        b.Step(1.0f, 1.0f); // 秩序滿 → α_eff=7.5 → x*₃=0.4 → 應升
        Check(a.Share() < 0.7f, "無秩序 → 合作跌");
        Check(b.Share() > 0.7f, "高秩序 → 合作升");
    }

    // ---- [6] 持久化 ----
    printf("\n[6] coop_dynamics 持久段\n");
    {
        CooperationDynamics c;
        c.alpha = 4.5f;
        c.SetShare(0.77f);
        const JsonValue j = c.ToJson();
        Check(j["schema"].AsString() == "potato.coop_dynamics/1",
              "寫出 potato.coop_dynamics/1");
        CooperationDynamics r;
        Check(r.FromJson(j) && std::fabs(r.Share() - 0.77f) < 1e-6f &&
                  std::fabs(r.alpha - 4.5f) < 1e-6f,
              "往返還原 share/參數");
        JsonValue badTag = j;
        badTag.objectValue["schema"] =
            JsonValue::String("potato.other/9");
        Check(!r.FromJson(badTag), "錯 tag 拒絕");
        JsonValue nullJ;
        Check(r.FromJson(nullJ), "缺段容忍（Null）");
    }

    // ---- [7] 整合：AdvanceChapter 驅動合作+和平 hazard ----
    printf("\n[7] CampaignState 整合\n");
    {
        CampaignState c;
        c.Peace().Seed(11);
        const float share0 = c.Coop().Share();
        c.SetIntelAsymmetry(0.8f); // 本場情報差極大
        c.AdvanceChapter(1, 1, "ch-1");
        Check(c.Coop().Share() != share0,
              "章節邊界推進合作比例");
        Check(c.Peace().CumulativePeaceHazard() > 0.0,
              "peace hazard 已累積");
        // 情報差逐場逐清——第二章不再計入
        PeaceHazard probe;
        auto f2 = PeaceHazard::FromGovernance(c.Gov(), 0.0f);
        f2.cooperation = c.Coop().Share();
        const double withAsym = probe.LambdaCollapse(
            [&f2] { auto g = f2; g.infoAsymmetry = 0.8f;
                    return g; }());
        Check(probe.LambdaCollapse(f2) < withAsym,
              "infoAsymmetry 抬高 λ_C");
    }

    printf("\n=== 結果: %s ===\n", failures == 0 ? "全部 PASS" : "有 FAIL");
    return failures == 0 ? 0 : 1;
}
