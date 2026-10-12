#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Potato::Gameplay {

// Outcome of verifying a potato.battle_record/1 document.
//   ok        — record is intact AND replays bit-exact.
//   tampered  — integrity root mismatch (byte-level edit detected
//               before any simulation work).
//   downgrade — record stamped by an older toolVersion (FR12 warning;
//               still verified, not rejected).
//   reason    — first divergence / rejection detail.
//   checksum  — replayed checksum when ok.
struct VerifyResult {
    bool ok = false;
    bool tampered = false;
    bool downgrade = false;
    std::string reason;
    std::uint64_t checksum = 0;
};

namespace Replay {

// The audit IS the replay: verify = recompute the integrity root over
// the payload, rebuild the sim from the embedded docs + seed, replay the
// recorded inputs (planning ops + Intervention events re-issued at their
// original ticks through the same Issue* validation), then compare the
// event stream, checksum, and outcome bit-exact.
Result<VerifyResult> Verify(const JsonValue& doc);

// Convenience: load + verify. File errors surface as Result failures.
Result<VerifyResult> VerifyFile(std::string_view path);

} // namespace Replay

} // namespace Potato::Gameplay
