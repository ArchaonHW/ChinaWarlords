#include "Game/Render/SpriteAtlas.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Json/JsonValue.h"

#include <cctype>
#include <map>
#include <set>
#include <utility>

namespace Potato::Game {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

// "#RRGGBB" or "#RRGGBBAA" → Rgba. Hex pairs only; anything else
// rejects (no "#RGB" shorthand — one spelling keeps the wire
// contract tight).
bool ParseColor(const std::string& s, Rgba& out) {
    if (s.size() != 7 && s.size() != 9) return false;
    if (s[0] != '#') return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    auto byte = [&](std::size_t i) -> int {
        const int hi = nib(s[i]), lo = nib(s[i + 1]);
        return (hi < 0 || lo < 0) ? -1 : hi * 16 + lo;
    };
    const int r = byte(1), g = byte(3), b = byte(5);
    const int a = s.size() == 9 ? byte(7) : 255;
    if (r < 0 || g < 0 || b < 0 || a < 0) return false;
    out = Rgba{static_cast<std::uint8_t>(r),
               static_cast<std::uint8_t>(g),
               static_cast<std::uint8_t>(b),
               static_cast<std::uint8_t>(a)};
    return true;
}

// Row char → palette index; -1 for non-hex.
int RowIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

} // namespace

Result<SpriteAtlas> SpriteAtlas::Load(std::string_view path) {
    Result<JsonValue> doc =
        Gameplay::Json::Load(path, SCHEMA);
    if (!doc.ok()) {
        return Gameplay::Fail<SpriteAtlas>(doc.error, doc.reason);
    }
    return FromJson(doc.value);
}

Result<SpriteAtlas> SpriteAtlas::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Gameplay::Fail<SpriteAtlas>("atlas",
                                           "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != SCHEMA) {
            return Gameplay::Fail<SpriteAtlas>(
                "schema", "expected potato.spriteatlas/1");
        }
    }

    SpriteAtlas atlas;

    if (root.Has("id") && !root["id"].IsString()) {
        return Gameplay::Fail<SpriteAtlas>("atlas",
                                           "'id' must be a string");
    }
    const std::string* id = root.FindString("id");
    if (id == nullptr || id->empty() || id->size() > MAX_ID_LEN) {
        return Gameplay::Fail<SpriteAtlas>(
            "atlas", "missing/empty/out-of-range 'id'");
    }
    atlas.id_ = *id;

    if (!root["tile"].IsInt() || root["tile"].AsInt() < MIN_TILE_PX ||
        root["tile"].AsInt() > MAX_TILE_PX) {
        return Gameplay::Fail<SpriteAtlas>(
            "atlas", "'tile' px out of range");
    }
    atlas.tilePx_ = static_cast<int>(root["tile"].AsInt());

    const JsonValue& pal = root["palette"];
    if (!pal.IsArray() || pal.Items().empty() ||
        pal.Items().size() > MAX_PALETTE) {
        return Gameplay::Fail<SpriteAtlas>(
            "atlas", "'palette' must be 1..16 colors");
    }
    for (const JsonValue& jc : pal.Items()) {
        if (!jc.IsString()) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "palette entries must be '#RRGGBB[AA]'");
        }
        Rgba c;
        if (!ParseColor(jc.AsString(), c)) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "bad palette color '" + jc.AsString() + "'");
        }
        atlas.palette_.push_back(c);
    }

    const JsonValue& tiles = root["tiles"];
    if (!tiles.IsArray() || tiles.Items().empty() ||
        tiles.Items().size() > MAX_TILES) {
        return Gameplay::Fail<SpriteAtlas>(
            "atlas", "'tiles' must be a non-empty array within bound");
    }
    std::set<std::string> seen;
    for (const JsonValue& jt : tiles.Items()) {
        if (!jt.IsObject()) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "tile entry is not an object");
        }
        if (jt.Has("id") && !jt["id"].IsString()) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "tile 'id' must be a string");
        }
        const std::string* tid = jt.FindString("id");
        if (tid == nullptr || tid->empty() ||
            tid->size() > MAX_ID_LEN) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "tile missing/empty/out-of-range 'id'");
        }
        if (seen.count(*tid) != 0) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas", "duplicate tile id '" + *tid + "'");
        }
        const JsonValue& rows = jt["rows"];
        if (!rows.IsArray() ||
            rows.Items().size() !=
                static_cast<std::size_t>(atlas.tilePx_)) {
            return Gameplay::Fail<SpriteAtlas>(
                "atlas",
                "tile '" + *tid + "': 'rows' must be tile strings");
        }
        SpriteTile t;
        t.id = *tid;
        t.indices.reserve(
            static_cast<std::size_t>(atlas.tilePx_) *
            static_cast<std::size_t>(atlas.tilePx_));
        for (const JsonValue& jr : rows.Items()) {
            if (!jr.IsString() ||
                jr.AsString().size() !=
                    static_cast<std::size_t>(atlas.tilePx_)) {
                return Gameplay::Fail<SpriteAtlas>(
                    "atlas", "tile '" + *tid +
                                 "': row length must equal 'tile'");
            }
            for (const char c : jr.AsString()) {
                const int idx = RowIndex(c);
                if (idx < 0 ||
                    static_cast<std::size_t>(idx) >=
                        atlas.palette_.size()) {
                    return Gameplay::Fail<SpriteAtlas>(
                        "atlas", "tile '" + *tid +
                                     "': pixel out of palette range");
                }
                t.indices.push_back(static_cast<std::uint8_t>(idx));
            }
        }
        seen.insert(*tid);
        atlas.tiles_.push_back(std::move(t));
    }

    return Gameplay::Ok(std::move(atlas));
}

const SpriteTile* SpriteAtlas::Find(std::string_view id) const {
    for (const SpriteTile& t : tiles_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

Rgba SpriteAtlas::ColorAt(std::uint8_t index) const {
    return index < palette_.size() ? palette_[index] : Rgba{};
}

} // namespace Potato::Game
