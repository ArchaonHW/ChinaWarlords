#pragma once

#include "Campaign/Ledger/Ledger.h"
#include "Gameplay/Result.h"

#include <cstdint>

namespace Potato::Campaign {

// The 天命 spend gate (Story 5.3). The ledger itself deliberately
// permits negative balances — a 民心 deficit is meaningful ledger
// state — so "insufficient funds" cannot live inside Post: it is a
// MYTH-ACTION rule, enforced here at the spend boundary. Story 5.4's
// myth actions (pacify shrine, invoke possession, raise ghost
// armies) all pay through this one gate; an action that cannot be
// afforded posts NOTHING — there is no myth credit, no overdraft.
//
// The caller builds the Posting: `debit.account` must be Mandate
// (this is the 天命 gate — spend a different account through Post
// directly) and `debit.amount` is the action's cost. The credit leg
// is the action's own sink — each deed names where the spent favor
// lands (pacifying banks 民心 awe, a ghost army banks 軍威 dread);
// asymmetric magnitudes are the ledger's design, not a bug.
//
// Forged mandate spends FOR REAL: a forged entry folds into
// Balance like an honest one (Story 2.3 — suspicion is data, not
// corruption), so enemy-injected 天命 grants genuinely fund actions
// until an audit flags them. The check reads the fold, not
// provenance.
Gameplay::Result<std::uint64_t> SpendMandate(Ledger& ledger,
                                             Posting p);

} // namespace Potato::Campaign
