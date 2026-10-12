#include "Campaign/World/March.h"

#include "Campaign/Ledger/Ledger.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"

#include <utility>
#include <vector>

namespace Potato::Campaign {

using Gameplay::Result;

Result<MarchPlan> IssueMarch(WorldState& ws, const WorldMap& map,
                             Ledger& ledger, std::string_view dest,
                             const MarchRules& rules) {
    if (ws.Marching()) {
        return Gameplay::Fail<MarchPlan>(
            "world", "warband already marching — one order at a time");
    }
    const std::string& from = ws.WarbandAt();
    const std::size_t fi = map.NodeIndexOf(from);
    const std::size_t ti = map.NodeIndexOf(dest);
    if (fi == WorldMap::NO_NODE || ti == WorldMap::NO_NODE) {
        return Gameplay::Fail<MarchPlan>("world",
                                         "march endpoints unknown");
    }
    // Single-hop: dest must be adjacent along a route.
    std::int64_t days = -1;
    for (const auto& [idx, d] : map.Neighbors(fi)) {
        if (idx == ti) {
            days = d;
            break;
        }
    }
    if (days < 0) {
        return Gameplay::Fail<MarchPlan>(
            "world", "no route between '" + from + "' and '" +
                         std::string(dest) + "'");
    }
    // 欠帳行軍：物資可負——affordability 是呈現層的事
    // （MarchOptions.affordable），sim 只負責記帳。軍隊開拔，
    // 帳本照記，管它能不能活到目的地。
    const std::int64_t supply = days * rules.supplyPerDay;
    // Enqueue's fallible conditions are preflighted here (node
    // exists, day >= Day() by construction, queue under cap) so the
    // ledger post below can't strand without its arrival event.
    if (ws.Pending().size() >= WorldState::MAX_EVENTS) {
        return Gameplay::Fail<MarchPlan>("world",
                                         "event queue full");
    }
    if (days > WorldState::MAX_DAY - ws.Day()) {
        return Gameplay::Fail<MarchPlan>("world",
                                         "arrival day out of range");
    }

    // Commit order: ledger first (the army eats whether or not it
    // survives the road), then the queued arrival. A zero-cost
    // march (rules with supplyPerDay=0) posts nothing — Post
    // would reject a 0-amount pair.
    if (supply > 0) {
        Posting p;
        p.debit = {Account::Materiel, supply};
        p.credit = {Account::ArmyPrestige, supply};
        p.memo = "march " + from + " -> " + std::string(dest);
        p.tags = {"march",
                  std::string(Ledger::TAG_REGION) +
                      std::string(dest)};
        if (const auto r = ledger.Post(std::move(p)); !r.ok()) {
            return Gameplay::Fail<MarchPlan>(r.error, r.reason);
        }
    }

    WorldEvent ev;
    ev.day = ws.Day() + days;
    ev.seq = ws.NextSeq();
    ev.kind = WorldEventKind::March;
    ev.node = std::string(dest);
    if (const auto r = ws.Enqueue(map, std::move(ev)); !r.ok()) {
        return Gameplay::Fail<MarchPlan>(r.error, r.reason);
    }
    ws.BeginMarch(dest, ws.Day() + days);

    return Gameplay::Ok(MarchPlan{from, std::string(dest), days,
                                  supply});
}

Result<bool> IssueSighting(WorldState& ws, const WorldMap& map,
                           std::string_view node) {
    if (map.FindNode(node) == nullptr) {
        return Gameplay::Fail<bool>("world",
                                    "sighting targets unknown node");
    }
    WorldEvent ev;
    ev.day = ws.Day();
    ev.seq = ws.NextSeq();
    ev.kind = WorldEventKind::Sight;
    ev.node = std::string(node);
    return ws.Enqueue(map, std::move(ev));
}

} // namespace Potato::Campaign
