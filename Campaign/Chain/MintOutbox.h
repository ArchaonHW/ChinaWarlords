#pragma once

#include "Campaign/Chain/MintClaim.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// The outbox (Story 11.1, D-ARCH-9): one `potato.mintclaim/1` file
// per claim in a spool directory the external relayer polls
// (Story 11.4). The ONLY boundary the chain layer owns — no
// sockets, no subprocess, no tick-path calls. Emission wiring from
// the aftermath ceremony is Story 11.3; this is the mechanism.
//
// Durability mirrors SaveSystem: emit the canonical JSON to
// `<id>.tmp`, then rename over `<id>.json` — a crash mid-write
// leaves a truncated .tmp while the previous claim stays valid.
// Same honesty notes apply: process-crash atomicity, not
// power-loss durability, and a single writer per outbox dir.
//
// Idempotent by construction: claim ids are content-derived, so a
// re-emitted claim writes byte-identical content to the same path —
// a safe overwrite, not a duplicate. No dedup bookkeeping exists.
class MintOutbox {
public:
    static constexpr std::string_view CLAIM_EXT = ".json";
    static constexpr std::string_view TMP_EXT = ".tmp";
    static constexpr std::size_t MAX_CLAIMS = 4096;

    explicit MintOutbox(std::filesystem::path dir)
        : dir_(std::move(dir)) {}

    const std::filesystem::path& Dir() const { return dir_; }

    // Canonical claim path (`<dir>/<id>.json`) — filename derived
    // from ClaimId; ids are charset-bounded by FromJson so the name
    // cannot escape the directory.
    std::filesystem::path ClaimPath(const MintClaim& claim) const;

    // Emits one claim: ToJson → `<id>.tmp` → rename over
    // `<id>.json`. Creates the outbox dir if absent. Any failure
    // leaves previously committed claims untouched.
    Gameplay::Result<std::filesystem::path> Emit(
        const MintClaim& claim) const;

    // Reads every `<dir>/*.json`, schema-gates through Json::Load +
    // MintClaim::FromJson (stored id must equal recomputed). A bad
    // file lands in `rejected[]` and the scan continues; an
    // unreadable dir or >MAX_CLAIMS files fails wholesale.
    // Claims return sorted by id — canonical eval order.
    struct RejectedClaim {
        std::filesystem::path path;
        std::string error;
        std::string reason;
    };
    struct ScanResult {
        std::vector<MintClaim> claims;
        std::vector<RejectedClaim> rejected;
    };
    Gameplay::Result<ScanResult> Scan() const;

private:
    std::filesystem::path dir_;
};

} // namespace Potato::Campaign
