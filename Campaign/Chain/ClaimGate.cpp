#include "Campaign/Chain/ClaimGate.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

// Self-check: a claim the gate can't read back is a `field`
// failure, never a file. ToJson → FromJson round-trip is the one
// validation rule — no duplicated field checks here.
Result<bool> SelfValidates(const MintClaim& c) {
    const auto doc = c.ToJson();
    if (!doc.ok()) {
        return Gameplay::Fail<bool>(doc.error, doc.reason);
    }
    const auto back = MintClaim::FromJson(doc.value);
    if (!back.ok()) {
        return Gameplay::Fail<bool>(back.error, back.reason);
    }
    return Gameplay::Ok(true);
}

Result<std::filesystem::path> EmitChecked(const MintClaim& c,
                                          const MintOutbox& outbox) {
    const auto ok = SelfValidates(c);
    if (!ok.ok()) {
        return Gameplay::Fail<std::filesystem::path>(ok.error,
                                                     ok.reason);
    }
    return outbox.Emit(c);
}

} // namespace

Result<ClaimGateReport> EmitGatedClaims(
    Ledger& ledger, const JsonValue& recordDoc,
    std::string_view resolution, std::int64_t chapter,
    std::span<const std::string_view> achievementKeys,
    const MintOutbox& outbox) {
    ClaimGateReport report;
    const CrossCheckResult xr = CrossCheckRecord(ledger, recordDoc);
    report.verdict = xr.verdict;
    report.recordRoot = xr.recomputedRoot;
    if (xr.verdict != CrossVerdict::Clean) {
        // Refusal is data, not an error — the caller confesses the
        // verdict; nothing reaches the outbox.
        return Gameplay::Ok(std::move(report));
    }

    MintClaim settlement;
    settlement.kind = ClaimKind::ChapterSettlement;
    settlement.chapter = chapter;
    settlement.recordRoot = xr.recomputedRoot;
    settlement.resolution = std::string(resolution);
    settlement.ledgerCount = ledger.Size();
    settlement.ledgerTip = ledger.Tip();
    const auto path = EmitChecked(settlement, outbox);
    if (!path.ok()) {
        return Gameplay::Fail<ClaimGateReport>(path.error,
                                               path.reason);
    }
    report.emitted.push_back(path.value);

    // The gate's own evidence: forged anchors claiming this record
    // are a forgery bust — only the cross-check can see them.
    std::set<std::string, std::less<>> keys(achievementKeys.begin(),
                                            achievementKeys.end());
    if (xr.forgedAnchors > 0) {
        keys.emplace(ACH_FORGERY_BUST);
    }
    // Id order (the set sorts and dedupes keys) — deterministic
    // emission; distinct keys can't collide (the key IS the id).
    for (const std::string& key : keys) {
        MintClaim c;
        c.kind = ClaimKind::Achievement;
        c.chapter = chapter;
        c.achievement = key;
        c.ledgerCount = ledger.Size();
        c.ledgerTip = ledger.Tip();
        const auto p = EmitChecked(c, outbox);
        if (!p.ok()) {
            return Gameplay::Fail<ClaimGateReport>(p.error,
                                                   p.reason);
        }
        report.emitted.push_back(p.value);
    }
    return Gameplay::Ok(std::move(report));
}

Result<ClaimGateReport> EmitGatedSettlement(
    Ledger& ledger, const JsonValue& recordDoc,
    std::string_view resolution, std::string_view encounterId,
    std::string_view node, const MintOutbox& outbox) {
    ClaimGateReport report;
    const CrossCheckResult xr = CrossCheckRecord(ledger, recordDoc);
    report.verdict = xr.verdict;
    report.recordRoot = xr.recomputedRoot;
    if (xr.verdict != CrossVerdict::Clean) {
        return Gameplay::Ok(std::move(report));
    }

    MintClaim c;
    c.kind = ClaimKind::Settlement;
    c.encounter = std::string(encounterId);
    c.node = std::string(node);
    c.recordRoot = xr.recomputedRoot;
    c.resolution = std::string(resolution);
    c.ledgerCount = ledger.Size();
    c.ledgerTip = ledger.Tip();
    const auto path = EmitChecked(c, outbox);
    if (!path.ok()) {
        return Gameplay::Fail<ClaimGateReport>(path.error,
                                               path.reason);
    }
    report.emitted.push_back(path.value);
    return Gameplay::Ok(std::move(report));
}

} // namespace Potato::Campaign
