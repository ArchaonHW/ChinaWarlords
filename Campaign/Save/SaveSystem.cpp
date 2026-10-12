#include "Campaign/Save/SaveSystem.h"

#include "Gameplay/Json/Json.h"

#include <fstream>
#include <string>
#include <system_error>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

bool SlotOk(int slot) { return slot >= 0 && slot < SaveSystem::MAX_SLOTS; }

} // namespace

std::filesystem::path SaveSystem::SlotPath(
    const std::filesystem::path& dir, int slot) {
    return dir / ("save_" + std::to_string(slot) +
                  std::string(SAVE_EXT));
}

std::filesystem::path SaveSystem::TmpPath(
    const std::filesystem::path& dir, int slot) {
    return dir / ("save_" + std::to_string(slot) +
                  std::string(TMP_EXT));
}

Result<std::filesystem::path> SaveSystem::Save(
    int slot, const CampaignState& state) const {
    if (!SlotOk(slot)) {
        return Gameplay::Fail<std::filesystem::path>(
            "slot", "slot out of range");
    }
    Result<JsonValue> doc = state.ToJson();
    if (!doc.ok()) {
        return Gameplay::Fail<std::filesystem::path>(doc.error,
                                                     doc.reason);
    }
    const std::string text = doc.value.Emit();

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec) {
        return Gameplay::Fail<std::filesystem::path>(
            "io", "cannot create save dir: " + ec.message());
    }

    const std::filesystem::path tmp = TmpPath(dir_, slot);
    const std::filesystem::path dst = SlotPath(dir_, slot);
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "cannot open tmp for write");
        }
        out << text;
        out.flush();
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "tmp write failed");
        }
        out.close(); // explicit: a close-time write error is
                     // observable here, swallowed in the dtor
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "tmp close failed");
        }
    } // closed before rename (Windows forbids moving open files)

    // Atomic commit — rename replaces the previous save (MoveFileExW
    // REPLACE_EXISTING on MSVC and MinGW GCC >= 11). On ANY failure
    // we return Fail without touching dst: the old save stays valid
    // and the complete new save sits in .tmp. (Review fix: a
    // remove+rename fallback was destructive — a transient rename
    // failure would delete the last good save.)
    std::filesystem::rename(tmp, dst, ec);
    if (ec) {
        return Gameplay::Fail<std::filesystem::path>(
            "io", "rename failed: " + ec.message());
    }
    return Gameplay::Ok(dst);
}

Result<CampaignState> SaveSystem::Load(int slot) const {
    if (!SlotOk(slot)) {
        return Gameplay::Fail<CampaignState>("slot",
                                             "slot out of range");
    }
    const std::filesystem::path dst = SlotPath(dir_, slot);
    std::error_code ec;
    if (!std::filesystem::exists(dst, ec)) {
        return Gameplay::Fail<CampaignState>("io",
                                             "save file missing");
    }
    // Delegate to the shared loader: 64 MiB cap, read-error check,
    // UTF-16 reject, BOM strip, schema gate — all before FromJson.
    const std::u8string u8 = dst.u8string();
    Result<JsonValue> doc = Gameplay::Json::Load(
        std::string_view(
            reinterpret_cast<const char*>(u8.data()), u8.size()),
        CampaignState::SCHEMA);
    if (!doc.ok()) {
        return Gameplay::Fail<CampaignState>(doc.error, doc.reason);
    }
    return CampaignState::FromJson(doc.value);
}

std::vector<int> SaveSystem::ListSlots() const {
    std::vector<int> out;
    for (int i = 0; i < MAX_SLOTS; ++i) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(SlotPath(dir_, i), ec)) {
            out.push_back(i);
        }
    }
    return out;
}

} // namespace Potato::Campaign
