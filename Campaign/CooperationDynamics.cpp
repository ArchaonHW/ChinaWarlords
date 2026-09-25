#include "Campaign/CooperationDynamics.h"

#include <algorithm>
#include <cmath>

namespace Potato {
namespace Campaign {

void CooperationDynamics::SetShare(float x) {
    share = std::clamp(x, 0.0f, 1.0f);
}

float CooperationDynamics::FitnessGap() const {
    const float n = std::max(1.0f, static_cast<float>(groupSize));
    return (r0 + alpha * share) / n - 1.0f;
}

float CooperationDynamics::BasinThreshold() const {
    if (alpha == 0.0f) {
        return 1.0f; // 無回饋增益——合作盆域萎縮至 {1}
    }
    return (groupSize - r0) / alpha;
}

bool CooperationDynamics::GlobalAttraction() const {
    return alpha > static_cast<float>(groupSize) - r0;
}

void CooperationDynamics::Step(float dt, float orderScale) {
    if (dt <= 0.0f || share <= 0.0f) {
        return; // x=0 是吸收均衡
    }
    // α_eff = α·(0.5+orderScale)：秩序 0 時半效，秩序滿時 1.5 倍
    const float scale =
        0.5f + std::clamp(orderScale, 0.0f, 1.0f);
    const float aEff = alpha * scale;
    const float n = std::max(1.0f, static_cast<float>(groupSize));
    const float df = (r0 + aEff * share) / n - 1.0f;
    // Euler 步進；x 夾 (0,1]——x=0 吸收、x=1 目標均衡
    share = std::clamp(share + dt * share * (1.0f - share) * df,
                       0.0f, 1.0f);
    // 一旦跌破存活底線視為合作崩解（吸收進 x=0）
    if (share < kMinShare) {
        share = 0.0f;
    }
}

JsonValue CooperationDynamics::ToJson() const {
    JsonValue o;
    o.type = JsonValue::Type::Object;
    o.objectValue["schema"] = JsonValue::String(kSchema);
    o.objectValue["share"] = JsonValue::Number(share);
    o.objectValue["r0"] = JsonValue::Number(r0);
    o.objectValue["alpha"] = JsonValue::Number(alpha);
    o.objectValue["n"] = JsonValue::Number(groupSize);
    return o;
}

bool CooperationDynamics::FromJson(const JsonValue& j) {
    if (j.IsNull()) {
        share = 0.5f; // 缺段降級：中性合作比例
        return true;
    }
    if (!j.IsObject()) {
        return false;
    }
    const JsonValue& tag = j["schema"];
    if (tag.IsString() && tag.stringValue != kSchema) {
        return false;
    }
    const auto finiteOr = [](double v, float def) {
        return std::isfinite(v) ? static_cast<float>(v) : def;
    };
    share = std::clamp(finiteOr(j["share"].AsNumber(0.5), 0.5f),
                       0.0f, 1.0f);
    r0 = finiteOr(j["r0"].AsNumber(1.0), 1.0f);
    alpha = finiteOr(j["alpha"].AsNumber(3.0), 3.0f);
    groupSize = std::clamp(j["n"].AsInt(4), 1, 10000);
    return true;
}

} // namespace Campaign
} // namespace Potato
