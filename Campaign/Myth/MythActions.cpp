#include "Campaign/Myth/MythActions.h"

#include "Campaign/Myth/Mandate.h"

#include <string>

namespace Potato::Campaign {

namespace {

// The catalog — costs/sinks are initial balance targets from the
// GDD tempo table (earn ~10/chapter early; actions ~15-25).
constexpr MythActionDef DEFS[] = {
    {Gameplay::MythActionKind::PacifyShrine,
     "pacify_shrine", "安撫", 15, Account::PopularSupport, 10,
     false},
    {Gameplay::MythActionKind::InvokePossession,
     "invoke_possession", "降神", 20, Account::MartialMerit, 6,
     true},
    {Gameplay::MythActionKind::RaiseGhostArmy,
     "ghost_army", "陰兵", 25, Account::ArmyPrestige, 8, false},
};

} // namespace

std::span<const MythActionDef> MythActionDefs() {
    return {DEFS, sizeof(DEFS) / sizeof(DEFS[0])};
}

const MythActionDef* FindMythAction(Gameplay::MythActionKind kind) {
    for (const MythActionDef& d : DEFS) {
        if (d.kind == kind) return &d;
    }
    return nullptr;
}

std::int64_t EffectiveCost(Gameplay::MythActionKind kind,
                           Gameplay::GodStance stance) {
    const MythActionDef* def = FindMythAction(kind);
    if (def == nullptr) return 0;
    switch (stance) {
        case Gameplay::GodStance::Favorable:
            // Favor discounts, never gifts — the floor keeps a
            // pleased god's price real.
            return def->cost - 5 > 5 ? def->cost - 5 : 5;
        case Gameplay::GodStance::Wrathful:
            return def->cost + 10;
        case Gameplay::GodStance::Neutral:
        default:
            return def->cost;
    }
}

Gameplay::Result<std::uint64_t>
PerformMythAction(Ledger& ledger, MythLog& log,
                  Gameplay::MythActionKind kind, int side,
                  int region, int squad,
                  Gameplay::GodStance stance) {
    const MythActionDef* def = FindMythAction(kind);
    if (def == nullptr) {
        return Gameplay::Fail<std::uint64_t>("mythact",
                                             "unknown action");
    }
    if (side != 0 && side != 1) {
        return Gameplay::Fail<std::uint64_t>("mythact",
                                             "side must be 0 or 1");
    }
    if (region < 0 || (def->needsSquad && squad < 0) ||
        (!def->needsSquad && squad != -1)) {
        return Gameplay::Fail<std::uint64_t>("mythact",
                                             "bad target shape");
    }
    Posting p;
    p.credit = {def->sink, def->sinkAmount};
    p.debit = {Account::Mandate, EffectiveCost(kind, stance)};
    p.memo = std::string(def->id);
    p.tags = {std::string(Ledger::TAG_MYTH),
              std::string("action:") + def->id,
              std::string(Ledger::TAG_FIELD) +
                  std::to_string(region)};
    auto r = SpendMandate(ledger, std::move(p));
    if (!r.ok()) {
        return Gameplay::Fail<std::uint64_t>("mythact", r.reason);
    }
    // Paid — the deed enters the chronicle by name.
    auto lr = log.Record(def->id, def->name, side, region,
                         def->needsSquad ? squad : -1);
    if (!lr.ok()) {
        // Spend landed but the log refused — the ledger is the
        // harder truth; surface the failure honestly.
        return Gameplay::Fail<std::uint64_t>(
            "mythact", "paid but unlogged: " + lr.reason);
    }
    return r;
}

} // namespace Potato::Campaign
