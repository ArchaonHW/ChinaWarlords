#include "Campaign/Ledger/DeedBook.h"

#include <string>
#include <utility>

namespace Potato::Campaign {

namespace {

using Gameplay::SimEvent;

// Tag vocabulary — the fold keys Epic 4's accumulators read.
// TAG_ATROCITY (Ledger.h) marks the unforgivable; "raid" marks
// commerce raiding (war, not atrocity); region/victim tags keep
// the fold placeable.
// Story 12.7: `field:<n>` is the battle-map-local region index —
// `region:` is reserved for world node ids.
std::string FieldTag(int region) {
    return std::string(Ledger::TAG_FIELD) + std::to_string(region);
}

// deed -> posting legs; returns false for non-deed kinds.
// `playerSide` is only consulted by MythInvasion — a visitation's
// legs depend on whose banner the god's host carried.
bool DeedPosting(const SimEvent& e, Posting& p, int playerSide) {
    switch (e.kind) {
        case SimEvent::Kind::VillageOccupied:
            p = {{Account::PopularSupport, 10}, {Account::Materiel, 5},
                 "occupied village", {FieldTag(e.param), "order:+5"}};
            return true;
        case SimEvent::Kind::VillageBurned:
            p = {{Account::Materiel, 40}, {Account::PopularSupport, 15},
                 "burned village",
                 {std::string(Ledger::TAG_ATROCITY), FieldTag(e.param),
                  "order:-10", "corruption:+15"}};
            return true;
        case SimEvent::Kind::ConvoyArrived:
            p = {{Account::Materiel, 30}, {Account::PopularSupport, 5},
                 "convoy arrived", {FieldTag(e.param), "order:+2"}};
            return true;
        case SimEvent::Kind::ConvoyRaided:
            p = {{Account::Materiel, 25}, {Account::PopularSupport, 5},
                 "raided convoy",
                 {"raid", FieldTag(e.param), "order:-3"}};
            return true;
        case SimEvent::Kind::ShrineCaptured:
            // Dedication earns 天命; the debit leg is 物資 — offerings
            // consume materiel. Story 5.4's "pacify shrine" action is
            // a different verb (a spend); this is the earn side.
            p = {{Account::Mandate, 10}, {Account::Materiel, 5},
                 "dedicated shrine",
                 {std::string(Ledger::TAG_MYTH), FieldTag(e.param),
                  "order:+2"}};
            return true;
        case SimEvent::Kind::MythInvasion:
            // A visitation, not a deed — it books regardless of whose
            // banner the host carried (the loop's side-gate is
            // bypassed for this kind). Blessing: the god's army
            // marches for us and bills the debt to 天命 — 神助要還.
            // Terror: an enemy/wild host empties hearts and stiffens
            // ranks.
            if (e.side == playerSide) {
                p = {{Account::ArmyPrestige, 5}, {Account::Mandate, 5},
                     "the god's host marches for us",
                     {std::string(Ledger::TAG_MYTH), FieldTag(e.param),
                      "invasion"}};
            } else {
                p = {{Account::ArmyPrestige, 2},
                     {Account::PopularSupport, 4},
                     "spirit host terror",
                     {std::string(Ledger::TAG_MYTH), FieldTag(e.param),
                      "invasion"}};
            }
            return true;
        case SimEvent::Kind::SquadExecuted:
            // "victim:" names the murdered squad — every other kind's
            // squadIndex is the perpetrator; conflating them would
            // attribute the atrocity to the wrong index.
            p = {{Account::ArmyPrestige, 5},
                 {Account::PopularSupport, 10},
                 "refused rout-surrender",
                 {std::string(Ledger::TAG_ATROCITY),
                  "victim:" + std::to_string(e.squadIndex),
                  FieldTag(e.param), "order:-5", "corruption:+20"}};
            return true;
        default:
            return false; // CardFired/BeatChanged/... aren't deeds
    }
}

} // namespace

Gameplay::Result<std::size_t>
BookDeeds(Ledger& ledger, int playerSide,
          std::span<const Gameplay::SimEvent> events,
          std::string_view worldNode) {
    if (playerSide != 0 && playerSide != 1) {
        return Gameplay::Fail<std::size_t>("deeds",
                                           "side must be 0 or 1");
    }
    if (worldNode.size() + Ledger::TAG_REGION.size() >
        Ledger::MAX_TAG_LEN) {
        return Gameplay::Fail<std::size_t>(
            "deeds", "world node id exceeds tag bound");
    }
    const std::string worldTag =
        worldNode.empty()
            ? std::string()
            : std::string(Ledger::TAG_REGION) +
                  std::string(worldNode);
    std::size_t posted = 0;
    for (const SimEvent& e : events) {
        // Only the player's deeds book into the player's ledger —
        // an enemy arsonist gains nothing from our 物資 account.
        // MythInvasion is exempt: a visitation lands on the player's
        // chronicle whether the god marched for us or against us.
        if (e.kind != SimEvent::Kind::MythInvasion &&
            e.side != playerSide) {
            continue;
        }
        Posting p;
        if (!DeedPosting(e, p, playerSide)) continue;
        if (!worldTag.empty()) p.tags.push_back(worldTag);
        auto r = ledger.Post(std::move(p));
        // A rejected deed aborts the fold. Entries already posted
        // STAY posted — the chain is append-only history, not a
        // transaction — so the failure reason names the count and
        // a retry must resume, not re-fold (no idempotency key).
        if (!r.ok()) {
            return Gameplay::Fail<std::size_t>(
                "deeds", "posting rejected after " +
                             std::to_string(posted) +
                             " deeds: " + r.reason);
        }
        ++posted;
    }
    return Gameplay::Ok(posted);
}

} // namespace Potato::Campaign
