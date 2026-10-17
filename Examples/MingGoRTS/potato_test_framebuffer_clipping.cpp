#include "Game/Render/Framebuffer.h"
#include "Game/Render/SpriteAtlas.h"
#include "Gameplay/Json/JsonValue.h"

#include <cstdio>
#include <limits>
#include <string>
#include <utility>

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

Potato::Game::SpriteAtlas TestAtlas() {
    const std::string document =
        "{\"schema\":\"potato.spriteatlas/1\",\"id\":\"clip-test\","
        "\"tile\":4,\"palette\":[\"#00000000\",\"#102030ff\"],"
        "\"tiles\":[{\"id\":\"unit\",\"rows\":[\"0101\",\"1010\","
        "\"1100\",\"0011\"]}]}";
    auto result = Potato::Game::SpriteAtlas::FromJson(
        Potato::Gameplay::JsonValue::Parse(document).value);
    return std::move(result.value);
}

} // namespace

int main() {
    using Potato::Game::Framebuffer;

    const auto atlas = TestAtlas();
    const auto* unit = atlas.Find("unit");
    bool clippedMatchesSource = unit != nullptr;
    Framebuffer clippedTile(2, 2);
    if (unit != nullptr) {
        clippedTile.DrawTile(atlas, *unit, -2, -2, 255);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const auto sourceIndex = unit->indices[
                    static_cast<std::size_t>(y + 2) *
                        static_cast<std::size_t>(atlas.TilePx()) +
                    static_cast<std::size_t>(x + 2)];
                const auto actual = clippedTile.PixelAt(x, y);
                const auto expected = atlas.ColorAt(sourceIndex);
                if (actual != expected) {
                    std::printf("pixel (%d,%d): got %u/%u/%u/%u, expected %u/%u/%u/%u\n",
                                x, y, actual.r, actual.g, actual.b, actual.a,
                                expected.r, expected.g, expected.b, expected.a);
                    clippedMatchesSource = false;
                }
            }
        }
    }
    Check(clippedMatchesSource, "partially clips atlas tile blit");

    const auto clippedHash = clippedTile.Hash();
    if (unit != nullptr) {
        clippedTile.DrawTile(atlas, *unit, clippedTile.W(), 0, 255);
        clippedTile.DrawTile(atlas, *unit, 0, clippedTile.H(), 255);
        clippedTile.DrawTile(atlas, *unit,
                             std::numeric_limits<int>::min(),
                             std::numeric_limits<int>::min(), 255);
    }
    Check(clippedTile.Hash() == clippedHash,
          "fully offscreen and extreme tile blits are no-ops");
    return failures == 0 ? 0 : 1;
}
