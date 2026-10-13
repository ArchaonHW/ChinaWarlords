#pragma once

#include "Game/Render/Framebuffer.h"
#include "Gameplay/Result.h"

#include <cstdint>

namespace Potato::Game {

// ReportView — the S3c aftermath page chrome (design S3c +
// §4 文字層路線 first stage): the framebuffer draws the PAGE —
// paper field, double border, ruled text lines, the
// historian's seal — while the CJK glyphs stay on the ImGui
// text layer over it (the bitmap-font glyph pipeline is the
// deferred second stage). Every mark is an integer
// computation of the inputs; Framebuffer::Hash pins it —
// the same chronicle reads out of the same page.

struct ReportInput {
    int w = 960, h = 600; // output dims, clamped
    // The sheet inside the frame — the ImGui text layer is
    // expected to align to pageX+margin/pageY+margin so the
    // ruled lines sit under the glyphs.
    int pageX = 150, pageY = 30, pageW = 660, pageH = 540;
    int margin = 30;        // text inset inside the sheet
    int lineHeight = 24;    // ruled-line pitch
    std::uint64_t seal = 0; // battle seed → stamp pattern
};

Gameplay::Result<Framebuffer>
RenderReportPage(const ReportInput& in);

} // namespace Potato::Game
