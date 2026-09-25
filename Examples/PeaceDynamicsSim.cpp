// PeaceDynamicsSim — PPAH 競爭風險 Monte-Carlo（工具消費者）
// 以 CampaignState::AdvanceChapter 真實章節邊界推進
// PeaceHazard，掃描「治理軌跡斜率」參數下的和平/崩壞命中
// 分布，與解析式 λ_P/(λ_P+λ_C) 對照。
//
// 用法: PeaceDynamicsSim [runs=2000] [chapters=30] [seed=1]
//      [slope=-1.0]
//   slope: 每章民心/秩序的漂移量（負=惡化，正=改善）

#include "Campaign/CampaignState.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace Potato::Campaign;

int main(int argc, char** argv) {
    int runs = 2000, chapters = 30;
    unsigned seed = 1;
    float slope = -1.0f;
    if (argc > 1) runs = std::atoi(argv[1]);
    if (argc > 2) chapters = std::atoi(argv[2]);
    if (argc > 3) seed = (unsigned)std::atoi(argv[3]);
    if (argc > 4) slope = (float)std::atof(argv[4]);

    printf("PPAH Monte-Carlo: runs=%d chapters=%d seed=%u "
           "slope=%.2f\n",
           runs, chapters, seed, slope);

    int peace = 0, collapse = 0, unresolved = 0;
    double sumPeaceHaz = 0.0, sumCollHaz = 0.0, sumCoop = 0.0;
    for (int i = 0; i < runs; ++i) {
        CampaignState c;
        c.Peace().Seed(seed + (unsigned)i);
        for (int ch = 1; ch <= chapters; ++ch) {
            // 治理漂移：惡化局每章民心/秩序各 -|slope|
            c.Gov().AdjustPopularSupport(slope);
            c.Gov().AdjustCivilOrder(slope);
            c.AdvanceChapter(1, ch, "sim-ch" + std::to_string(ch));
            if (c.Peace().GetOutcome() !=
                PeaceHazard::Outcome::Unresolved) {
                break;
            }
        }
        sumPeaceHaz += c.Peace().CumulativePeaceHazard();
        sumCollHaz += c.Peace().CumulativeCollapseHazard();
        sumCoop += c.Coop().Share();
        switch (c.Peace().GetOutcome()) {
        case PeaceHazard::Outcome::StablePeace: ++peace; break;
        case PeaceHazard::Outcome::Collapse: ++collapse; break;
        default: ++unresolved; break;
        }
    }

    printf("結果: 和平 %d (%.1f%%)  崩壞 %d (%.1f%%)  "
           "未決 %d (%.1f%%)\n",
           peace, 100.0 * peace / runs, collapse,
           100.0 * collapse / runs, unresolved,
           100.0 * unresolved / runs);
    printf("平均累積 hazard: Λ_P=%.3f Λ_C=%.3f  "
           "平均合作比例 x̄=%.3f\n",
           sumPeaceHaz / runs, sumCollHaz / runs, sumCoop / runs);
    return 0;
}
