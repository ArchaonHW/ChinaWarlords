#include "Gameplay/Plan/BattlePlan.h"

#include "Gameplay/Command/Intervention.h" // CP_CAP
#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Json/JsonValue.h"

#include <string>

namespace Potato::Gameplay {

Result<PlanConfig> PlanConfig::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<PlanConfig>("balance", "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.balance/1") {
            return Fail<PlanConfig>("schema", "expected potato.balance/1");
        }
    }
    const JsonValue& plan = root["plan"];
    if (plan.IsNull()) return Ok(PlanConfig{});
    if (!plan.IsObject()) {
        return Fail<PlanConfig>("balance", "'plan' must be an object");
    }
    PlanConfig c;
    const JsonValue& cap = plan["bonus_percent_cap"];
    if (!cap.IsNull()) {
        if (!cap.IsInt() || cap.AsInt() < 0 || cap.AsInt() > 100) {
            return Fail<PlanConfig>(
                "balance", "'plan.bonus_percent_cap' out of range");
        }
        c.bonusPercentCap = static_cast<int>(cap.AsInt());
    }
    const JsonValue& rc = plan["replan_cost"];
    if (!rc.IsNull()) {
        // > CP_CAP would make Replan permanently unaffordable.
        if (!rc.IsInt() || rc.AsInt() < 0 || rc.AsInt() > CP_CAP) {
            return Fail<PlanConfig>("balance",
                                    "'plan.replan_cost' out of range");
        }
        c.replanCost = static_cast<int>(rc.AsInt());
    }
    return Ok(c);
}

int MeanCertainty(const QuantumFog& fog,
                  const std::vector<std::size_t>& path) {
    if (path.empty()) return 0;
    int sum = 0;
    for (std::size_t r : path) sum += fog.CertaintyAt(r);
    return sum / static_cast<int>(path.size());
}

} // namespace Potato::Gameplay
