#pragma once

// 合作演化動力學（UGSP §6–7：state-feedback 公共財博弈
// 複製方程）。人口合作比例 x ∈ (0,1] 隨時間演化：
//
//     ẋ = x(1−x)·Δf(x),  Δf(x) = (r0 + α·x)/N − 1
//
// r0 = 基礎合作回報、α = 合作-回饋增益（state feedback：
// 合作者越多、公共財乘數越大）、N = 群體規模。
//
// 均衡：x*=0（全背離）、x*=1（全合作）、
// x*₃=(N−r0)/α（內點、不穩定分水嶺）。
// Lyapunov（§7.2，V=1−x）：x > x*₃ 時 V̇<0 → x 收斂 1；
// α > N−r0 ⇔ x*₃<1——合作盆域非空的條件。
//
// 專案語義：秩序調變 α_eff = α·orderScale（制度放大合作
// 回報）；x 直接餵 PeaceHazard 的 cooperation 特徵——
// 讓和平 hazard 由動力學主幹驅動而非靜態快照。
// 確定性 ODE（無 RNG）——存檔只需 share/參數。

#include "Serialization/JsonParser.h"

namespace Potato {
namespace Campaign {

class CooperationDynamics {
public:
    float Share() const { return share; }
    void SetShare(float x);

    // Δf = f_C − f_D = (r0 + αx)/N − 1（正值合作擴散）
    float FitnessGap() const;
    // 內點不穩定均衡 x*₃ = (N−r0)/α（分水嶺）；<0 表示
    // 任意 x>0 皆流向合作
    float BasinThreshold() const;
    // 論文 §7.1 全局吸引條件：α > N−r0
    bool GlobalAttraction() const;

    // 前進 dt（章節時間單位）。orderScale ∈ [0,1] 由呼叫端
    // 從秩序折入——α_eff = alpha·(0.5+orderScale)：秩序越
    // 高，合作回饋越強（制度基礎建設放大器）。
    void Step(float dt, float orderScale);

    // 參數（建構後可調）
    float r0 = 1.0f;      // 基礎合作回報
    float alpha = 3.0f;   // 合作回饋增益
    int groupSize = 4;    // 公共財群體規模 N

    JsonValue ToJson() const;
    bool FromJson(const JsonValue& j);

    static constexpr float kMinShare = 0.02f; // x>0 存活底線

private:
    float share = 0.5f; // 合作人口比例
    static constexpr const char* kSchema = "potato.coop_dynamics/1";
};

} // namespace Campaign
} // namespace Potato
