#pragma once

// 資訊不對稱度量（UGSP §8 危害算子，header-only）。
// 對應論文：H(p,a) = ∫(γ·I_asymmetry + β·V_harm) dt；
// I_asymmetry 為資訊集 Jaccard 距離 |I_A∖I_B|/|I_A∪I_B|——
// A→1 表示資訊壟斷（rent-seeking），A→0 表示資訊對稱。
//
// 專案語義：I_A = 戰場真實實體集、I_B = 觀測方已揭露集——
// QuantumFog 的「未揭露敵軍」正是 I_A∖I_B，故
// A_info = hidden / total。情資黑箱累積社會危害，可作
// PeaceHazard 的 infoAsymmetry 特徵輸入（λ_C 正係數）。

#include <algorithm>
#include <cstddef>

namespace Potato {
namespace Gameplay {

struct IntelAsymmetry {
    // Jaccard 距離：觀測方已知 ⊆ 全域的前提下
    // A_info = |I_A∖I_B| / |I_A∪I_B| = (total−known)/total。
    // 全域為空 → 0（無可隱藏即無不對稱）。
    static float InfoAsymmetry(size_t total, size_t known) {
        if (total == 0) {
            return 0.0f;
        }
        const size_t hidden = (known <= total) ? total - known : 0;
        return static_cast<float>(hidden) / static_cast<float>(total);
    }

    // 泛型 Jaccard：任兩集合的 |A∖B|/|A∪B|（交集合乎
    // |A∩B| 已知情境下使用——本專案觀測集必為全域子集）。
    static float JaccardDistance(size_t unionSize,
                                 size_t intersectionSize) {
        if (unionSize == 0) {
            return 0.0f;
        }
        return 1.0f - static_cast<float>(intersectionSize) /
                          static_cast<float>(unionSize);
    }

    // 危害速率（UGSP §8.1 的被積分項）：
    // harm_rate = γ·A_info + β·V_harm_agg，γ,β>0。
    // V_harm 三分量 [尊嚴削減/剝削/侵略]——對應治理軸：
    // 墮落(depravity)→尊嚴、動亂(unrest)→剝削、
    // 暴行(atrocity 計數)→侵略。各自先正規化 0..1 傳入。
    static float HarmRate(float gamma, float beta,
                          float infoAsymmetry,
                          float dignityLoss, float exploitation,
                          float aggression) {
        const float v = std::max(0.0f, dignityLoss) +
                        std::max(0.0f, exploitation) +
                        std::max(0.0f, aggression);
        return std::max(0.0f, gamma) * infoAsymmetry +
               std::max(0.0f, beta) * v;
    }
};

} // namespace Gameplay
} // namespace Potato
