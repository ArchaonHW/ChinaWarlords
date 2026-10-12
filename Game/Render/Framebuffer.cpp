#include "Game/Render/Framebuffer.h"

#include <algorithm>
#include <cstdlib>

namespace Potato::Game {

namespace {

// FNV-1a 64 — same constants the ledger chain pins.
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

} // namespace

Framebuffer::Framebuffer(int w, int h)
    : w_(w < 0 ? 0 : (w > MAX_DIM ? MAX_DIM : w)),
      h_(h < 0 ? 0 : (h > MAX_DIM ? MAX_DIM : h)),
      px_(static_cast<std::size_t>(w_) *
              static_cast<std::size_t>(h_) * BytesPerPixel,
          0) {}

Rgba Framebuffer::PixelAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return {};
    const std::size_t i =
        (static_cast<std::size_t>(y) *
             static_cast<std::size_t>(w_) +
         static_cast<std::size_t>(x)) * BytesPerPixel;
    return {px_[i], px_[i + 1], px_[i + 2], px_[i + 3]};
}

void Framebuffer::Clear(Rgba c) {
    for (std::size_t i = 0; i + 3 < px_.size(); i += 4) {
        px_[i] = c.r;
        px_[i + 1] = c.g;
        px_[i + 2] = c.b;
        px_[i + 3] = c.a;
    }
}

void Framebuffer::SetPixel(int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    const std::size_t i =
        (static_cast<std::size_t>(y) *
             static_cast<std::size_t>(w_) +
         static_cast<std::size_t>(x)) * BytesPerPixel;
    px_[i] = c.r;
    px_[i + 1] = c.g;
    px_[i + 2] = c.b;
    px_[i + 3] = c.a;
}

void Framebuffer::BlendPixel(int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_ || c.a == 0) return;
    const std::size_t i =
        (static_cast<std::size_t>(y) *
             static_cast<std::size_t>(w_) +
         static_cast<std::size_t>(x)) * BytesPerPixel;
    if (c.a == 255) {
        px_[i] = c.r;
        px_[i + 1] = c.g;
        px_[i + 2] = c.b;
        px_[i + 3] = c.a;
        return;
    }
    const int a = c.a, ia = 255 - a;
    px_[i] = static_cast<std::uint8_t>(
        (c.r * a + px_[i] * ia) / 255);
    px_[i + 1] = static_cast<std::uint8_t>(
        (c.g * a + px_[i + 1] * ia) / 255);
    px_[i + 2] = static_cast<std::uint8_t>(
        (c.b * a + px_[i + 2] * ia) / 255);
    px_[i + 3] = static_cast<std::uint8_t>(
        std::min(255, a + px_[i + 3] * ia / 255));
}

void Framebuffer::FillRect(int x, int y, int w, int h, Rgba c) {
    const int x1 = std::max(0, x), y1 = std::max(0, y);
    const int x2 = std::min(w_, x + w), y2 = std::min(h_, y + h);
    for (int yy = y1; yy < y2; ++yy) {
        for (int xx = x1; xx < x2; ++xx) {
            BlendPixel(xx, yy, c);
        }
    }
}

void Framebuffer::DrawLine(int x0, int y0, int x1, int y1,
                           Rgba c) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        BlendPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Framebuffer::DrawTile(const SpriteAtlas& atlas,
                           const SpriteTile& tile, int px, int py,
                           int shade) {
    const int s = shade < 0 ? 0 : (shade > 255 ? 255 : shade);
    const int tpx = atlas.TilePx();
    for (int ty = 0; ty < tpx; ++ty) {
        for (int tx = 0; tx < tpx; ++tx) {
            const std::uint8_t idx = tile.indices
                [static_cast<std::size_t>(ty) *
                     static_cast<std::size_t>(tpx) +
                 static_cast<std::size_t>(tx)];
            Rgba c = atlas.ColorAt(idx);
            if (s != 255) {
                c.r = static_cast<std::uint8_t>(c.r * s / 255);
                c.g = static_cast<std::uint8_t>(c.g * s / 255);
                c.b = static_cast<std::uint8_t>(c.b * s / 255);
            }
            BlendPixel(px + tx, py + ty, c);
        }
    }
}

std::uint64_t Framebuffer::Hash() const {
    std::uint64_t h = kFnvOffset;
    for (const std::uint8_t b : px_) {
        h ^= b;
        h *= kFnvPrime;
    }
    return h;
}

} // namespace Potato::Game
