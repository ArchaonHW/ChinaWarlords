#pragma once

#include "Campaign/State/CampaignState.h"
#include "Gameplay/Result.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Durability layer over CampaignState: tmp→rename atomic writes,
// schema-gated loads. The only file-I/O surface in the campaign
// layer (sim/tick paths stay I/O-free).
//
// Save: emit the canonical JSON to `save_<slot>.tmp`, then rename
// over `save_<slot>.json` — a crash mid-write leaves a truncated
// .tmp while the previous .json stays valid. On Windows,
// std::filesystem::rename maps to MoveFileExW with
// MOVEFILE_REPLACE_EXISTING (MSVC, and MinGW GCC >= 11 — verified
// toolchain GCC 16). A rename failure NEVER touches the committed
// save; the complete new data stays in .tmp.
//
// Boundary honesty: this is process-crash atomicity, not
// power-loss durability — no fsync on the file or directory. And
// it assumes a single writer per save dir: two concurrent Saves to
// one slot share the tmp name and can commit torn content.
//
// Load: Json::Load (64 MiB cap, UTF-16 reject, BOM strip, schema
// gate) → CampaignState::FromJson. Every failure is a typed Fail;
// the caller's state is structurally untouchable (load returns a
// fresh value).
class SaveSystem {
public:
    static constexpr int MAX_SLOTS = 8;
    static constexpr std::string_view SAVE_EXT = ".json";
    static constexpr std::string_view TMP_EXT = ".tmp";

    explicit SaveSystem(std::filesystem::path dir)
        : dir_(std::move(dir)) {}

    const std::filesystem::path& Dir() const { return dir_; }

    // Canonical slot paths (slot bounds-checked by Save/Load).
    static std::filesystem::path SlotPath(
        const std::filesystem::path& dir, int slot);
    static std::filesystem::path TmpPath(
        const std::filesystem::path& dir, int slot);

    Gameplay::Result<std::filesystem::path> Save(
        int slot, const CampaignState& state) const;
    Gameplay::Result<CampaignState> Load(int slot) const;

    // Enumerates present save_N.json slots (N < MAX_SLOTS), sorted.
    std::vector<int> ListSlots() const;

private:
    std::filesystem::path dir_;
};

} // namespace Potato::Campaign
