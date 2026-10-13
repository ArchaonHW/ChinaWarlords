#pragma once

#include "Gameplay/Result.h"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace Potato::Gameplay {
class JsonValue;
} // namespace Potato::Gameplay

namespace Potato::Game {

// Settings panel (design §全域覆層 — 設定: 音量/解析度/HUD
// 密度). A headless model + a potato.settings/1 document;
// persisting keeps the same temp+rename discipline SaveSystem
// keeps — a settings file is a save file.
//
// What each knob honestly controls in the v0 shell:
//   volume          — stored for the mixer that does not exist
//                     yet: it round-trips and nothing else. Not
//                     decorative — the persisted preference.
//   resolutionIndex — the WINDOW size; the logical framebuffer
//                     stays 960x600 and the blit scales — the
//                     shipped raster never changes.
//   density         — ImGui style scale via DensityScale; the
//                     shell applies it once at init and on
//                     every 儲存.
//
// Edit flow: the panel mutates a DRAFT copy; 儲存 commits +
// writes + applies, 返回 discards — a canceled settings page
// must leave no residue.

// The UI-scale knob — this OWNS the name "HudDensity" in the
// Game layer (HudView's widget knob renamed to HudVerbosity to
// make room; see the note in HudView.h).
enum class HudDensity : std::uint8_t {
    Compact = 0, // "compact"  — 0.85x
    Standard,    // "standard" — 1.00x
    Roomy,       // "roomy"    — 1.15x
};
constexpr int kHudDensityCount = 3;
const char* HudDensityName(HudDensity d);  // wire spelling
const char* HudDensityLabel(HudDensity d); // CJK display
float DensityScale(HudDensity d);

struct Resolution {
    int w, h;
};
inline constexpr Resolution kResolutions[] = {
    {960, 600}, {1280, 800}, {1440, 900}, {1600, 1000},
    {1920, 1200},
};
constexpr int kResolutionCount = 5;

struct GameSettings {
    static constexpr std::string_view SCHEMA =
        "potato.settings/1";
    static constexpr int MIN_VOLUME = 0;
    static constexpr int MAX_VOLUME = 100;
    static constexpr int VOLUME_STEP = 5;

    int volume = 80;
    int resolutionIndex = 0; // index into kResolutions
    HudDensity density = HudDensity::Standard;

    Resolution Res() const { return kResolutions[resolutionIndex]; }

    // Absent members take defaults (forward-compat); present-but-
    // invalid members reject the document — strict content, no
    // partial mutation.
    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<GameSettings> FromJson(
        const Gameplay::JsonValue& doc);

    // Atomic write — tmp + rename, SaveSystem's contract.
    Gameplay::Result<std::filesystem::path> Save(
        const std::filesystem::path& path) const;
    // A missing file yields defaults (first run is not an
    // error); a PRESENT but malformed file is an error —
    // silently resetting someone's settings is worse.
    static Gameplay::Result<GameSettings> Load(
        const std::filesystem::path& path);
};

// The ◀ ▶ widgets' math — wrap cycles, clamped steps.
void CycleResolution(GameSettings& s, int dir);
void CycleDensity(GameSettings& s, int dir);
void StepVolume(GameSettings& s, int steps); // ±1 = ±VOLUME_STEP

} // namespace Potato::Game
