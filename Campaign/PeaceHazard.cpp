#include "Campaign/PeaceHazard.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Potato {
namespace Campaign {

PeaceHazard::Features PeaceHazard::FromGovernance(
    const Governance& gov, float seepagePressure01) {
    Features f;
    f.popularSupport = gov.PopularSupport() / 100.0f;
    f.civilOrder = gov.CivilOrder() / 100.0f;
    f.depravity = gov.Depravity() / 100.0f;
    f.seepagePressure = std::clamp(seepagePressure01, 0.0f, 1.0f);
    return f;
}

float PeaceHazard::Softplus(float x) {
    // softplus(x)=log(1+e^x)；x 大時直接回 x 防 exp 溢出
    if (x > 20.0f) {
        return x;
    }
    return std::log1p(std::exp(x));
}

static float ApplyCoefs(const PeaceHazard::Coefs& c,
                        const PeaceHazard::Features& f) {
    return PeaceHazard::Softplus(c.intercept +
                                 c.support * f.popularSupport +
                                 c.order * f.civilOrder +
                                 c.depravity * f.depravity +
                                 c.seepage * f.seepagePressure +
                                 c.coop * f.cooperation +
                                 c.asym * f.infoAsymmetry);
}

float PeaceHazard::LambdaPeace(const Features& f) const {
    return ApplyCoefs(kPeaceCoefs, f);
}

float PeaceHazard::LambdaCollapse(const Features& f) const {
    return ApplyCoefs(kCollapseCoefs, f);
}

double PeaceHazard::Survival() const {
    return std::exp(-(cumPeace + cumCollapse));
}

double PeaceHazard::StepPeaceChance(const Features& f,
                                    double dt) const {
    const double lp = LambdaPeace(f);
    const double lc = LambdaCollapse(f);
    const double total = lp + lc;
    if (total <= 0.0) {
        return 0.0;
    }
    // 本步命中且歸因和平 = (1−exp(−λΔt)) · λ_P/λ
    return (1.0 - std::exp(-total * dt)) * (lp / total);
}

double PeaceHazard::StepCollapseChance(const Features& f,
                                       double dt) const {
    const double lp = LambdaPeace(f);
    const double lc = LambdaCollapse(f);
    const double total = lp + lc;
    if (total <= 0.0) {
        return 0.0;
    }
    return (1.0 - std::exp(-total * dt)) * (lc / total);
}

void PeaceHazard::Advance(double dt, const Features& f) {
    if (outcome != Outcome::Unresolved || dt <= 0.0) {
        return;
    }
    const double lp = LambdaPeace(f);
    const double lc = LambdaCollapse(f);
    // 累積 hazard 永遠計入——即使本步未命中（S(t) 連續衰減）
    cumPeace += lp * dt;
    cumCollapse += lc * dt;

    const double total = lp + lc;
    if (total <= 0.0) {
        return;
    }
    std::uniform_real_distribution<double> U(0.0, 1.0);
    const double u = U(rng);
    ++draws;
    const double q = 1.0 - std::exp(-total * dt);
    if (u >= q) {
        return; // 本步存活
    }
    // 命中——歸因：常數 hazard 下 Pr(和平先於崩壞)=λ_P/λ
    const double cause = U(rng);
    ++draws;
    outcome = (cause < lp / total) ? Outcome::StablePeace
                                   : Outcome::Collapse;
}

void PeaceHazard::Seed(uint32_t s) {
    seed = s;
    rng.seed(s);
    draws = 0;
}

JsonValue PeaceHazard::ToJson() const {
    JsonValue o;
    o.type = JsonValue::Type::Object;
    o.objectValue["schema"] = JsonValue::String(kSchema);
    o.objectValue["cum_peace"] = JsonValue::Number(cumPeace);
    o.objectValue["cum_collapse"] = JsonValue::Number(cumCollapse);
    o.objectValue["outcome"] = JsonValue::Number(
        static_cast<int>(outcome));
    o.objectValue["seed"] = JsonValue::Number(seed);
    o.objectValue["draws"] = JsonValue::Number(
        static_cast<double>(draws));
    return o;
}

bool PeaceHazard::FromJson(const JsonValue& j) {
    if (j.IsNull()) {
        // 缺段降級：全新競爭態（舊檔無此段）
        cumPeace = cumCollapse = 0.0;
        outcome = Outcome::Unresolved;
        seed = 0;
        draws = 0;
        rng.seed(0);
        return true;
    }
    if (!j.IsObject()) {
        return false;
    }
    // 軟驗證：缺 tag = 舊檔容忍；有 tag 但不相符 = 拒絕
    const JsonValue& tag = j["schema"];
    if (tag.IsString() && tag.stringValue != kSchema) {
        return false;
    }
    const auto finiteOr = [](double v, double def) {
        return std::isfinite(v) ? v : def;
    };
    cumPeace = std::max(0.0, finiteOr(j["cum_peace"].AsNumber(0.0), 0.0));
    cumCollapse =
        std::max(0.0, finiteOr(j["cum_collapse"].AsNumber(0.0), 0.0));
    const int o = j["outcome"].AsInt(0);
    outcome = (o == 1)   ? Outcome::StablePeace
              : (o == 2) ? Outcome::Collapse
                         : Outcome::Unresolved;
    const uint32_t s =
        static_cast<uint32_t>(j["seed"].AsInt(0));
    const uint64_t d =
        static_cast<uint64_t>(std::max(0, j["draws"].AsInt(0)));
    // 重建 RNG 狀態：同一 seed 重播 draws 次——還原後續序列
    seed = s;
    rng.seed(s);
    rng.discard(d);
    draws = d;
    return true;
}

} // namespace Campaign
} // namespace Potato
