#pragma once

// 和平/崩壞隨機首達時框架（PPAH hitting-time model）。
// 參照 peace_stochastic_framework.pdf（Iris Yu v0.1）：
//
// - 戰役在每一步（章節邊界）以條件 hazard 投擲是否進入吸收態：
//   「穩定和平」P 或「終局崩壞」C。
// - hazard 規格：λ(t|X) = softplus(β0 + β·X_t)，保證非負；
//   X = 民心/秩序/墮落/滲透壓力（0..1 正規化）。
// - 競爭風險抽樣：總 hazard λ=λ_P+λ_C，本步命中率
//   q = 1−exp(−λ·Δt)；命中後歸因機率 = λ_P/λ（常數 hazard 下
//   即論文 Eq.32 的 a/(a+b)）。
// - 時間單位不變性（Thm 4.3）：只有 λ·Δt 乘積有意義，Δt=1
//   章節時間單位與任意切細等價（累積 hazard 相同）。
// - Restart 性質：存檔還原後由 cumHazard+draws 重建，後續命中
//   序列與不中斷執行一致——章節邊界即 stopping time。
//
// RNG 為 seeded mt19937；序列化存 seed+draws，還原時
// seed + discard(draws) 重建同一序列（決定性存檔）。

#include "Campaign/Governance.h"
#include "Serialization/JsonParser.h"

#include <cstdint>
#include <random>

namespace Potato {
namespace Campaign {

class PeaceHazard {
public:
    enum class Outcome {
        Unresolved,  // 仍在競爭中
        StablePeace, // 先命中和平吸收態
        Collapse,    // 先命中崩壞吸收態
    };

    // 潛狀態特徵 X_t（0..1 正規化輸入）。
    struct Features {
        float popularSupport = 0.5f;  // 民心
        float civilOrder = 0.5f;      // 秩序
        float depravity = 0.0f;       // 墮落（ratchet）
        float seepagePressure = 0.0f; // 神話滲透壓力
        float cooperation = 0.5f;     // UGSP：合作人口比例 x
        float infoAsymmetry = 0.0f;   // UGSP：情報 Jaccard 距離
    };

    // 由治理帳 + 滲透壓力組特徵（呼叫端把 MythLayer pressure
    // 折成 0..1 傳入——本層不直接依賴 MythLayer）。
    static Features FromGovernance(const Governance& gov,
                                   float seepagePressure01);

    // hazard 係數：λ = softplus(intercept + Σ coef·feature)
    struct Coefs {
        float intercept;
        float support;
        float order;
        float depravity;
        float seepage;
        float coop; // UGSP 合作比例
        float asym; // UGSP 情報不對稱（資訊租→崩壞壓力）
    };

    PeaceHazard() = default;

    static float Softplus(float x); // log(1+e^x)，數值穩定

    float LambdaPeace(const Features& f) const;
    float LambdaCollapse(const Features& f) const;

    // 推進一個離散步（Δt 章節時間單位，預設 1）並做一次
    // 競爭風險抽樣。已進吸收態後呼叫為 no-op（absorbing 契約）。
    void Advance(double dt, const Features& f);
    void Advance(const Features& f) { Advance(1.0, f); }

    Outcome GetOutcome() const { return outcome; }
    // 累積 hazard Λ_P/Λ_C——生存函數 S(t)=exp(−Λ_P−Λ_C)
    double CumulativePeaceHazard() const { return cumPeace; }
    double CumulativeCollapseHazard() const { return cumCollapse; }
    double Survival() const;
    // 解析式當前步條件機率（不消耗 RNG）——HUD/情報層可讀
    double StepPeaceChance(const Features& f, double dt = 1.0) const;
    double StepCollapseChance(const Features& f,
                              double dt = 1.0) const;

    void Seed(uint32_t seed);
    uint64_t DrawCount() const { return draws; }

    JsonValue ToJson() const;
    bool FromJson(const JsonValue& j);

    // 預設係數——基準局（sup=ord=0.5, dep=seep=0）下
    // λ_P≈softplus(−1.5)≈0.20/章、λ_C≈softplus(−5.25)≈0.005/章；
    // 民心秩序歸零+墮落滿格時 λ_C 反超。
    static constexpr Coefs kPeaceCoefs = {-3.0f, +1.5f, +1.5f,
                                          -1.0f, -1.0f, +1.0f, +0.0f};
    static constexpr Coefs kCollapseCoefs = {-4.0f, -1.0f, -1.5f,
                                             +2.0f, +1.5f, -0.8f,
                                             +1.2f};

private:
    std::mt19937 rng{0};
    uint32_t seed = 0;
    uint64_t draws = 0; // 已消耗亂數步數（還原用）
    double cumPeace = 0.0;
    double cumCollapse = 0.0;
    Outcome outcome = Outcome::Unresolved;

    static constexpr const char* kSchema = "potato.peace_hazard/1";
};

} // namespace Campaign
} // namespace Potato
