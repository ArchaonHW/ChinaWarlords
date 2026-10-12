#include "Gameplay/Sim/Sim.h"

namespace Potato::Gameplay {

namespace {
std::uint64_t Avalanche(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
} // namespace

std::uint64_t Prng::Next() {
    state_ += 0x9E3779B97F4A7C15ull;
    return Avalanche(state_);
}

Sim::Sim(std::uint64_t seed) : rng_(seed) {}

void Sim::Tick() {
    const std::uint64_t draw = rng_.Next();
    checksum_ = Avalanche(checksum_ + draw + (tickCount_ * 0x9E3779B97F4A7C15ull));
    ++tickCount_;
}

} // namespace Potato::Gameplay
