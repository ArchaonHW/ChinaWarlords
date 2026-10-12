#pragma once

#include "Gameplay/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Potato::Campaign {

class Ledger;
class WorldMap;
class WorldState;

// The march order (Story 12.3) — the player's verb on the open
// map: pick an adjacent node, the army walks the route's day cost
// and pays 物資 per day. Arrival lands through the event queue,
// not instantly — beats decide when things happen.
//
// Supply is code-resident (RefitCamp band-table precedent): no
// potato.balance doc exists yet — the constant is parameterized
// so a future balance pass swaps the number, not the mechanic.
//
// Supply posts as debit Materiel / credit ArmyPrestige — spent
// materiel converts into the army's logged exertion, the same
// asymmetric-posting model as RefitCamp spending.

struct MarchRules {
    std::int64_t supplyPerDay = 5;
};

struct MarchPlan {
    std::string from;
    std::string to;
    std::int64_t days = 0;   // route cost
    std::int64_t supply = 0; // days * rules.supplyPerDay
};

// Issue a march order. Reject-before-mutate order: adjacency →
// already-marching → affordability → Post → Enqueue → in-flight
// marker. Any failure leaves ledger and state untouched.
//
// On success: posts the supply cost (tagged "march" +
// "region:<dest>" for the 12.7 regional fold) and queues a March
// event at day+days carrying the state's own seq counter.
Gameplay::Result<MarchPlan> IssueMarch(
    WorldState& ws, const WorldMap& map, Ledger& ledger,
    std::string_view dest, const MarchRules& rules = {});

// Record hearsay — rival banners sighted at `node`. Enqueues a
// Sight event for the current day; ResolveBeats(0) drains same-day
// events, so the caller decides when the rumor lands.
Gameplay::Result<bool> IssueSighting(WorldState& ws,
                                     const WorldMap& map,
                                     std::string_view node);

} // namespace Potato::Campaign
