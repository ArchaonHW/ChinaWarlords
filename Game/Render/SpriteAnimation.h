#pragma once

#include "Game/Render/SpriteAtlas.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Potato::Game {

struct SpriteAnimationFrame {
    std::string tileId;
    std::uint32_t durationTicks = 1;
};

// Game presentation clip for indexed-color atlas tiles. It advances only
// from explicit integer ticks and never changes Gameplay simulation state.
class SpriteAnimation final {
public:
    enum class Playback { Loop, Once };

    bool Configure(const SpriteAtlas& atlas,
                   std::span<const SpriteAnimationFrame> frames,
                   Playback playback = Playback::Loop);
    void Reset();
    void Advance(std::uint64_t ticks = 1);

    bool IsValid() const { return !frames_.empty(); }
    bool IsFinished() const { return finished_; }
    std::size_t FrameIndex() const { return frameIndex_; }
    const SpriteTile* CurrentTile(const SpriteAtlas& atlas) const;

private:
    std::string atlasId_;
    std::vector<SpriteAnimationFrame> frames_;
    Playback playback_ = Playback::Loop;
    std::size_t frameIndex_ = 0;
    std::uint32_t elapsedTicks_ = 0;
    std::uint64_t cycleTicks_ = 0;
    bool finished_ = false;
};

} // namespace Potato::Game
