#include "Gameplay/Sim/SimulationLod.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using namespace Potato::Gameplay;
using Clock = std::chrono::steady_clock;
constexpr std::size_t ITERATIONS = 7;

SimulationLodFactors Uniform(std::uint16_t value) {
    return {value, value, value, value, value, value};
}

std::uint64_t Checksum(const std::vector<SimulationLodAssignment>& plan) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& assignment : plan) {
        hash ^= assignment.agentId;
        hash *= 1099511628211ull;
        hash ^= static_cast<std::uint8_t>(assignment.lod);
        hash *= 1099511628211ull;
    }
    return hash;
}

bool Run(std::size_t count) {
    std::vector<SimulationLodSubject> subjects;
    subjects.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto factors = i < 100 ? Uniform(1000) : Uniform(250);
        subjects.push_back({static_cast<std::uint64_t>(i + 1), factors});
    }

    SimulationLodPolicy policy;
    std::array<std::int64_t, ITERATIONS> elapsed{};
    std::uint64_t expectedChecksum = 0;
    for (std::size_t iteration = 0; iteration < ITERATIONS; ++iteration) {
        const auto start = Clock::now();
        auto result = policy.Plan(subjects, SimulationBudget{});
        const auto stop = Clock::now();
        if (!result.ok() || result.value.size() != count) {
            std::fprintf(stderr, "LOD plan failed for %zu subjects: %s\n",
                         count, result.reason.c_str());
            return false;
        }
        const std::uint64_t checksum = Checksum(result.value);
        if (iteration == 0) expectedChecksum = checksum;
        if (checksum != expectedChecksum) {
            std::fprintf(stderr, "LOD plan changed between runs at %zu subjects\n",
                         count);
            return false;
        }
        elapsed[iteration] = std::chrono::duration_cast<
            std::chrono::microseconds>(stop - start).count();
    }

    std::sort(elapsed.begin(), elapsed.end());
    std::printf("%zu,%zu,%lld,%zu,%llu\n", count, ITERATIONS,
                static_cast<long long>(elapsed[ITERATIONS / 2]), count,
                static_cast<unsigned long long>(expectedChecksum));
    return true;
}

} // namespace

int main() {
    std::puts("subjects,iterations,median_us,assignments,checksum");
    for (const std::size_t count : {100u, 1000u, 10000u, 100000u}) {
        if (!Run(count)) return 1;
    }
    return 0;
}
