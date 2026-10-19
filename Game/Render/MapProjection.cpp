#include "Game/Render/MapProjection.h"

#include "Gameplay/Map/BattleMap.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <unordered_map>

namespace Potato::Game {

namespace {

constexpr std::int64_t HIT_CELL_SIZE = 32;
constexpr std::size_t HIT_GRID_MIN_REGIONS = 32;

std::int64_t CellOf(std::int64_t coordinate) {
    const std::int64_t quotient = coordinate / HIT_CELL_SIZE;
    const std::int64_t remainder = coordinate % HIT_CELL_SIZE;
    return quotient - (coordinate < 0 && remainder != 0 ? 1 : 0);
}

std::uint64_t CellKey(std::int64_t x, std::int64_t y) {
    const auto ux = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(x));
    const auto uy = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(y));
    return (static_cast<std::uint64_t>(ux) << 32) | uy;
}

std::uint64_t OrderedDistance(std::int64_t high, std::int64_t low) {
    return static_cast<std::uint64_t>(high) -
           static_cast<std::uint64_t>(low);
}

// Computes floor(numerator * multiplier / denominator) without a wider
// integer type. The projection inputs guarantee numerator <= denominator
// and multiplier <= INT_MAX, so the result is bounded by multiplier.
std::uint64_t ScaleFraction(std::uint64_t numerator,
                            std::uint64_t multiplier,
                            std::uint64_t denominator) {
    if (numerator == denominator) return multiplier;

    std::uint64_t quotient = 0;
    std::uint64_t remainder = 0;
    for (int bit = 63; bit >= 0; --bit) {
        quotient *= 2;

        std::uint64_t carry = 0;
        const std::uint64_t remainderComplement = denominator - remainder;
        if (remainder >= remainderComplement) {
            remainder -= remainderComplement;
            ++carry;
        } else {
            remainder += remainder;
        }

        if (((multiplier >> bit) & 1u) != 0) {
            const std::uint64_t numeratorComplement = denominator - numerator;
            if (remainder >= numeratorComplement) {
                remainder -= numeratorComplement;
                ++carry;
            } else {
                remainder += numerator;
            }
        }
        quotient += carry;
    }
    return quotient;
}

std::uint64_t ScaleRatio(std::uint64_t numerator,
                         std::uint64_t multiplier,
                         std::uint64_t denominator) {
    // Splitting the whole part keeps ScaleFraction's numerator bounded by
    // the denominator even when the coordinate span exceeds it.
    const std::uint64_t whole = numerator / denominator;
    const std::uint64_t remainder = numerator % denominator;
    return whole * multiplier +
           ScaleFraction(remainder, multiplier, denominator);
}

struct WideProduct {
    std::uint64_t high;
    std::uint64_t low;
};

// Preserve the full product of an int64 coordinate span and an int-sized
// viewport extent without relying on compiler-specific 128-bit integers.
WideProduct MultiplyByViewExtent(std::uint64_t value,
                                 std::uint32_t extent) {
    const std::uint64_t lowProduct =
        (value & 0xffffffffu) * extent;
    const std::uint64_t highProduct = (value >> 32) * extent;
    const std::uint64_t shiftedHigh = highProduct << 32;
    const std::uint64_t low = lowProduct + shiftedHigh;
    return { (highProduct >> 32) + (low < lowProduct ? 1u : 0u), low };
}

int CompareAspectProducts(std::uint32_t width, std::uint64_t spanY,
                          std::uint32_t height, std::uint64_t spanX) {
    const WideProduct widthTimesY = MultiplyByViewExtent(spanY, width);
    const WideProduct heightTimesX = MultiplyByViewExtent(spanX, height);
    if (widthTimesY.high != heightTimesX.high) {
        return widthTimesY.high < heightTimesX.high ? -1 : 1;
    }
    if (widthTimesY.low == heightTimesX.low) return 0;
    return widthTimesY.low < heightTimesX.low ? -1 : 1;
}

} // namespace

using Gameplay::BattleMap;
using Gameplay::Result;

Result<MapProjection>
MapProjection::Build(const BattleMap& map, int w, int h,
                     int margin) {
    const std::int64_t twiceMargin =
        static_cast<std::int64_t>(margin) * 2;
    if (map.RegionCount() == 0) {
        return Gameplay::Fail<MapProjection>(
            "projection", "map has no regions");
    }
    if (w <= 0 || h <= 0 || margin < 0 || twiceMargin >= w ||
        twiceMargin >= h) {
        return Gameplay::Fail<MapProjection>(
            "projection", "view bounds too small");
    }

    MapProjection p;
    const std::size_t n = map.RegionCount();
    p.regions_.resize(n);

    bool allHinted = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (!map.RegionAt(i).hasCenter) allHinted = false;
    }
    p.hinted_ = allHinted;

    if (allHinted) {
        // Normalize hint coords into (margin..w-margin,
        // margin..h-margin), aspect-preserved, centered.
        std::int64_t minX = map.RegionAt(0).centerX,
                     maxX = minX,
                     minY = map.RegionAt(0).centerY,
                     maxY = minY;
        for (std::size_t i = 1; i < n; ++i) {
            const auto& r = map.RegionAt(i);
            minX = std::min(minX, r.centerX);
            maxX = std::max(maxX, r.centerX);
            minY = std::min(minY, r.centerY);
            maxY = std::max(maxY, r.centerY);
        }
        const std::uint64_t spanX = OrderedDistance(maxX, minX),
                            spanY = OrderedDistance(maxY, minY);
        const std::int64_t boxW = w - 2 * margin,
                           boxH = h - 2 * margin;
        std::uint64_t renderW = spanX == 0 ? 0 : boxW;
        std::uint64_t renderH = spanY == 0 ? 0 : boxH;
        if (spanX != 0 && spanY != 0) {
            if (CompareAspectProducts(
                    static_cast<std::uint32_t>(boxW), spanY,
                    static_cast<std::uint32_t>(boxH), spanX) <= 0) {
                renderH = ScaleRatio(
                    spanY, static_cast<std::uint64_t>(boxW), spanX);
            } else {
                renderW = ScaleRatio(
                    spanX, static_cast<std::uint64_t>(boxH), spanY);
            }
        }
        const std::int64_t originX =
            margin + (boxW - static_cast<std::int64_t>(renderW)) / 2;
        const std::int64_t originY =
            margin + (boxH - static_cast<std::int64_t>(renderH)) / 2;
        for (std::size_t i = 0; i < n; ++i) {
            const auto& r = map.RegionAt(i);
            auto& pr = p.regions_[i];
            pr.index = i;
            // Degenerate span → center on that axis.
            pr.x = spanX == 0
                ? w / 2
                : static_cast<int>(originX + ScaleRatio(
                      OrderedDistance(r.centerX, minX), renderW, spanX));
            pr.y = spanY == 0
                ? h / 2
                : static_cast<int>(originY + ScaleRatio(
                      OrderedDistance(r.centerY, minY), renderH, spanY));
        }
    } else {
        // Index-ordered grid: cols = ceil(sqrt(n)).
        std::size_t cols = 1;
        while (cols * cols < n) ++cols;
        const std::size_t rows = (n + cols - 1) / cols;
        const std::int64_t boxW = w - 2 * margin,
                           boxH = h - 2 * margin;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t cx = i % cols, cy = i / cols;
            auto& pr = p.regions_[i];
            pr.index = i;
            pr.x = margin + static_cast<int>(
                       boxW * (2 * cx + 1) / (2 * cols));
            pr.y = margin + static_cast<int>(
                       boxH * (2 * cy + 1) / (2 * rows));
        }
    }

    // Tiny battle maps are cheaper to scan than to index. Projection order is
    // the canonical tie-break for buckets once the grid is worthwhile.
    if (p.regions_.size() >= HIT_GRID_MIN_REGIONS) {
        for (const ProjectedRegion& region : p.regions_) {
            p.hitCells_[CellKey(CellOf(region.x), CellOf(region.y))]
                .push_back(region.index);
        }
    }

    // Dedupe adjacency into (lower, higher) pairs.
    std::set<std::pair<std::size_t, std::size_t>> seen;
    for (std::size_t i = 0; i < n; ++i) {
        for (const std::size_t j : map.Neighbors(i)) {
            if (j == i || j >= n) continue;
            seen.insert(std::minmax(i, j));
        }
    }
    p.edges_.assign(seen.begin(), seen.end());
    return Gameplay::Ok(std::move(p));
}

int MapProjection::Hit(int x, int y, int radius) const {
    if (radius < 0 || regions_.empty()) return -1;
    const std::int64_t r = radius;
    const std::int64_t rr = r * r;
    int best = -1;
    std::int64_t bestD = rr + 1;

    const auto consider = [&](std::size_t index) {
        const ProjectedRegion& region = regions_[index];
        const std::int64_t dx = static_cast<std::int64_t>(x) - region.x;
        const std::int64_t dy = static_cast<std::int64_t>(y) - region.y;
        if (dx < -r || dx > r || dy < -r || dy > r) return;
        const std::int64_t d = dx * dx + dy * dy;
        if (d <= rr &&
            (d < bestD ||
             (d == bestD &&
              (best < 0 || index < static_cast<std::size_t>(best))))) {
            bestD = d;
            best = static_cast<int>(index);
        }
    };

    const std::int64_t x0 = CellOf(static_cast<std::int64_t>(x) - r);
    const std::int64_t x1 = CellOf(static_cast<std::int64_t>(x) + r);
    const std::int64_t y0 = CellOf(static_cast<std::int64_t>(y) - r);
    const std::int64_t y1 = CellOf(static_cast<std::int64_t>(y) + r);
    const std::uint64_t columns = static_cast<std::uint64_t>(x1 - x0 + 1);
    const std::uint64_t rows = static_cast<std::uint64_t>(y1 - y0 + 1);
    const bool areaOverflow =
        rows > std::numeric_limits<std::uint64_t>::max() / columns;
    const std::uint64_t cellCount = areaOverflow ? 0 : columns * rows;
    // A very broad query can cover more empty cells than there are regions.
    // Fall back to the reference scan instead of walking a huge screen area.
    const std::uint64_t scanLimit =
        static_cast<std::uint64_t>(hitCells_.size()) * 4 + 16;
    if (hitCells_.empty() || areaOverflow || cellCount > scanLimit) {
        for (std::size_t i = 0; i < regions_.size(); ++i) consider(i);
        return best;
    }

    for (std::int64_t cy = y0; cy <= y1; ++cy) {
        for (std::int64_t cx = x0; cx <= x1; ++cx) {
            const auto cell = hitCells_.find(CellKey(cx, cy));
            if (cell == hitCells_.end()) continue;
            for (const std::size_t index : cell->second) consider(index);
        }
    }
    return best;
}

} // namespace Potato::Game
