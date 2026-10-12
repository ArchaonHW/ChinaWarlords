#include "Gameplay/Fog/QuantumFog.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Map/BattleMap.h"

#include <string>

namespace Potato::Gameplay {

namespace {

// 60 s at the fixed sim rate (20 Hz) — kept local so Fog/ has no
// dependency back into Sim/ (Sim consumes Fog, not vice versa).
constexpr int TICKS_PER_MIN = 1200;

Result<int> ReadCfgInt(const JsonValue& obj, const char* key,
                       int fallback, int min, int max) {
    const JsonValue& f = obj[key];
    if (f.IsNull()) return Ok<int>(fallback);
    if (!f.IsInt()) {
        return Fail<int>("balance", std::string("'fog.") + key +
                                    "' must be an integer");
    }
    const std::int64_t v = f.AsInt();
    if (v < min || v > max) {
        return Fail<int>("balance", std::string("'fog.") + key +
                                    "' out of range");
    }
    return Ok<int>(static_cast<int>(v));
}

} // namespace

// --- FogConfig --------------------------------------------------------------

Result<FogConfig> FogConfig::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<FogConfig>("balance", "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.balance/1") {
            return Fail<FogConfig>("schema",
                                   "expected potato.balance/1");
        }
    }
    const JsonValue& fog = root["fog"];
    if (fog.IsNull()) return Ok(FogConfig{});
    if (!fog.IsObject()) {
        return Fail<FogConfig>("balance", "'fog' must be an object");
    }
    FogConfig c;
    auto decay = ReadCfgInt(fog, "decay_per_minute", c.decayPerMinute, 0, 1200);
    if (!decay.ok()) return Fail<FogConfig>(decay.error, decay.reason);
    c.decayPerMinute = decay.value;
    auto probe = ReadCfgInt(fog, "probe_gain", c.probeGain, 0, 100);
    if (!probe.ok()) return Fail<FogConfig>(probe.error, probe.reason);
    c.probeGain = probe.value;
    auto thresh = ReadCfgInt(fog, "detect_threshold", c.detectThreshold, 0, 100);
    if (!thresh.ok()) return Fail<FogConfig>(thresh.error, thresh.reason);
    c.detectThreshold = thresh.value;
    auto init = ReadCfgInt(fog, "initial_intel", c.initialIntel, 0, 100);
    if (!init.ok()) return Fail<FogConfig>(init.error, init.reason);
    c.initialIntel = init.value;
    return Ok(c);
}

// --- QuantumFog --------------------------------------------------------------

QuantumFog::QuantumFog(const BattleMap& map, const FogConfig& config)
    : map_(&map), config_(config),
      certainty_(map.RegionCount(), 0) {}

int QuantumFog::CertaintyAt(std::size_t region) const {
    if (region >= certainty_.size()) return 0;
    return certainty_[region];
}

int QuantumFog::AddCloud(int believedRegion, int certainty) {
    CloudEntity c;
    c.id = nextCloudId_++;
    c.believedRegion = believedRegion;
    c.certainty = Clamp100(certainty);
    clouds_.push_back(c);
    return c.id;
}

void QuantumFog::RemoveCloud(int cloudId) {
    for (std::size_t i = 0; i < clouds_.size(); ++i) {
        if (clouds_[i].id == cloudId) {
            // Untangle any partner so it doesn't point at a ghost.
            if (clouds_[i].entangledWith >= 0) {
                if (CloudEntity* p = FindCloud(clouds_[i].entangledWith)) {
                    p->entangledWith = -1;
                }
            }
            clouds_.erase(clouds_.begin() +
                          static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

void QuantumFog::Observe(std::size_t region) {
    if (region >= certainty_.size()) return;
    certainty_[region] = 100;
}

void QuantumFog::Collapse(int cloudId, int believedRegion) {
    if (CloudEntity* c = FindCloud(cloudId)) {
        c->believedRegion = believedRegion;
        c->certainty = 100;
    }
}

void QuantumFog::SetCloudBelief(int cloudId, int believedRegion,
                                int certainty) {
    if (CloudEntity* c = FindCloud(cloudId)) {
        c->believedRegion = believedRegion;
        c->certainty = Clamp100(certainty);
    }
}

void QuantumFog::Probe(std::size_t region) {
    if (region >= certainty_.size()) return;
    certainty_[region] = Clamp100(certainty_[region] + config_.probeGain);
}

void QuantumFog::Entangle(int cloudIdA, int cloudIdB) {
    CloudEntity* a = FindCloud(cloudIdA);
    CloudEntity* b = FindCloud(cloudIdB);
    if (a == nullptr || b == nullptr || a == b) return;
    // Sever existing links first — re-entangling without this leaves a
    // one-way stale back-link (old partner propagates into a cloud that
    // no longer reciprocates).
    if (a->entangledWith >= 0) {
        if (CloudEntity* p = FindCloud(a->entangledWith))
            p->entangledWith = -1;
    }
    if (b->entangledWith >= 0) {
        if (CloudEntity* p = FindCloud(b->entangledWith))
            p->entangledWith = -1;
    }
    a->entangledWith = b->id;
    b->entangledWith = a->id;
}

void QuantumFog::TickDecay() {
    decayAccum_ += config_.decayPerMinute;
    while (decayAccum_ >= TICKS_PER_MIN) {
        decayAccum_ -= TICKS_PER_MIN;
        for (int& v : certainty_) v = Clamp100(v - 1);
        for (CloudEntity& c : clouds_) c.certainty = Clamp100(c.certainty - 1);
    }
}

int QuantumFog::VisibleAt(std::size_t region) const {
    if (region >= certainty_.size()) return 0;
    const int r = static_cast<int>(region);
    int n = 0;
    for (const CloudEntity& c : clouds_) {
        if (c.believedRegion == r && c.certainty >= config_.detectThreshold) {
            ++n;
        }
    }
    return n;
}

int QuantumFog::EntangledWith(int cloudId) const {
    const CloudEntity* c = FindCloud(cloudId);
    return c != nullptr ? c->entangledWith : -1;
}

bool QuantumFog::HasCloud(int cloudId) const {
    return FindCloud(cloudId) != nullptr;
}

CloudEntity* QuantumFog::FindCloud(int cloudId) {
    for (CloudEntity& c : clouds_) {
        if (c.id == cloudId) return &c;
    }
    return nullptr;
}

const CloudEntity* QuantumFog::FindCloud(int cloudId) const {
    for (const CloudEntity& c : clouds_) {
        if (c.id == cloudId) return &c;
    }
    return nullptr;
}

} // namespace Potato::Gameplay
