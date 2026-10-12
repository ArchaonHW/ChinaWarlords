#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay { class JsonValue; }

namespace Potato::Game {

// SpriteAtlas — indexed-color pixel tiles for the game's
// presentation layer (Story 8.1). `potato.spriteatlas/1` carries
// a palette and a set of square tiles authored as string rows:
// one hex char per pixel (0-9a-f) indexing the palette — the
// classic 16-color constraint keeps placeholder art honest.
//
// Transparency is a palette entry with A=0 (index 0 by
// convention, not enforced). The atlas is pure data — loading
// happens at the content boundary; the render path never does
// file I/O.
//
// Wire shape:
//   {"schema":"potato.spriteatlas/1","id":"core","tile":8,
//    "palette":["#00000000","#2b2b2bff", ...],
//    "tiles":[{"id":"terrain.open",
//              "rows":["0000","0110","0110","0000"]}]}

struct Rgba {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;
    friend bool operator==(const Rgba&, const Rgba&) = default;
};

struct SpriteTile {
    std::string id;
    // tile × tile palette indices, row-major.
    std::vector<std::uint8_t> indices;
};

class SpriteAtlas {
public:
    static constexpr std::string_view SCHEMA =
        "potato.spriteatlas/1";
    static constexpr int MIN_TILE_PX = 4;
    static constexpr int MAX_TILE_PX = 64;
    static constexpr std::size_t MAX_TILES = 4096;
    static constexpr std::size_t MAX_PALETTE = 16;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_NAME_LEN = 64;

    // potato.spriteatlas/1 file → gated load → validation.
    static Gameplay::Result<SpriteAtlas> Load(std::string_view path);
    // Validate a DOM root (schema must already be gated for
    // file loads). Pure: failure produces no partial atlas.
    static Gameplay::Result<SpriteAtlas> FromJson(
        const Gameplay::JsonValue& root);

    const std::string& Id() const { return id_; }
    int TilePx() const { return tilePx_; }
    const std::vector<Rgba>& Palette() const { return palette_; }
    std::size_t TileCount() const { return tiles_.size(); }
    // nullptr on miss — callers fall back to solid rects so an
    // incomplete placeholder atlas never kills the frame.
    const SpriteTile* Find(std::string_view id) const;
    Rgba ColorAt(std::uint8_t index) const;

private:
    std::string id_;
    int tilePx_ = 0;
    std::vector<Rgba> palette_;
    std::vector<SpriteTile> tiles_;
};

} // namespace Potato::Game
