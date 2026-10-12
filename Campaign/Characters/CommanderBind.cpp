#include "Campaign/Characters/CommanderBind.h"

#include <string>

namespace Potato::Campaign {

using Gameplay::Result;

namespace {

int Clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

} // namespace

PriorFogBias PriorBias(RivalPrior p) {
    switch (p) {
    case RivalPrior::Aggressive:
        return {10, 0, 0, -10};
    case RivalPrior::Defensive:
        return {0, -5, 0, 0};
    case RivalPrior::Cunning:
        return {0, 0, -10, 0};
    }
    return {};
}

void ApplyPrior(Gameplay::FogConfig& cfg, RivalPrior p) {
    const PriorFogBias b = PriorBias(p);
    cfg.probeGain += b.probeGainDelta;
    if (cfg.probeGain < 0) cfg.probeGain = 0;
    cfg.decayPerMinute += b.decayPerMinuteDelta;
    if (cfg.decayPerMinute < 1) cfg.decayPerMinute = 1;
    cfg.detectThreshold =
        Clamp100(cfg.detectThreshold + b.detectThresholdDelta);
    cfg.initialIntel =
        Clamp100(cfg.initialIntel + b.initialIntelDelta);
}

Result<std::uint64_t>
RecordCommander(const Character& c, IntelLedger& intel) {
    const std::string claim = "統帥" + c.name + "，性向" +
                              std::string(RivalPriorName(c.prior));
    return intel.Record(c.id, IntelKind::RivalTemperament, claim, 0);
}

} // namespace Potato::Campaign
