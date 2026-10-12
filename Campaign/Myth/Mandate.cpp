#include "Campaign/Myth/Mandate.h"

#include <string>
#include <utility>

namespace Potato::Campaign {

Gameplay::Result<std::uint64_t> SpendMandate(Ledger& ledger,
                                             Posting p) {
    if (p.debit.account != Account::Mandate) {
        return Gameplay::Fail<std::uint64_t>(
            "mandate", "spend gate only takes 天命 debits");
    }
    // Reject before folding: the Post-side validation (amount > 0,
    // leg distinctness, meta) runs after the funds check, but a
    // non-positive cost would pass a "balance >= amount" test
    // vacuously — gate it explicitly so the rejection reason names
    // the real fault.
    if (p.debit.amount <= 0) {
        return Gameplay::Fail<std::uint64_t>(
            "mandate", "myth action cost must be positive");
    }
    // Fold includes forged entries by design — an injected grant
    // really does fund the action (suspicion is an audit overlay,
    // not a balance correction).
    const std::int64_t funds = ledger.Balance(Account::Mandate);
    if (funds < p.debit.amount) {
        return Gameplay::Fail<std::uint64_t>(
            "mandate", "insufficient 天命: cost " +
                           std::to_string(p.debit.amount) + ", hold " +
                           std::to_string(funds));
    }
    return ledger.Post(std::move(p));
}

} // namespace Potato::Campaign
