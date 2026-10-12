#include "Gameplay/Squad/BudgetedBuild.h"

#include <unordered_map>

namespace Potato::Gameplay {

const char* BuildSkipReasonName(BuildSkipReason r) {
    switch (r) {
        case BuildSkipReason::UnknownId:  return "unknown_id";
        case BuildSkipReason::OverBudget: return "over_budget";
    }
    return "unknown_id";
}

BudgetedBuildResult BuildSquads(const SquadTemplateLibrary& lib,
                                const std::vector<std::string>& ids,
                                int budget, std::size_t regionIndex,
                                int side) {
    BudgetedBuildResult r;
    int remaining = budget < 0 ? 0 : budget;
    std::unordered_map<std::string, int> instances;
    for (const std::string& id : ids) {
        const SquadTemplate* t = lib.Find(id);
        if (t == nullptr) {
            r.skips.push_back({id, BuildSkipReason::UnknownId});
            continue;
        }
        if (t->cost > remaining) {
            r.skips.push_back({id, BuildSkipReason::OverBudget});
            continue;
        }
        Squad s = Squad::Instantiate(*t, regionIndex, side);
        const int n = ++instances[id];
        if (n > 1) {
            s.id += '#';
            s.id += static_cast<char>('0' + n % 10);
            if (n >= 10) {
                // Rare; keep it deterministic, not clever.
                s.id.pop_back();
                s.id += std::to_string(n);
            }
        }
        r.squads.push_back(std::move(s));
        r.templates.push_back(t);
        remaining -= t->cost;
    }
    r.spent = (budget < 0 ? 0 : budget) - remaining;
    return r;
}

} // namespace Potato::Gameplay
