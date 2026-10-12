#include "Campaign/Quantum/QuantumOracle.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Json/JsonValue.h"
#include "Quantum/QubitRegister.h"
#include "Quantum/Qudit.h"
#include "Quantum/RandomSource.h"

#include <cmath>
#include <string>

namespace Potato {
namespace Campaign {
namespace {

const char* kSchema = "potato.oracle/1";

// Q-5 injection: the campaign's integer PRNG wears the quantum
// seam. Owned by the fresh circuit but forwards every roll to the
// oracle's stream — draw order = construction order.
class PrngSource final : public Quantum::IRandomSource {
public:
    explicit PrngSource(Gameplay::Prng* p) : p_(p) {}
    std::uint64_t NextU64() override { return p_->Next(); }
private:
    Gameplay::Prng* p_;
};

} // namespace

QuantumOracle::QuantumOracle(std::uint64_t seed, std::uint64_t state)
    : rng_(state ? state : seed) {}

Gameplay::Result<QuantumOracle> QuantumOracle::FromJson(
    const Gameplay::JsonValue& doc) {
    using Gameplay::Fail;
    using Gameplay::Ok;
    if (!doc.IsObject()) {
        return Fail<QuantumOracle>("oracle", "doc not object");
    }
    const std::string* schema = doc.FindString("schema");
    if (!schema || *schema != kSchema) {
        return Fail<QuantumOracle>("oracle", "schema mismatch");
    }
    for (const auto& kv : doc.Members()) {
        if (kv.first != "schema" && kv.first != "seed" &&
            kv.first != "state") {
            return Fail<QuantumOracle>("oracle",
                                       "oracle unknown key");
        }
    }
    if (!doc["seed"].IsInt() || doc["seed"].AsInt() == 0) {
        // seed 0 means "OS entropy" inside the engine module —
        // an oracle that can't be replayed is a bug, reject it.
        return Fail<QuantumOracle>("oracle",
                                   "oracle seed missing/zero");
    }
    const std::uint64_t seed =
        static_cast<std::uint64_t>(doc["seed"].AsInt());
    std::uint64_t state = 0;
    if (doc.Has("state")) {
        if (!doc["state"].IsInt()) {
            return Fail<QuantumOracle>("oracle",
                                       "oracle state mistyped");
        }
        state = static_cast<std::uint64_t>(doc["state"].AsInt());
    }
    return Ok(QuantumOracle{seed, state});
}

Gameplay::Result<QuantumOracle> QuantumOracle::Load(
    std::string_view path) {
    auto doc = Gameplay::Json::Load(path, kSchema);
    if (!doc.ok()) {
        return Gameplay::Fail<QuantumOracle>(doc.error,
                                             std::move(doc.reason));
    }
    return FromJson(doc.value);
}

bool QuantumOracle::DrawPermille(int permille) {
    if (permille <= 0) return false;   // no stream burn
    if (permille >= 1000) return true; // no stream burn
    Quantum::QubitRegister r(
        1, std::make_unique<PrngSource>(&rng_));
    // P(1) = sin²(θ/2) under Ry — θ = 2·asin(√p) hits the
    // permille exactly; the Born-rule collapse is the draw.
    const double p = static_cast<double>(permille) / 1000.0;
    r.Ry(0, 2.0 * std::asin(std::sqrt(p)));
    return r.Measure(0);
}

int QuantumOracle::DrawCategorical(const int* weights, int count) {
    if (!weights || count <= 0) return -1;
    int nonzero = 0;
    int sole = -1;
    std::int64_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (weights[i] > 0) { ++nonzero; sole = i; total += weights[i]; }
    }
    if (nonzero == 0) return -1;      // empty question — no answer
    if (nonzero == 1) return sole;    // forced — no stream burn
    std::vector<double> probs(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        probs[static_cast<std::size_t>(i)] =
            weights[i] > 0 ? static_cast<double>(weights[i]) : 0.0;
    }
    Quantum::Qudit d(count,
                     std::make_unique<PrngSource>(&rng_));
    d.SetProbabilities(probs);
    return d.Measure();
}

} // namespace Campaign
} // namespace Potato
