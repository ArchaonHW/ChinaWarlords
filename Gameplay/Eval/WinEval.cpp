#include "Gameplay/Eval/WinEval.h"

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Sim/Sim.h"

#include <string>

namespace Potato::Gameplay {

namespace {

Result<int> ReadCfgInt(const JsonValue& obj, const char* key,
                       int fallback, int min, int max) {
    const JsonValue& f = obj[key];
    if (f.IsNull()) return Ok<int>(fallback);
    if (!f.IsInt()) {
        return Fail<int>("balance", std::string("'eval.") + key +
                                    "' must be an integer");
    }
    const std::int64_t v = f.AsInt();
    if (v < min || v > max) {
        return Fail<int>("balance", std::string("'eval.") + key +
                                    "' out of range");
    }
    return Ok<int>(static_cast<int>(v));
}

} // namespace

Result<EvalConfig> EvalConfig::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<EvalConfig>("balance", "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.balance/1") {
            return Fail<EvalConfig>("schema", "expected potato.balance/1");
        }
    }
    const JsonValue& eval = root["eval"];
    if (eval.IsNull()) return Ok(EvalConfig{});
    if (!eval.IsObject()) {
        return Fail<EvalConfig>("balance", "'eval' must be an object");
    }
    EvalConfig c;
    // 0 disables the timer; the cap keeps a typo'd config from
    // producing an effectively-infinite battle (200 min bound matches
    // the replay verifier's hard cap).
    auto ticks = ReadCfgInt(eval, "stalemate_ticks", c.stalemateTicks,
                            0, 200 * 60 * TICK_RATE_HZ);
    if (!ticks.ok()) return Fail<EvalConfig>(ticks.error, ticks.reason);
    c.stalemateTicks = ticks.value;
    return Ok(c);
}

} // namespace Potato::Gameplay
