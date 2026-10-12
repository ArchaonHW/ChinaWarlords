#pragma once

#include "Game/Render/SpriteAtlas.h"

#include <cstdint>
#include <vector>

namespace Potato::Game {

// Framebuffer — RGBA8 software render target (Story 8.1). All
// ops are integer-only and clipped; the buffer a GL presenter
// would blit is exactly this byte layout, so the headless
// raster IS the shipped raster — the window is a seam, not a
// second renderer.
//
// `shade` multiplies RGB by shade/255 before blending —
// certainty dims belief, it never deletes it.

class Framebuffer {
public:
    static constexpr int MAX_DIM = 4096;

    Framebuffer() = default; // empty buffer — Result<T> needs it
    // Clamped to MAX_DIM² — an oversized request still yields a
    // valid (clipped) buffer.
    Framebuffer(int w, int h);

    int W() const { return w_; }
    int H() const { return h_; }
    const std::vector<std::uint8_t>& Bytes() const { return px_; }
    // 4 bytes per pixel, row-major, RGBA.
    static constexpr int BytesPerPixel = 4;

    Rgba PixelAt(int x, int y) const;

    void Clear(Rgba c);
    void SetPixel(int x, int y, Rgba c);                 // clipped
    void BlendPixel(int x, int y, Rgba c);               // src-over
    void FillRect(int x, int y, int w, int h, Rgba c);   // src-over
    // Bresenham — route edges. src-over.
    void DrawLine(int x0, int y0, int x1, int y1, Rgba c);
    // Tile blit: every pixel src-over blended after shade
    // multiply. `shade` 255 = identity.
    void DrawTile(const SpriteAtlas& atlas,
                  const SpriteTile& tile, int px, int py,
                  int shade);

    // FNV-1a 64 over the byte buffer — the determinism pin a
    // golden-hash test latches onto.
    std::uint64_t Hash() const;

private:
    int w_ = 0, h_ = 0;
    std::vector<std::uint8_t> px_;
};

} // namespace Potato::Game
