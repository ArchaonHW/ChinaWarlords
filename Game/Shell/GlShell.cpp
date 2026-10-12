#include "Game/Shell/GlShell.h"

#include "Game/Render/Framebuffer.h"

// THE GL SEAM — glfw/glad/imgui includes live only in Game/Shell.
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>

namespace Potato::Game {

namespace {

const char* kBlitVS = R"GLSL(
#version 330 core
const vec2 P[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
out vec2 uv;
void main() {
    vec2 p = P[gl_VertexID];
    uv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

const char* kBlitFS = R"GLSL(
#version 330 core
in vec2 uv;
uniform sampler2D tex;
out vec4 frag;
void main() { frag = texture(tex, uv); }
)GLSL";

GLuint CompileShader(GLenum type, const char* src) {
    const GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

} // namespace

bool GlShell::Init(int w, int h, const char* title) {
    if (glfwInit() != GLFW_TRUE) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win =
        glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (win == nullptr) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    if (gladLoadGLLoader(
            reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) ==
        0) {
        glfwDestroyWindow(win);
        glfwTerminate();
        return false;
    }
    window_ = win;

    // Fullscreen blit: RGBA8 texture + NDC triangle.
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);
    const GLuint vs = CompileShader(GL_VERTEX_SHADER, kBlitVS);
    const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kBlitFS);
    if (vs == 0 || fs == 0) {
        Shutdown();
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        Shutdown();
        return false;
    }
    glGenVertexArrays(1, &vao_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    imgui_ = ImGui_ImplGlfw_InitForOpenGL(win, true) &&
             ImGui_ImplOpenGL3_Init("#version 330");
    if (!imgui_) {
        Shutdown();
        return false;
    }
    return true;
}

void GlShell::Shutdown() {
    if (imgui_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imgui_ = false;
    }
    if (window_ != nullptr) {
        glfwMakeContextCurrent(
            static_cast<GLFWwindow*>(window_));
        if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
        if (program_ != 0) glDeleteProgram(program_);
        if (texture_ != 0) glDeleteTextures(1, &texture_);
        vao_ = program_ = texture_ = 0;
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
        window_ = nullptr;
        glfwTerminate();
    }
}

bool GlShell::ShouldClose() const {
    return window_ == nullptr ||
           glfwWindowShouldClose(
               static_cast<GLFWwindow*>(window_)) != 0;
}

void GlShell::NewFrame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GlShell::Present(const Framebuffer& fb) {
    glBindTexture(GL_TEXTURE_2D, texture_);
    if (fb.W() != texW_ || fb.H() != texH_) {
        texW_ = fb.W();
        texH_ = fb.H();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW_, texH_, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, fb.Bytes().data());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW_, texH_,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        fb.Bytes().data());
    }
    int vw = 0, vh = 0;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window_),
                           &vw, &vh);
    glViewport(0, 0, vw, vh);
    glClearColor(0.07f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GlShell::Swap() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(static_cast<GLFWwindow*>(window_));
}

} // namespace Potato::Game
