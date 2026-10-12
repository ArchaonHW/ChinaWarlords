#pragma once

#include "Campaign/Chain/MintOutbox.h"
#include "Campaign/Ledger/CrossCheck.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// The replay gate (Story 11.3): a claim may only reach the outbox
// when the chronicle can prove it — the record must cross-check
// Clean against the ledger chain. The gate writes ONLY into the
// outbox; a refused claim leaves no artifact except the refusal
// the caller confesses in the report stream (CrossCheck flags
// diverging anchors suspect inside its own contract).
struct ClaimGateReport {
    CrossVerdict verdict = CrossVerdict::BadRecord;
    std::uint64_t recordRoot = 0; // recomputed root the gate bound
    // Committed outbox files — settlement claim first, then
    // achievement claims in id order.
    std::vector<std::filesystem::path> emitted;
};

// Achievement trigger keys — the AC's named set. Charset is the
// MintClaim achievement alphabet [a-z0-9_].
inline constexpr std::string_view ACH_ZERO_COMBAT = "zero_combat";
inline constexpr std::string_view ACH_FORGERY_BUST = "forgery_bust";
inline constexpr std::string_view ACH_FOUR_VOICE =
    "four_voice_ending";

// Cross-checks `recordDoc` (the raw potato.battle_record doc)
// against `ledger`, and only on verdict Clean emits the chapter-
// settlement claim plus one achievement claim per key through
// `outbox`. A non-Clean verdict emits NOTHING — the verdict rides
// back in the report; refusal is data, not an error.
//
// The gate ADDS "forgery_bust" itself when the cross-check finds
// forged anchors claiming the record — only the gate sees that
// evidence; caller-passed keys cover the external triggers
// (zero_combat, four_voice_ending, …). Ids dedupe — emit each
// claim once.
//
// The root bound is `recomputedRoot` (for Clean, declared ==
// recomputed — the gate binds what it proved, not what was said).
// Ledger seal fields stamp at emission time (Size()/Tip()). Every
// claim self-validates ToJson → FromJson before commit; an outbox
// failure propagates as Fail (files already written remain —
// idempotent re-emission heals the set).
Gameplay::Result<ClaimGateReport> EmitGatedClaims(
    Ledger& ledger, const Gameplay::JsonValue& recordDoc,
    std::string_view resolution, std::int64_t chapter,
    std::span<const std::string_view> achievementKeys,
    const MintOutbox& outbox);

// Encounter variant (Story 12.5): same gate, same Clean verdict
// requirement, but the claim is the generalized "settlement"
// kind — encounter/node bound, no chapter field, and no
// achievement claims (those stay chapter-keyed).
Gameplay::Result<ClaimGateReport> EmitGatedSettlement(
    Ledger& ledger, const Gameplay::JsonValue& recordDoc,
    std::string_view resolution, std::string_view encounterId,
    std::string_view node, const MintOutbox& outbox);

} // namespace Potato::Campaign
