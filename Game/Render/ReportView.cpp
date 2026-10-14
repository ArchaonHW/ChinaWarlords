#include "Game/Render/ReportView.h"

namespace Potato::Game {

using Gameplay::Result;

namespace {

// The page palette — aged paper on a dark desk, ruled in faded
// ink, sealed in vermillion. Constants, not content: the
// chronicle's dignity doesn't need an atlas entry.
constexpr Rgba DESK{16, 14, 18, 255};
constexpr Rgba PAPER{226, 214, 188, 255};
constexpr Rgba PAPER_SHADE{208, 194, 166, 255};
constexpr Rgba BORDER{92, 74, 52, 255};
constexpr Rgba RULE{140, 120, 92, 120};
constexpr Rgba SEAL_INK{168, 44, 36, 255};

void Rule(Framebuffer& fb, int x0, int y0, int x1, int y1,
          int t, Rgba c) {
    for (int k = 0; k < t; ++k) {
        fb.DrawLine(x0, y0 + k, x1, y1 + k, c);
    }
}

} // namespace

Result<Framebuffer> RenderReportPage(const ReportInput& in) {
    if (in.w <= 0 || in.h <= 0 || in.pageW <= 0 ||
        in.pageH <= 0 || in.margin < 0 || in.lineHeight <= 0) {
        return Gameplay::Fail<Framebuffer>("input",
                                           "bad page dims");
    }
    const int w = in.w > Framebuffer::MAX_DIM
                      ? Framebuffer::MAX_DIM
                      : in.w;
    const int h = in.h > Framebuffer::MAX_DIM
                      ? Framebuffer::MAX_DIM
                      : in.h;
    Framebuffer fb(w, h);
    fb.Clear(DESK);

    const int px0 = in.pageX, py0 = in.pageY;
    const int px1 = px0 + in.pageW - 1, py1 = py0 + in.pageH - 1;

    // The sheet — a drop shadow two px southeast, then paper.
    fb.FillRect(px0 + 2, py0 + 2, in.pageW, in.pageH,
                {0, 0, 0, 110});
    fb.FillRect(px0, py0, in.pageW, in.pageH, PAPER);
    // Footer shade — the page's lower hand-rest darkens slightly.
    fb.FillRect(px0 + 1, py1 - 22, in.pageW - 2, 22,
                PAPER_SHADE);

    // Double border — outer rule, 3 px gap, inner rule.
    Rule(fb, px0, py0, px1, py0, 1, BORDER);
    Rule(fb, px0, py1, px1, py1, 1, BORDER);
    for (int y = py0; y <= py1; ++y) {
        fb.SetPixel(px0, y, BORDER);
        fb.SetPixel(px1, y, BORDER);
    }
    Rule(fb, px0 + 4, py0 + 4, px1 - 4, py0 + 4, 1, BORDER);
    Rule(fb, px0 + 4, py1 - 4, px1 - 4, py1 - 4, 1, BORDER);
    for (int y = py0 + 4; y <= py1 - 4; ++y) {
        fb.SetPixel(px0 + 4, y, BORDER);
        fb.SetPixel(px1 - 4, y, BORDER);
    }

    // Ruled text lines — fixed pitch from the text inset down to
    // the footer; the ImGui layer aligns its wrapped text to
    // the same grid.
    const int tx0 = px0 + in.margin;
    const int tx1 = px1 - in.margin;
    for (int y = py0 + in.margin + in.lineHeight - 2;
         y < py1 - 24; y += in.lineHeight) {
        fb.DrawLine(tx0, y, tx1, y, RULE);
    }

    // The historian's seal — bottom-left (bottom-right is the
    // settle commit's chrome): a 24x24 vermillion stamp whose
    // 6x6 cell pattern comes straight from the battle seed's
    // low bits: each chronicle's seal differs, deterministically.
    if (in.seal != 0) {
        const int ss = 24, cell = 4;
        const int sx0 = px0 + in.margin;
        const int sy0 = py1 - 34 - ss;
        fb.FillRect(sx0, sy0, ss, ss, SEAL_INK);
        for (int cy = 0; cy < 6; ++cy) {
            for (int cx = 0; cx < 6; ++cx) {
                const int bit = cy * 6 + cx;
                if (((in.seal >> bit) & 1ull) == 0) {
                    fb.FillRect(sx0 + cx * cell, sy0 + cy * cell,
                                cell, cell, PAPER);
                }
            }
        }
        // Carved edge — the stamp's border stays inked.
        for (int y = sy0; y < sy0 + ss; ++y) {
            fb.SetPixel(sx0, y, SEAL_INK);
            fb.SetPixel(sx0 + ss - 1, y, SEAL_INK);
        }
        fb.DrawLine(sx0, sy0, sx0 + ss - 1, sy0, SEAL_INK);
        fb.DrawLine(sx0, sy0 + ss - 1, sx0 + ss - 1, sy0 + ss - 1,
                    SEAL_INK);
    }

    return Gameplay::Ok(std::move(fb));
}

} // namespace Potato::Game
