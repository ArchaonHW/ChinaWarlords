#include "Game/Render/SpriteAnimation.h"

#include <limits>

namespace Potato::Game {

bool SpriteAnimation::Configure(
    const SpriteAtlas& atlas, std::span<const SpriteAnimationFrame> frames,
    Playback playback) {
    if (frames.empty() || frames.size() > SpriteAtlas::MAX_TILES) return false;

    std::uint64_t cycleTicks = 0;
    for (const SpriteAnimationFrame& frame : frames) {
        if (frame.tileId.empty() || frame.tileId.size() > SpriteAtlas::MAX_ID_LEN ||
            !atlas.Find(frame.tileId) || frame.durationTicks == 0 ||
            cycleTicks > std::numeric_limits<std::uint64_t>::max() -
                             frame.durationTicks) {
            return false;
        }
        cycleTicks += frame.durationTicks;
    }

    atlasId_ = atlas.Id();
    frames_.assign(frames.begin(), frames.end());
    playback_ = playback;
    cycleTicks_ = cycleTicks;
    Reset();
    return true;
}

void SpriteAnimation::Reset() {
    frameIndex_ = 0;
    elapsedTicks_ = 0;
    finished_ = false;
}

void SpriteAnimation::Advance(std::uint64_t ticks) {
    if (frames_.empty() || finished_ || ticks == 0) return;

    const std::uint64_t remaining =
        frames_[frameIndex_].durationTicks - elapsedTicks_;
    if (ticks < remaining) {
        elapsedTicks_ += static_cast<std::uint32_t>(ticks);
        return;
    }
    ticks -= remaining;

    if (playback_ == Playback::Once && frameIndex_ + 1 == frames_.size()) {
        elapsedTicks_ = frames_[frameIndex_].durationTicks;
        finished_ = true;
        return;
    }
    frameIndex_ = (frameIndex_ + 1) % frames_.size();
    elapsedTicks_ = 0;
    if (playback_ == Playback::Loop) ticks %= cycleTicks_;

    while (ticks >= frames_[frameIndex_].durationTicks) {
        ticks -= frames_[frameIndex_].durationTicks;
        if (playback_ == Playback::Once &&
            frameIndex_ + 1 == frames_.size()) {
            elapsedTicks_ = frames_[frameIndex_].durationTicks;
            finished_ = true;
            return;
        }
        frameIndex_ = (frameIndex_ + 1) % frames_.size();
    }
    elapsedTicks_ = static_cast<std::uint32_t>(ticks);
}

const SpriteTile* SpriteAnimation::CurrentTile(const SpriteAtlas& atlas) const {
    if (frames_.empty() || atlas.Id() != atlasId_) return nullptr;
    return atlas.Find(frames_[frameIndex_].tileId);
}

} // namespace Potato::Game
