#pragma once

#include "Campaign/Ledger/Ledger.h"
#include "Campaign/Myth/MythLog.h"
#include "Gameplay/Myth/Infiltration.h" // MythActionKind
#include "Gameplay/Result.h"

#include <cstdint>
#include <span>

namespace Potato::Campaign {

// The myth-action catalog (Story 5.4): every 天命-spending verb the
// player can author. The sim-side effect lives in Gameplay
// (MythField / Squad via the IssueMyth* verbs); THIS table prices
// it — 天命 debit through SpendMandate and a credit sink that banks
// the deed's echo into the ledger's other accounts.
//
// Sink choices are the GDD's "mythic actions amplify the 民心 axis":
// pacifying converts into governance capital directly, possession
// honors the host's line (武功), a ghost army banks dread (軍威).
// Costs track the GDD tempo table (~15 early / ~25 mid bands).
struct MythActionDef {
    Gameplay::MythActionKind kind;
    const char* id;          // canonical ASCII wire id
    const char* name;        // chronicle name — MythLog stores this
    std::int64_t cost;       // 天命 debit
    Account sink;            // credit leg — where the echo lands
    std::int64_t sinkAmount;
    bool needsSquad;         // target is a squad (possession), not
                             // a region (pacify / ghost army)
};

// The catalog — array order is the enum order.
std::span<const MythActionDef> MythActionDefs();
const MythActionDef* FindMythAction(Gameplay::MythActionKind kind);

// Stance-modulated price (Story 5.7): the deity's mood is a market
// force — Favorable discounts a miracle (god's favor makes miracles
// cheap), Wrathful surcharges it (appeasing an angry god costs
// more). The floor keeps a favored act from becoming free — even a
// pleased god takes an offering.
// Caller sources the stance from MythField::StanceAt(region, side)
// — the sim owns the mood, the ledger prices it.
std::int64_t EffectiveCost(Gameplay::MythActionKind kind,
                           Gameplay::GodStance stance);

// The purchase seam: validate the kind, pay 天命 through
// SpendMandate (insufficient funds reject BEFORE anything posts),
// and enter the act into the MythLog BY NAME. The host then issues
// the battle verb — pairing the spend to a sim effect is the
// caller's contract (an unpaid act in a record is an audit finding;
// a paid act never issued is just a donation).
//
// `stance` modulates the 天命 cost through EffectiveCost — pass
// the deity's disposition toward `side` at the target region's
// shrine (Neutral for non-shrine ground or unknown).
//
// Returns the ledger seq of the spend entry — the durable anchor
// tying the miracle to its price.
Gameplay::Result<std::uint64_t>
PerformMythAction(Ledger& ledger, MythLog& log,
                  Gameplay::MythActionKind kind, int side,
                  int region, int squad,
                  Gameplay::GodStance stance =
                      Gameplay::GodStance::Neutral);

} // namespace Potato::Campaign
