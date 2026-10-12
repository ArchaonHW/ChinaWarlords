#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <vector>

namespace Potato::Gameplay {

class QuantumFog;
class JsonValue;

// BattlePlan — drawn movement arrows (GDD FR5).
//
// An arrow is a path of adjacent regions authored during Planning.
// When Execution begins the arrow grants its squad an attack bonus =
// mean CertaintyField over the covered regions (integer mean, capped
// by PlanConfig) — better intel, better plan. A squad Holding on its
// arrow's current waypoint auto-marches one leg per opportunity; a
// squad pulled off-path stalls the arrow until it returns or a replan
// rewrites it.

struct PlanArrow {
    bool active = false;
    std::vector<std::size_t> path; // path[0] = start region; adjacent chain
    std::size_t cursor = 0;        // index of squad's expected position
    int bonusApplied = 0;          // attack points already granted
};

// "plan" object inside potato.balance/1.
struct PlanConfig {
    int bonusPercentCap = 100; // bonus = min(mean certainty, cap)
    int replanCost = 2;        // CP per mid-execution replan

    static Result<PlanConfig> FromJson(const JsonValue& root);
};

// Mean certainty over a path — the f() of the AC. Integer division;
// empty path = 0.
int MeanCertainty(const QuantumFog& fog, const std::vector<std::size_t>& path);

} // namespace Potato::Gameplay
