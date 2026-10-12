#include "Campaign/Myth/MythState.h"

#include "Gameplay/Json/JsonValue.h"

namespace Potato::Campaign {

namespace {

// Strict decimal key parse — region keys are map object keys
// ("0".."1023"), so hand-rolled to reject signs, whitespace, junk,
// and non-canonical spellings ("01", "007") that would alias a
// different canonical key — wire keys must round-trip byte-exact.
bool RegionKeyFromString(const std::string& s, int& out) {
    if (s.empty() || s.size() > 4) return false; // 0..1023 max 4 digits
    if (s.size() > 1 && s[0] == '0') return false; // leading zero
    std::int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
        if (v >= static_cast<std::int64_t>(
                     MythState::MAX_REGIONS_PER_CHAPTER)) {
            return false;
        }
    }
    out = static_cast<int>(v);
    return true;
}

bool ChapterIdOk(std::string_view id) {
    return !id.empty() && id.size() <= MythState::MAX_ID_LEN;
}

} // namespace

int MythState::LevelAt(std::string_view chapterId, int region) const {
    const auto ci = byChapter_.find(std::string(chapterId));
    if (ci == byChapter_.end()) return 0;
    const auto ri = ci->second.find(region);
    return ri == ci->second.end() ? 0 : ri->second;
}

bool MythState::Set(std::string_view chapterId, int region,
                    int level) {
    if (!ChapterIdOk(chapterId) || region < 0 ||
        region >= static_cast<int>(MAX_REGIONS_PER_CHAPTER) ||
        level < 0 ||
        level >= static_cast<int>(
                     Gameplay::kInfiltrationLevelCount)) {
        return false;
    }
    // All fallible checks precede mutation — a rejected call leaves
    // no half-created chapter entry.
    const auto ci = byChapter_.find(std::string(chapterId));
    if (level == 0) {
        // A 0-write to an unknown chapter is a no-op (nothing to
        // erase); canonical form stores only levels 1..3.
        if (ci != byChapter_.end()) {
            ci->second.erase(region);
            if (ci->second.empty()) {
                byChapter_.erase(ci);
            }
        }
        return true;
    }
    if (ci == byChapter_.end() &&
        byChapter_.size() >= MAX_CHAPTERS) {
        return false;
    }
    auto& regions = byChapter_[std::string(chapterId)];
    if (regions.size() >= MAX_REGIONS_PER_CHAPTER &&
        regions.find(region) == regions.end()) {
        return false;
    }
    regions[region] = static_cast<std::uint8_t>(level);
    return true;
}

void MythState::ClearChapter(std::string_view chapterId) {
    byChapter_.erase(std::string(chapterId));
}

Gameplay::Result<Gameplay::JsonValue> MythState::ToJson() const {
    using Gameplay::JsonValue;
    JsonValue::Object chapters;
    for (const auto& [id, regions] : byChapter_) {
        JsonValue::Object rs;
        for (const auto& [region, level] : regions) {
            rs[std::to_string(region)] =
                JsonValue::Int(static_cast<std::int64_t>(level));
        }
        chapters[id] = JsonValue::MakeObject(std::move(rs));
    }
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["chapters"] = JsonValue::MakeObject(std::move(chapters));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Gameplay::Result<MythState> MythState::FromJson(
    const Gameplay::JsonValue& doc) {
    using Gameplay::Fail;
    const std::string* s = doc.FindString("schema");
    if (!s || *s != SCHEMA) {
        return Fail<MythState>("schema", "expected potato.myth/1");
    }
    const auto& chapters = doc["chapters"];
    if (!chapters.IsObject()) {
        return Fail<MythState>("schema", "chapters: not an object");
    }
    if (chapters.Members().size() > MAX_CHAPTERS) {
        return Fail<MythState>("myth", "too many chapters");
    }
    MythState out;
    for (const auto& [id, regions] : chapters.Members()) {
        if (!ChapterIdOk(id)) {
            return Fail<MythState>("myth", "bad chapter id");
        }
        if (!regions.IsObject()) {
            return Fail<MythState>("schema",
                                   "chapter regions: not an object");
        }
        if (regions.Members().size() > MAX_REGIONS_PER_CHAPTER) {
            return Fail<MythState>("myth", "too many regions");
        }
        for (const auto& [rk, lv] : regions.Members()) {
            int region;
            if (!RegionKeyFromString(rk, region)) {
                return Fail<MythState>("schema",
                                       "bad region key");
            }
            if (!lv.IsInt() || lv.AsInt() < 1 ||
                lv.AsInt() >= static_cast<std::int64_t>(
                    Gameplay::kInfiltrationLevelCount)) {
                return Fail<MythState>(
                    "schema", "level out of range 1..3 "
                              "(canonical form stores no zeros)");
            }
            out.byChapter_[id][region] =
                static_cast<std::uint8_t>(lv.AsInt());
        }
    }
    return Gameplay::Ok(std::move(out));
}

Gameplay::Result<int> CommitMyth(MythState& state,
                                 std::string_view chapterId,
                                 const Gameplay::MythField& field) {
    using Gameplay::Fail;
    // Preflight EVERY fallible condition before ClearChapter — a
    // rejected commit must leave the stored state untouched.
    if (!ChapterIdOk(chapterId)) {
        return Fail<int>("myth", "bad chapter id");
    }
    if (field.RegionCount() > MythState::MAX_REGIONS_PER_CHAPTER) {
        return Fail<int>("myth", "field exceeds region cap");
    }
    std::size_t nonzero = 0;
    for (std::size_t r = 0; r < field.RegionCount(); ++r) {
        if (field.LevelAt(r) != Gameplay::InfiltrationLevel::None) {
            ++nonzero;
        }
    }
    if (nonzero > 0 && !state.HasChapter(chapterId) &&
        state.ChapterCount() >= MythState::MAX_CHAPTERS) {
        return Fail<int>("myth", "chapter book full");
    }
    // Cleared first so regions a map revision dropped can't linger;
    // every Set below is provably infallible after preflight
    // (valid id, regions < cap, chapter slot secured).
    state.ClearChapter(chapterId);
    int written = 0;
    for (std::size_t r = 0; r < field.RegionCount(); ++r) {
        const int level = static_cast<int>(field.LevelAt(r));
        if (level != 0) {
            state.Set(chapterId, static_cast<int>(r), level);
            ++written;
        }
    }
    return Gameplay::Ok(written);
}

} // namespace Potato::Campaign
