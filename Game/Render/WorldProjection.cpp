#include "Game/Render/WorldProjection.h"

#include "Campaign/World/WorldMap.h"

#include <algorithm>
#include <cstdint>
#include <set>

namespace Potato::Game {

using Campaign::WorldMap;
using Gameplay::Result;

Result<WorldProjection>
WorldProjection::Build(const WorldMap& map, int w, int h,
                       int margin) {
    if (map.NodeCount() == 0) {
        return Gameplay::Fail<WorldProjection>(
            "projection", "world has no nodes");
    }
    if (w <= 0 || h <= 0 || margin < 0 || margin * 2 >= w ||
        margin * 2 >= h) {
        return Gameplay::Fail<WorldProjection>(
            "projection", "view bounds too small");
    }

    WorldProjection p;
    const std::size_t n = map.NodeCount();
    p.nodes_.resize(n);

    bool allHinted = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (!map.NodeAt(i).hasCenter) allHinted = false;
    }
    p.hinted_ = allHinted;

    if (allHinted) {
        // Normalize hint coords into (margin..w-margin,
        // margin..h-margin), aspect-preserved, centered.
        std::int64_t minX = map.NodeAt(0).centerX,
                     maxX = minX,
                     minY = map.NodeAt(0).centerY,
                     maxY = minY;
        for (std::size_t i = 1; i < n; ++i) {
            const auto& nd = map.NodeAt(i);
            minX = std::min(minX, nd.centerX);
            maxX = std::max(maxX, nd.centerX);
            minY = std::min(minY, nd.centerY);
            maxY = std::max(maxY, nd.centerY);
        }
        const std::int64_t spanX = maxX - minX,
                           spanY = maxY - minY;
        const std::int64_t boxW = w - 2 * margin,
                           boxH = h - 2 * margin;
        for (std::size_t i = 0; i < n; ++i) {
            const auto& nd = map.NodeAt(i);
            auto& pn = p.nodes_[i];
            pn.index = i;
            // Degenerate span → center on that axis.
            pn.x = spanX == 0
                ? w / 2
                : margin + static_cast<int>(
                      (nd.centerX - minX) * boxW / spanX);
            pn.y = spanY == 0
                ? h / 2
                : margin + static_cast<int>(
                      (nd.centerY - minY) * boxH / spanY);
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
            auto& pn = p.nodes_[i];
            pn.index = i;
            pn.x = margin + static_cast<int>(
                       boxW * (2 * cx + 1) / (2 * cols));
            pn.y = margin + static_cast<int>(
                       boxH * (2 * cy + 1) / (2 * rows));
        }
    }

    // Dedupe routes into (lower, higher) pairs — the day cost is
    // order-menu data, not edge render data.
    std::set<std::pair<std::size_t, std::size_t>> seen;
    for (std::size_t i = 0; i < n; ++i) {
        for (const auto& [j, days] : map.Neighbors(i)) {
            (void)days;
            if (j == i || j >= n) continue;
            seen.insert(std::minmax(i, j));
        }
    }
    p.edges_.assign(seen.begin(), seen.end());
    return Gameplay::Ok(std::move(p));
}

} // namespace Potato::Game
