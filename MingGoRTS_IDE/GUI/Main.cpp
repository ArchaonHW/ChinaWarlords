/**
 * MingGoRTS IDE GUI Main Application
 * ImGui-based GUI for AI-powered game development
 */

#include "IDEGUI.h"
#include "../IDECore.h"
#include "../AIIntegration.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

static void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    std::cout << "=== MingGoRTS IDE GUI ===" << std::endl;
    std::cout << "AI-Powered Game Development Environment" << std::endl;
    std::cout << std::endl;
    
    // Setup GLFW
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "MingGoRTS IDE", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Not available in all ImGui versions
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Initialize IDE Core
    MingGoRTSIDE::IDEConfig config;
    config.workspacePath = "C:\\HWC\\MingGoRTS";
    config.projectName = "MingGoRTS Game Project";
    config.enableAIAssistance = true;
    config.enableMultiAgent = true;
    
    MingGoRTSIDE::IDECore ideCore;
    if (!ideCore.Initialize(config)) {
        std::cerr << "Failed to initialize IDE Core" << std::endl;
        return 1;
    }
    
    // Initialize IDE GUI
    MingGoRTSIDE::IDEGUI ideGUI;
    if (!ideGUI.Initialize(&ideCore)) {
        std::cerr << "Failed to initialize IDE GUI" << std::endl;
        return 1;
    }
    
    ideGUI.Run();
    
    std::cout << "MingGoRTS IDE GUI started successfully" << std::endl;
    
    // Main loop
    while (!glfwWindowShouldClose(window) && ideGUI.GetState().running) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render IDE GUI
        ideGUI.RenderMainMenu();
        ideGUI.RenderToolbar();
        ideGUI.RenderFileExplorer();
        ideGUI.RenderCodeEditor();
        ideGUI.RenderAIAgentPanel();
        ideGUI.RenderTerminal();
        ideGUI.RenderOutput();
        ideGUI.RenderProperties();
        ideGUI.RenderDebugger();
        ideGUI.RenderGitPanel();
        ideGUI.RenderCodeAnalysis();
        ideGUI.RenderIntelligentSuggestions();
        ideGUI.RenderDevelopmentAssistant();
        ideGUI.RenderSettings();
        ideGUI.RenderHelp();
        ideGUI.RenderStatusBar();
        
        // Handle keyboard shortcuts
        ideGUI.HandleKeyboardShortcuts();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ideGUI.Shutdown();
    ideCore.Shutdown();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "MingGoRTS IDE GUI closed" << std::endl;
    
    return 0;
}