#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {

class BattleMap;
class JsonValue;

// QuantumFog — probabilistic intel (GDD FR4, D-ARCH-7).
//
// TRUTH BOUNDARY: this module stores BELIEF ONLY. CloudEntity has no
// squad index and no true position — the truth->cloud mapping lives in
// Gameplay/Sim (BattleController), which supplies truth as call
// parameters (Collapse). Nothing in Fog/ may read ground truth, and
// nothing outside Sim/ may read truth at all.
//
// Two layers:
//   CertaintyField — per-map-region intel value 0..100 (probes raise,
//                    decay erodes, observation fills).
//   CloudEntity    — a superposed enemy position: believedRegion +
//                    certainty, resolved to truth by collapse, possibly
//                    entangled with a partner that shares its fate.

struct CloudEntity {
    int id = -1;
    int believedRegion = -1;   // belief, not truth
    int certainty = 0;         // 0..100
    int entangledWith = -1;    // cloud id sharing fate, -1 = none
};

// Balance knobs — "fog" object inside potato.balance/1. All optional;
// defaults apply when absent.
struct FogConfig {
    int decayPerMinute = 10;   // certainty points lost per 60 s
    int probeGain = 25;        // CertaintyField bump per probe
    int detectThreshold = 60;  // doctrine "enemy_*" sees clouds at >= this
    int initialIntel = 60;     // certainty of a freshly deployed cloud

    static Result<FogConfig> FromJson(const JsonValue& root);
};

// Per-observer fog view. One QuantumFog per battle side.
class QuantumFog {
public:
    QuantumFog(const BattleMap& map, const FogConfig& config);

    int RegionCount() const { return static_cast<int>(certainty_.size()); }
    int CertaintyAt(std::size_t region) const;
    const std::vector<CloudEntity>& Clouds() const { return clouds_; }
    const FogConfig& Config() const { return config_; }

    // --- Belief mutations (caller supplies truth where needed) ---
    int AddCloud(int believedRegion, int certainty); // -> cloud id
    void RemoveCloud(int cloudId);                   // target left field
    void Observe(std::size_t region);                // field -> 100
    void Collapse(int cloudId, int believedRegion);  // snap + 100
    // Planning/briefing hook (SetCloudIntel) — arbitrary certainty.
    void SetCloudBelief(int cloudId, int believedRegion, int certainty);
    void Probe(std::size_t region);                  // field += probeGain
    void Entangle(int cloudIdA, int cloudIdB);
    // Exact rational decay: decayPerMinute points per 1200 ticks.
    void TickDecay();
    // Doctrine view: clouds believed in `region` at detection strength.
    int VisibleAt(std::size_t region) const;
    // Reverse lookup for entanglement propagation (caller keeps truth).
    int EntangledWith(int cloudId) const;
    bool HasCloud(int cloudId) const;

private:
    CloudEntity* FindCloud(int cloudId);
    const CloudEntity* FindCloud(int cloudId) const;
    static int Clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

    const BattleMap* map_ = nullptr;
    FogConfig config_;
    std::vector<int> certainty_;       // per region, 0..100
    std::vector<CloudEntity> clouds_;  // insertion order; ids stable
                                       // and sparse after removal
    int nextCloudId_ = 0;
    int decayAccum_ = 0;               // certainty-points numerator
};

} // namespace Potato::Gameplay
