#include "Game/Render/SpriteAnimation.h"
#include "Game/Render/SpriteAtlas.h"
#include "Gameplay/Json/JsonValue.h"

#include <array>
#include <cstdio>
#include <span>
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
        "{\"schema\":\"potato.spriteatlas/1\",\"id\":\"animation-test\","
        "\"tile\":4,\"palette\":[\"#00000000\",\"#102030ff\"],"
        "\"tiles\":["
        "{\"id\":\"terrain.open\",\"rows\":[\"1111\",\"1111\",\"1111\",\"1111\"]},"
        "{\"id\":\"terrain.water\",\"rows\":[\"1111\",\"1111\",\"1111\",\"1111\"]},"
        "{\"id\":\"mark.village\",\"rows\":[\"1111\",\"1111\",\"1111\",\"1111\"]}]}";
    auto result = Potato::Game::SpriteAtlas::FromJson(
        Potato::Gameplay::JsonValue::Parse(document).value);
    return std::move(result.value);
}

} // namespace

int main() {
    using Potato::Game::SpriteAnimation;
    using Potato::Game::SpriteAnimationFrame;

    const auto atlas = TestAtlas();
    const std::array<SpriteAnimationFrame, 3> frames{{
        {"terrain.open", 2},
        {"terrain.water", 1},
        {"mark.village", 3},
    }};
    SpriteAnimation animation;
    Check(animation.Configure(atlas, frames), "configure atlas clip");
    Check(animation.CurrentTile(atlas) == atlas.Find("terrain.open"),
          "starts at first tile");
    animation.Advance(2);
    Check(animation.CurrentTile(atlas) == atlas.Find("terrain.water"),
          "honors per-frame tick duration");
    animation.Advance(4);
    Check(animation.FrameIndex() == 0, "loop wraps clip");
    animation.Advance(6000000000ULL);
    Check(animation.FrameIndex() == 0, "skips full cycles for large ticks");

    Check(animation.Configure(atlas, frames, SpriteAnimation::Playback::Once),
          "configure one-shot clip");
    animation.Advance(100);
    Check(animation.IsFinished() && animation.FrameIndex() == 2,
          "one-shot holds final tile");
    animation.Reset();
    Check(!animation.IsFinished() && animation.FrameIndex() == 0,
          "reset starts over");

    SpriteAnimationFrame missing{"not.in.atlas", 1};
    Check(!animation.Configure(atlas, std::span(&missing, 1)),
          "rejects missing atlas frames");
    Check(animation.CurrentTile(atlas) == atlas.Find("terrain.open"),
          "invalid clip leaves prior clip intact");
    return failures == 0 ? 0 : 1;
}
