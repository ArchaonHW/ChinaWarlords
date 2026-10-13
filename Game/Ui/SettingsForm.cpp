#include "Game/Ui/SettingsForm.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Json/JsonValue.h"

#include <fstream>
#include <system_error>

namespace Potato::Game {

using Gameplay::JsonValue;
using Gameplay::Result;

const char* HudDensityName(HudDensity d) {
    switch (d) {
    case HudDensity::Compact: return "compact";
    case HudDensity::Standard: return "standard";
    case HudDensity::Roomy: return "roomy";
    }
    return "standard";
}

const char* HudDensityLabel(HudDensity d) {
    switch (d) {
    case HudDensity::Compact: return "緊湊";
    case HudDensity::Standard: return "標準";
    case HudDensity::Roomy: return "疏朗";
    }
    return "標準";
}

float DensityScale(HudDensity d) {
    switch (d) {
    case HudDensity::Compact: return 0.85f;
    case HudDensity::Standard: return 1.0f;
    case HudDensity::Roomy: return 1.15f;
    }
    return 1.0f;
}

namespace {

bool HudDensityFromName(std::string_view name, HudDensity& out) {
    for (int i = 0; i < kHudDensityCount; ++i) {
        const auto d = static_cast<HudDensity>(i);
        if (HudDensityName(d) == name) {
            out = d;
            return true;
        }
    }
    return false;
}

std::string ResName(const Resolution& r) {
    return std::to_string(r.w) + "x" + std::to_string(r.h);
}

} // namespace

Result<JsonValue> GameSettings::ToJson() const {
    if (volume < MIN_VOLUME || volume > MAX_VOLUME ||
        resolutionIndex < 0 ||
        resolutionIndex >= kResolutionCount) {
        return Gameplay::Fail<JsonValue>("state",
                                         "settings out of range");
    }
    return Gameplay::Ok(JsonValue::MakeObject({
        {"schema", JsonValue::String(std::string(SCHEMA))},
        {"volume", JsonValue::Int(volume)},
        {"resolution", JsonValue::String(ResName(Res()))},
        {"hud_density",
         JsonValue::String(HudDensityName(density))},
    }));
}

Result<GameSettings> GameSettings::FromJson(const JsonValue& doc) {
    if (!doc.IsObject() || doc["schema"].AsString() != SCHEMA) {
        return Gameplay::Fail<GameSettings>("schema",
                                            "not potato.settings/1");
    }
    GameSettings s;
    if (doc.Has("volume")) {
        const auto& v = doc["volume"];
        if (!v.IsInt() || v.AsInt() < MIN_VOLUME ||
            v.AsInt() > MAX_VOLUME) {
            return Gameplay::Fail<GameSettings>(
                "volume", "volume out of range 0-100");
        }
        s.volume = static_cast<int>(v.AsInt());
    }
    if (doc.Has("resolution")) {
        const auto& r = doc["resolution"];
        if (!r.IsString()) {
            return Gameplay::Fail<GameSettings>(
                "resolution", "resolution must be a string");
        }
        bool found = false;
        for (int i = 0; i < kResolutionCount; ++i) {
            if (ResName(kResolutions[i]) == r.AsString()) {
                s.resolutionIndex = i;
                found = true;
                break;
            }
        }
        if (!found) {
            return Gameplay::Fail<GameSettings>(
                "resolution", "unknown window size");
        }
    }
    if (doc.Has("hud_density")) {
        const auto& d = doc["hud_density"];
        if (!d.IsString() ||
            !HudDensityFromName(d.AsString(), s.density)) {
            return Gameplay::Fail<GameSettings>(
                "hud_density", "unknown density name");
        }
    }
    return Gameplay::Ok(s);
}

Result<std::filesystem::path> GameSettings::Save(
    const std::filesystem::path& path) const {
    auto doc = ToJson();
    if (!doc.ok()) {
        return Gameplay::Fail<std::filesystem::path>(doc.error,
                                                     doc.reason);
    }
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(),
                                            ec);
        if (ec) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "cannot create dir: " + ec.message());
        }
    }
    const auto tmp = path.parent_path() /
                     (path.filename().string() + ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "cannot open tmp for write");
        }
        out << doc.value.Emit();
        out.flush();
        out.close(); // explicit — a close-time write error is
                     // observable here
        if (!out) {
            return Gameplay::Fail<std::filesystem::path>(
                "io", "tmp write failed");
        }
    } // closed before rename (Windows forbids moving open files)
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        return Gameplay::Fail<std::filesystem::path>(
            "io", "rename failed: " + ec.message());
    }
    return Gameplay::Ok(path);
}

Result<GameSettings> GameSettings::Load(
    const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Gameplay::Ok(GameSettings{}); // first run
    }
    const std::u8string u8 = path.u8string();
    auto doc = Gameplay::Json::Load(
        std::string_view(
            reinterpret_cast<const char*>(u8.data()), u8.size()),
        SCHEMA);
    if (!doc.ok()) {
        return Gameplay::Fail<GameSettings>(doc.error, doc.reason);
    }
    return FromJson(doc.value);
}

void CycleResolution(GameSettings& s, int dir) {
    s.resolutionIndex = (s.resolutionIndex + dir +
                         kResolutionCount) %
                        kResolutionCount;
}

void CycleDensity(GameSettings& s, int dir) {
    const int i = static_cast<int>(s.density);
    s.density = static_cast<HudDensity>(
        (i + dir + kHudDensityCount) % kHudDensityCount);
}

void StepVolume(GameSettings& s, int steps) {
    s.volume += steps * GameSettings::VOLUME_STEP;
    if (s.volume < GameSettings::MIN_VOLUME)
        s.volume = GameSettings::MIN_VOLUME;
    if (s.volume > GameSettings::MAX_VOLUME)
        s.volume = GameSettings::MAX_VOLUME;
}

} // namespace Potato::Game
