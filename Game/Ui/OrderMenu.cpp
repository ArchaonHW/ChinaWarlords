#include "Game/Ui/OrderMenu.h"

#include "Campaign/World/March.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"

namespace Potato::Game {

using Campaign::WorldMap;
using Campaign::WorldState;

std::vector<MarchOption>
MarchOptions(const WorldMap& map, const WorldState& ws,
             std::int64_t supplyAvailable,
             const Campaign::MarchRules& rules) {
    std::vector<MarchOption> out;
    // One in-flight march at a time — an en-route warband takes
    // no new order (IssueMarch would reject anyway).
    if (ws.Marching()) return out;
    const std::string& from = ws.WarbandAt();
    if (from.empty()) return out;
    const std::size_t fi = map.NodeIndexOf(from);
    if (fi == WorldMap::NO_NODE) return out;
    for (const auto& [idx, days] : map.Neighbors(fi)) {
        MarchOption o;
        o.nodeId = map.NodeAt(idx).id;
        o.name = map.NodeAt(idx).name;
        o.days = days;
        o.supply = days * rules.supplyPerDay;
        o.affordable = o.supply > 0 && supplyAvailable >= o.supply;
        out.push_back(std::move(o));
    }
    return out;
}

std::vector<MarchOption>
MarchOptions(const WorldMap& map, const WorldState& ws,
             std::int64_t supplyAvailable) {
    return MarchOptions(map, ws, supplyAvailable,
                        Campaign::MarchRules{});
}

} // namespace Potato::Game
