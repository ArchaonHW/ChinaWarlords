#pragma once

namespace Potato::Game {

class Framebuffer;

// GlShell — the windowed seam (Story 12.9). The ONLY place the
// game layer touches glfw/glad/imgui: Game/Render renders the
// software Framebuffer headlessly; this presents it.
//
// Present uploads the RGBA8 buffer to a texture and blits it
// fullscreen, nearest-filtered — the shipped raster is the
// headless raster; the window adds nothing but glass.
//
// All GL/glfw/imgui types stay behind void*/bool here so this
// header is includable from TUs that never see GL headers —
// the concrete headers are GlShell.cpp-only.
//
// Only compiled into PotatoGame when POTATO_BUILD_GUI=ON.

class GlShell {
public:
    // Window + GL 3.3 core context + glad + ImGui. w/h are the
    // framebuffer's logical size; the window blits it scaled.
    bool Init(int w, int h, const char* title);
    // Tears down imgui, GL objects, the window, glfw.
    void Shutdown();

    bool ShouldClose() const;
    // Poll events + begin the ImGui frame.
    void NewFrame();
    // Upload + blit the frame's RGBA8 bytes, nearest-filtered.
    void Present(const Framebuffer& fb);
    // Flush ImGui draw data + swap buffers.
    void Swap();

private:
    void* window_ = nullptr;      // GLFWwindow*
    bool imgui_ = false;          // backends initialized
    unsigned texture_ = 0;        // GL texture (fb upload)
    unsigned program_ = 0;        // blit shader
    unsigned vao_ = 0;            // empty VAO (core profile)
    int texW_ = 0, texH_ = 0;     // allocated texture extent
};

} // namespace Potato::Game
