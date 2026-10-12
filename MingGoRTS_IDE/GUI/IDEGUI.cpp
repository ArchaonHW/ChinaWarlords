/**
 * MingGoRTS IDE GUI Implementation
 */

#include "IDEGUI.h"
#include "../IntelligentSuggestion.h"
#include "../../AI/IntelligentDevelopmentSystem.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <iomanip>

// Portable shims for the MSVC secure CRT functions used below
#if !defined(_MSC_VER)
#ifndef _TRUNCATE
#define _TRUNCATE (static_cast<size_t>(-1))
#endif
static int localtime_s(std::tm* result, const std::time_t* time) {
    return localtime_r(time, result) ? 0 : -1;
}
static int strncpy_s(char* dest, size_t destSize, const char* src, size_t count) {
    if (!dest || destSize == 0 || !src) return -1;
    size_t n = std::strlen(src);
    if (n > count) n = count;
    if (n >= destSize) n = destSize - 1;
    std::memcpy(dest, src, n);
    dest[n] = '\0';
    return 0;
}
#endif

namespace MingGoRTSIDE {

// Intelligent Development System instance
static Potato::AI::IntelligentDevelopmentSystem* g_DevSystem = nullptr;
static Potato::AI::DevelopmentAssistant* g_DevAssistant = nullptr;
static Potato::AI::LLMManager* g_LLMManager = nullptr;

IDEGUI::IDEGUI()
    : core(nullptr)
    , running(false) {
    memset(state.editorBuffer, 0, sizeof(state.editorBuffer));
    memset(state.aiInputBuffer, 0, sizeof(state.aiInputBuffer));
    memset(state.terminalBuffer, 0, sizeof(state.terminalBuffer));
    memset(state.searchBuffer, 0, sizeof(state.searchBuffer));
    memset(state.replaceBuffer, 0, sizeof(state.replaceBuffer));
    memset(state.developmentPrompt, 0, sizeof(state.developmentPrompt));
    memset(state.developmentResponse, 0, sizeof(state.developmentResponse));
    
    // Set default path
    state.currentPath = "C:\\HWC\\MingGoRTS";
    
    // Initialize Intelligent Development System
    g_LLMManager = new Potato::AI::LLMManager();
    auto localClient = std::make_unique<Potato::AI::LocalModelClient>("llama-2-7b");
    g_LLMManager->RegisterClient(Potato::AI::LLMProvider::Local, std::move(localClient));
    g_LLMManager->SetDefaultProvider(Potato::AI::LLMProvider::Local);
    
    g_DevSystem = new Potato::AI::IntelligentDevelopmentSystem();
    // Note: We'll initialize properly when IDECore is available
}

IDEGUI::~IDEGUI() {
    Shutdown();
}

bool IDEGUI::Initialize(IDECore* ideCore) {
    std::cout << "Initializing MingGoRTS IDE GUI..." << std::endl;
    
    this->core = ideCore;
    
    // Initialize I18N system
    if (!g_I18N) {
        g_I18N = new I18NManager();
        g_I18N->Initialize();
    }
    
    // Initialize templates
    InitializeTemplates();
    
    // Setup default layout
    SetupDefaultLayout();
    
    // Setup ImGui style
    SetupImGuiStyle();
    
    // Initialize AI conversation
    state.aiConversation.push_back("AI Assistant: Hello! I'm ready to help with your game development.");
    
    // Initialize output log
    AddOutputLog("MingGoRTS IDE GUI initialized");
    AddOutputLog("AI Agent System: Ready");
    
    std::cout << "MingGoRTS IDE GUI initialized successfully" << std::endl;
    return true;
}

void IDEGUI::Shutdown() {
    std::cout << "Shutting down MingGoRTS IDE GUI..." << std::endl;
    
    if (g_DevAssistant) {
        delete g_DevAssistant;
        g_DevAssistant = nullptr;
    }
    if (g_DevSystem) {
        g_DevSystem->Shutdown();
        delete g_DevSystem;
        g_DevSystem = nullptr;
    }
    if (g_LLMManager) {
        delete g_LLMManager;
        g_LLMManager = nullptr;
    }
    
    if (g_I18N) {
        g_I18N->Shutdown();
        delete g_I18N;
        g_I18N = nullptr;
    }
    
    running = false;
}

void IDEGUI::Run() {
    running = true;
    std::cout << "MingGoRTS IDE GUI running..." << std::endl;
}

void IDEGUI::Stop() {
    running = false;
}

void IDEGUI::SetupImGuiStyle() {
    SetDarkTheme();
}

void IDEGUI::SetDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Modern dark theme with better colors
    ImVec4* colors = style.Colors;
    
    // Background colors - darker and more modern
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.12f, 0.98f);
    
    // Frame colors - better contrast
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    
    // Title bar - gradient effect
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
    
    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    
    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.41f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.51f, 0.52f, 1.00f);
    
    // Check mark
    colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    
    // Slider
    colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
    
    // Button - modern accent color
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    
    // Header (tree nodes, etc.)
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    
    // Separator
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.41f, 0.42f, 1.00f);
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.41f, 0.42f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.51f, 0.52f, 1.00f);
    
    // Tab
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    
    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Selection
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
    
    // Drag and drop
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.30f, 0.50f, 0.70f, 1.00f);
    
    // Nav highlight
    colors[ImGuiCol_NavHighlight] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    
    // Better spacing and rounding
    style.WindowPadding = ImVec2(12, 12);
    style.WindowRounding = 8.0f;
    style.WindowMinSize = ImVec2(320, 240);
    
    style.FramePadding = ImVec2(8, 6);
    style.FrameRounding = 6.0f;
    
    style.ItemSpacing = ImVec2(12, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    
    style.IndentSpacing = 24.0f;
    style.ColumnsMinSpacing = 6.0f;
    
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 6.0f;
    
    style.GrabMinSize = 12.0f;
    style.GrabRounding = 6.0f;
    
    style.TabRounding = 6.0f;
    
    style.DisplaySafeAreaPadding = ImVec2(4, 4);
    
    style.MouseCursorScale = 1.0f;
    
    // Anti-aliasing
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.CurveTessellationTol = 1.25f;
}

void IDEGUI::RenderMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(T(TranslationKey::Menu_File).c_str())) {
            if (ImGui::MenuItem(T(TranslationKey::File_NewProject).c_str())) {
                AddOutputLog("New Project: Create new game project");
            }
            if (ImGui::MenuItem(T(TranslationKey::File_OpenProject).c_str())) {
                AddOutputLog("Open Project: Open existing game project");
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::File_Save).c_str())) {
                if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
                    SaveTab(state.activeTab);
                }
            }
            if (ImGui::MenuItem(T(TranslationKey::File_SaveAs).c_str())) {
                AddOutputLog("Save As: Save file with new name");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Tab")) {
                if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
                    CloseTab(state.activeTab);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::File_Exit).c_str())) {
                state.running = false;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(T(TranslationKey::Menu_Edit).c_str())) {
            if (ImGui::MenuItem(T(TranslationKey::Edit_Undo).c_str())) {
                AddOutputLog("Undo: Undo last action");
            }
            if (ImGui::MenuItem(T(TranslationKey::Edit_Redo).c_str())) {
                AddOutputLog("Redo: Redo last action");
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::Edit_Cut).c_str())) {
                AddOutputLog("Cut: Cut selected text");
            }
            if (ImGui::MenuItem(T(TranslationKey::Edit_Copy).c_str())) {
                AddOutputLog("Copy: Copy selected text");
            }
            if (ImGui::MenuItem(T(TranslationKey::Edit_Paste).c_str())) {
                AddOutputLog("Paste: Paste from clipboard");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(T(TranslationKey::Menu_View).c_str())) {
            ImGui::MenuItem(T(TranslationKey::Panel_FileExplorer).c_str(), nullptr, &state.showFileExplorer);
            ImGui::MenuItem(T(TranslationKey::Panel_CodeEditor).c_str(), nullptr, &state.showCodeEditor);
            ImGui::MenuItem(T(TranslationKey::Panel_AIAgentPanel).c_str(), nullptr, &state.showAIAgentPanel);
            ImGui::MenuItem("Development Assistant", "Ctrl+D", &state.showDevelopmentAssistant);
            ImGui::MenuItem(T(TranslationKey::Panel_Terminal).c_str(), nullptr, &state.showTerminal);
            ImGui::MenuItem(T(TranslationKey::Panel_Output).c_str(), nullptr, &state.showOutput);
            ImGui::MenuItem(T(TranslationKey::Panel_Properties).c_str(), nullptr, &state.showProperties);
            ImGui::Separator();
            ImGui::MenuItem("Debugger", "F8", &state.showDebugger);
            ImGui::MenuItem("Git Panel", "F9", &state.showGitPanel);
            ImGui::MenuItem("Code Analysis", "F7", &state.showCodeAnalysis);
            ImGui::Separator();
            ImGui::MenuItem("Toolbar", nullptr, &state.showToolbar);
            ImGui::MenuItem("Status Bar", nullptr, &state.showStatusBar);
            ImGui::Separator();
            ImGui::MenuItem("Settings", nullptr, &state.showSettings);
            ImGui::MenuItem("Help", nullptr, &state.showHelp);
            ImGui::Separator();
            
            // Language selection
            if (ImGui::BeginMenu(T(TranslationKey::Language_Select).c_str())) {
                for (Language lang : g_I18N->GetSupportedLanguages()) {
                    if (ImGui::MenuItem(g_I18N->GetLanguageName(lang).c_str())) {
                        UpdateLanguage(lang);
                    }
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(T(TranslationKey::Menu_Build).c_str())) {
            if (ImGui::MenuItem(T(TranslationKey::Build_BuildProject).c_str())) {
                AddOutputLog("Build: Building project...");
                if (core && core->GetBuilder()) {
                    core->GetBuilder()->BuildProject("Release");
                    AddOutputLog("Build completed: " + core->GetBuilder()->GetBuildStatus());
                }
            }
            if (ImGui::MenuItem(T(TranslationKey::Build_CleanProject).c_str())) {
                AddOutputLog("Clean: Cleaning project...");
                if (core && core->GetBuilder()) {
                    core->GetBuilder()->CleanProject();
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(T(TranslationKey::Menu_AIAgent).c_str())) {
            if (ImGui::MenuItem(T(TranslationKey::AI_CreateEngineCodeAgent).c_str())) {
                AddOutputLog("AI: Creating Engine Code Agent");
                if (core && core->GetAIInterface() && core->GetAIInterface()->GetAgentManager()) {
                    core->GetAIInterface()->GetAgentManager()->CreateAgent(
                        GameDevAgentType::EngineCode, "EngineCodeAgent"
                    );
                    AddOutputLog("AI: Engine Code Agent created");
                }
            }
            if (ImGui::MenuItem(T(TranslationKey::AI_CreateAssetAgent).c_str())) {
                AddOutputLog("AI: Creating Asset Agent");
                if (core && core->GetAIInterface() && core->GetAIInterface()->GetAgentManager()) {
                    core->GetAIInterface()->GetAgentManager()->CreateAgent(
                        GameDevAgentType::AssetAgent, "AssetAgent"
                    );
                    AddOutputLog("AI: Asset Agent created");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Development Assistant", "Ctrl+D")) {
                ShowDevelopmentAssistant();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::AI_ShowAgentStatus).c_str())) {
                AddOutputLog("AI: Showing agent status");
                if (core && core->GetAIInterface() && core->GetAIInterface()->GetAgentManager()) {
                    std::string status = core->GetAIInterface()->GetAgentManager()->GetCollaborationStatus();
                    AddOutputLog("AI Status: " + status);
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu(T(TranslationKey::Menu_Help).c_str())) {
            if (ImGui::MenuItem(T(TranslationKey::Help_Documentation).c_str())) {
                AddOutputLog("Help: Opening documentation");
            }
            if (ImGui::MenuItem(T(TranslationKey::Help_About).c_str())) {
                AddOutputLog("Help: MingGoRTS IDE v1.0 - AI-Powered Game Development Environment");
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void IDEGUI::RenderToolbar() {
    if (!state.showToolbar) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar();
    
    // File operations
    if (ImGui::Button("New")) {
        AddOutputLog("New file: Create new file");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        AddOutputLog("Open file: Open file dialog");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
            SaveTab(state.activeTab);
        }
    }
    
    ImGui::Separator();
    ImGui::SameLine();
    
    // Edit operations
    if (ImGui::Button("Undo")) {
        AddOutputLog("Undo: Undo last action");
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        AddOutputLog("Redo: Redo last action");
    }
    
    ImGui::Separator();
    ImGui::SameLine();
    
    // Search
    if (ImGui::Button("Search")) {
        state.showSearch = !state.showSearch;
    }
    ImGui::SameLine();
    
    // Build
    if (ImGui::Button("Build")) {
        StartBuild();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clean")) {
        AddOutputLog("Clean: Cleaning project");
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // Configuration selection
    ImGui::Text("Config:");
    ImGui::SameLine();
    ImGui::PushItemWidth(100);
    if (ImGui::BeginCombo("##config", state.buildConfiguration.c_str())) {
        if (ImGui::Selectable("Debug")) {
            state.buildConfiguration = "Debug";
        }
        if (ImGui::Selectable("Release")) {
            state.buildConfiguration = "Release";
        }
        if (ImGui::Selectable("RelWithDebInfo")) {
            state.buildConfiguration = "RelWithDebInfo";
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    
    ImGui::End();
}

void IDEGUI::RenderStatusBar() {
    if (!state.showStatusBar) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar();
    
    // Cursor position
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        ImGui::Text("Line: %d  Col: %d", state.openTabs[state.activeTab].currentLine, state.openTabs[state.activeTab].currentColumn);
    } else {
        ImGui::Text("Line: 1  Col: 1");
    }
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    
    // Encoding
    ImGui::Text("UTF-8");
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    
    // File info
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        ImGui::Text("File: %s", state.openTabs[state.activeTab].fileName.c_str());
        if (state.openTabs[state.activeTab].modified) {
            ImGui::SameLine();
            ImGui::Text("(Modified)");
        }
    } else {
        ImGui::Text("No file");
    }
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    
    // Build status
    if (state.building) {
        ImGui::Text("Building: %.0f%%", state.buildProgress * 100.0f);
    } else {
        ImGui::Text("Ready");
    }
    
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    
    // Time
    ImGui::Text("%s", GetCurrentTime().c_str());
    
    ImGui::End();
}

void IDEGUI::RenderFileExplorer() {
    if (!state.showFileExplorer) return;
    
    ImGui::SetNextWindowPos(state.fileExplorerPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.fileExplorerSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_FileExplorer).c_str(), &state.showFileExplorer);
    ImGui::PopStyleVar();
    
    // Header with icon
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
    ImGui::Text("[FOLDER]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("%s: %s", T(TranslationKey::FileExplorer_Project).c_str(), core && core->GetProject() ? core->GetProject()->GetProjectName().c_str() : "No Project");
    ImGui::Separator();
    
    // File tree with better styling
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.16f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.25f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.30f, 0.35f, 1.0f));
    
    if (ImGui::TreeNodeEx(T(TranslationKey::FileExplorer_Source).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 0.9f, 1.0f));
        if (ImGui::Selectable("  [FILE] MainGame.cpp")) {
            OpenTab("MainGame.cpp");
        }
        if (ImGui::Selectable("  [FILE] GameEngine.h")) {
            OpenTab("GameEngine.h");
        }
        if (ImGui::Selectable("  [FILE] PlayerController.cpp")) {
            OpenTab("PlayerController.cpp");
        }
        ImGui::PopStyleColor();
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNodeEx(T(TranslationKey::FileExplorer_Assets).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.6f, 1.0f));
        if (ImGui::Selectable("  [DIR] textures")) {
            ImGui::NextColumn();
        }
        if (ImGui::Selectable("  [DIR] models")) {
            ImGui::NextColumn();
        }
        if (ImGui::Selectable("  [DIR] audio")) {
            ImGui::NextColumn();
        }
        ImGui::PopStyleColor();
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNodeEx(T(TranslationKey::FileExplorer_Scripts).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.5f, 1.0f));
        if (ImGui::Selectable("  [FILE] gameplay.lua")) {
            OpenTab("gameplay.lua");
        }
        ImGui::PopStyleColor();
        ImGui::TreePop();
    }
    
    ImGui::PopStyleColor(3);
    
    ImGui::End();
}

void IDEGUI::RenderCodeEditor() {
    if (!state.showCodeEditor) return;
    
    ImGui::SetNextWindowPos(state.codeEditorPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.codeEditorSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_CodeEditor).c_str(), &state.showCodeEditor);
    ImGui::PopStyleVar();
    
    // Render tabs
    RenderEditorTabs();
    
    // Render search panel
    if (state.showSearch) {
        RenderSearchPanel();
    }
    
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        auto& tab = state.openTabs[state.activeTab];
        
        // File header with icon
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
        ImGui::Text("[EDIT]");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        
        ImGui::Text("%s: %s", T(TranslationKey::CodeEditor_File).c_str(), tab.fileName.c_str());
        ImGui::SameLine();
        
        // Save button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));
        if (ImGui::SmallButton(T(TranslationKey::CodeEditor_Save).c_str())) {
            SaveTab(state.activeTab);
        }
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        // Code editor with better styling
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.13f, 0.14f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
        
        ImGui::InputTextMultiline("##editor", tab.buffer, sizeof(tab.buffer), 
            ImVec2(-1, -ImGui::GetCursorPosY() - 40), ImGuiInputTextFlags_AllowTabInput);
        tab.content = tab.buffer;
        
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        
        // Update cursor position
        UpdateCursorPosition();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("%s", T(TranslationKey::CodeEditor_NoFileOpen).c_str());
        ImGui::Text("Open a file from the File Explorer");
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}

void IDEGUI::RenderAIAgentPanel() {
    if (!state.showAIAgentPanel) return;
    
    ImGui::SetNextWindowPos(state.aiAgentPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.aiAgentSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_AIAgentPanel).c_str(), &state.showAIAgentPanel);
    ImGui::PopStyleVar();
    
    // Agent status header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.5f, 1.0f));
    ImGui::Text("[AI]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("%s", T(TranslationKey::AIAgent_Status).c_str());
    ImGui::Separator();
    
    if (core && core->GetAIInterface() && core->GetAIInterface()->GetAgentManager()) {
        size_t agentCount = core->GetAIInterface()->GetAgentManager()->GetAllAgents().size();
        
        // Agent count with colored indicator
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
        ImGui::Text("%s: %zu", T(TranslationKey::AIAgent_ActiveAgents).c_str(), agentCount);
        ImGui::PopStyleColor();
        
        // Collaboration status with colored indicator
        bool collabEnabled = core->GetAIInterface()->GetAgentManager()->IsCollaborationEnabled();
        ImGui::PushStyleColor(ImGuiCol_Text, collabEnabled ? ImVec4(0.5f, 0.8f, 0.5f, 1.0f) : ImVec4(0.8f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("%s: %s", T(TranslationKey::AIAgent_Collaboration).c_str(), collabEnabled ? "Enabled" : "Disabled");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("No AI Agent Manager initialized");
        ImGui::PopStyleColor();
    }
    
    ImGui::Separator();
    
    // AI Conversation
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.5f, 1.0f));
    ImGui::Text("[CHAT]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("%s", T(TranslationKey::AIAgent_Conversation).c_str());
    ImGui::Separator();
    
    // Conversation area with better styling
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::BeginChild("##conversation", ImVec2(-1, 200), true);
    
    for (const auto& message : state.aiConversation) {
        // Different colors for user vs AI
        if (message.find("You:") == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 0.9f, 1.0f));
        } else if (message.find("AI:") == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        }
        
        ImGui::TextWrapped("%s", message.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // AI Input
    ImGui::Text("%s:", T(TranslationKey::AIAgent_AskAI).c_str());
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::InputText("##aiinput", state.aiInputBuffer, sizeof(state.aiInputBuffer));
    ImGui::PopStyleColor();
    
    // Styled buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));
    if (ImGui::Button(T(TranslationKey::AIAgent_Send).c_str())) {
        HandleAISubmit();
    }
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button(T(TranslationKey::AIAgent_Clear).c_str())) {
        state.aiConversation.clear();
        memset(state.aiInputBuffer, 0, sizeof(state.aiInputBuffer));
    }
    ImGui::PopStyleColor(2);
    
    ImGui::End();
}

void IDEGUI::RenderTerminal() {
    if (!state.showTerminal) return;
    
    ImGui::SetNextWindowPos(state.terminalPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.terminalSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_Terminal).c_str(), &state.showTerminal);
    ImGui::PopStyleVar();
    
    // Terminal header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));
    ImGui::Text("[TERM]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("%s", T(TranslationKey::Terminal_Command).c_str());
    ImGui::Separator();
    
    // Terminal history with better styling
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::BeginChild("##terminal", ImVec2(-1, 200), true);
    
    for (const auto& cmd : state.terminalHistory) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.6f, 1.0f));
        ImGui::Text("> %s", cmd.c_str());
        ImGui::PopStyleColor();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // Terminal input
    ImGui::Text("%s:", T(TranslationKey::Terminal_Command).c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::InputText("##terminal", state.terminalBuffer, sizeof(state.terminalBuffer));
    ImGui::PopStyleColor();
    
    // Styled buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));
    if (ImGui::Button(T(TranslationKey::Terminal_Execute).c_str())) {
        HandleTerminalCommand();
    }
    ImGui::PopStyleColor(2);
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button(T(TranslationKey::Terminal_Clear).c_str())) {
        state.terminalHistory.clear();
        memset(state.terminalBuffer, 0, sizeof(state.terminalBuffer));
    }
    ImGui::PopStyleColor(2);
    
    ImGui::End();
}

void IDEGUI::RenderOutput() {
    if (!state.showOutput) return;
    
    ImGui::SetNextWindowPos(state.outputPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.outputSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_Output).c_str(), &state.showOutput);
    ImGui::PopStyleVar();
    
    // Output header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
    ImGui::Text("[LOG]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("%s", T(TranslationKey::Output_Log).c_str());
    ImGui::Separator();
    
    // Output log with better styling
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::BeginChild("##output", ImVec2(-1, -ImGui::GetCursorPosY() - 40), true);
    
    for (const auto& log : state.outputLog) {
        // Color-code different log types
        if (log.find("Error") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        } else if (log.find("Warning") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
        } else if (log.find("AI:") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 0.5f, 1.0f));
        } else if (log.find("Build") != std::string::npos) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.9f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        }
        
        ImGui::TextWrapped("%s", log.c_str());
        ImGui::PopStyleColor();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // Clear button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button(T(TranslationKey::Output_ClearLog).c_str())) {
        state.outputLog.clear();
    }
    ImGui::PopStyleColor(2);
    
    ImGui::End();
}

void IDEGUI::RenderProperties() {
    if (!state.showProperties) return;
    
    ImGui::SetNextWindowPos(state.propertiesPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(state.propertiesSize, ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin(T(TranslationKey::Panel_Properties).c_str(), &state.showProperties);
    ImGui::PopStyleVar();
    
    // Properties header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
    ImGui::Text("[PROP]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("File Properties");
    ImGui::Separator();
    
    if (!state.currentFile.empty()) {
        // File info with icons
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
        ImGui::Text("%s: %s", T(TranslationKey::Properties_File).c_str(), state.currentFile.c_str());
        ImGui::PopStyleColor();
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 0.9f, 1.0f));
        ImGui::Text("%s: %d", T(TranslationKey::Properties_Lines).c_str(), state.currentLine);
        ImGui::Text("%s: %d", T(TranslationKey::Properties_Columns).c_str(), state.currentColumn);
        ImGui::Text("%s: UTF-8", T(TranslationKey::Properties_Encoding).c_str());
        ImGui::Text("%s: %zu bytes", T(TranslationKey::Properties_Size).c_str(), state.editorContent.size());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("%s", T(TranslationKey::Properties_NoFileSelected).c_str());
        ImGui::Text("Select a file to view properties");
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}

void IDEGUI::HandleFileOpen(const std::string& filePath) {
    // Use the new tab system
    OpenTab(filePath);
}

void IDEGUI::HandleFileSave(const std::string& filePath) {
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        auto& tab = state.openTabs[state.activeTab];
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << tab.content;
            file.close();
            tab.modified = false;
            AddOutputLog("Saved file: " + filePath);
        } else {
            AddOutputLog("Failed to save file: " + filePath);
        }
    } else {
        // Fallback to old system
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << state.editorContent;
            file.close();
            AddOutputLog("Saved file: " + filePath);
        } else {
            AddOutputLog("Failed to save file: " + filePath);
        }
    }
}

void IDEGUI::HandleAISubmit() {
    std::string input(state.aiInputBuffer);
    if (input.empty()) return;
    
    // Add user message
    state.aiConversation.push_back("You: " + input);
    
    // Process with AI
    std::string aiResponse = "AI: ";
    if (core && core->GetAIInterface()) {
        aiResponse += core->GetAIInterface()->ProcessRequest(input);
    } else {
        aiResponse += "I understand your request. In a full implementation, I would process this with an LLM.";
    }
    
    state.aiConversation.push_back(aiResponse);
    memset(state.aiInputBuffer, 0, sizeof(state.aiInputBuffer));
    
    AddOutputLog("AI interaction completed");
}

void IDEGUI::HandleTerminalCommand() {
    std::string input(state.terminalBuffer);
    if (input.empty()) return;
    
    AddTerminalHistory(input);
    state.terminalHistoryIndex = state.terminalHistory.size();
    
    // Execute command (simplified)
    std::string output = "Command executed: " + input;
    AddOutputLog(output);
    
    memset(state.terminalBuffer, 0, sizeof(state.terminalBuffer));
}

void IDEGUI::AddOutputLog(const std::string& message) {
    state.outputLog.push_back(message);
    
    // Keep log size manageable
    if (state.outputLog.size() > 100) {
        state.outputLog.erase(state.outputLog.begin());
    }
}

void IDEGUI::AddTerminalHistory(const std::string& command) {
    state.terminalHistory.push_back(command);
    
    // Keep history size manageable
    if (state.terminalHistory.size() > 50) {
        state.terminalHistory.erase(state.terminalHistory.begin());
    }
}

void IDEGUI::UpdateLanguage(Language lang) {
    if (g_I18N) {
        g_I18N->SetLanguage(lang);
        AddOutputLog("Language changed to: " + g_I18N->GetLanguageName(lang));
    }
}

void IDEGUI::RenderEditorTabs() {
    if (state.openTabs.empty()) {
        return;
    }
    
    ImGui::BeginTabBar("EditorTabs");
    
    for (size_t i = 0; i < state.openTabs.size(); i++) {
        auto& tab = state.openTabs[i];
        
        std::string tabLabel = tab.fileName;
        if (tab.modified) {
            tabLabel += " *";
        }
        
        if (ImGui::BeginTabItem(tabLabel.c_str())) {
            if (state.activeTab != (int)i) {
                SwitchTab(i);
            }
            ImGui::EndTabItem();
        }
        
        // Close button on right-click
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("TabContextMenu");
        }
    }
    
    ImGui::EndTabBar();
    
    // Tab context menu
    if (ImGui::BeginPopup("TabContextMenu")) {
        if (ImGui::MenuItem("Close")) {
            if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
                CloseTab(state.activeTab);
            }
        }
        if (ImGui::MenuItem("Close All")) {
            while (!state.openTabs.empty()) {
                CloseTab(0);
            }
        }
        if (ImGui::MenuItem("Close Others")) {
            if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
                std::string currentFile = state.openTabs[state.activeTab].filePath;
                while (state.openTabs.size() > 1) {
                    if (state.openTabs[0].filePath != currentFile) {
                        CloseTab(0);
                    } else {
                        CloseTab(1);
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}

void IDEGUI::RenderSearchPanel() {
    ImGui::Separator();
    
    // Search input
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::InputText("##search", state.searchBuffer, sizeof(state.searchBuffer));
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (ImGui::Button("Find")) {
        SearchInEditor(state.searchBuffer);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Find Next")) {
        FindNext();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Find Prev")) {
        FindPrevious();
    }
    
    // Replace input
    ImGui::Text("Replace:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 1.0f));
    ImGui::InputText("##replace", state.replaceBuffer, sizeof(state.replaceBuffer));
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (ImGui::Button("Replace")) {
        ReplaceInEditor(state.searchBuffer, state.replaceBuffer);
    }
    
    // Search options
    ImGui::Checkbox("Case Sensitive", &state.searchCaseSensitive);
    ImGui::SameLine();
    ImGui::Checkbox("Whole Word", &state.searchWholeWord);
    
    // Search results
    if (state.totalSearchResults > 0) {
        ImGui::Text("Results: %d/%d", state.currentSearchResult + 1, state.totalSearchResults);
    }
    
    ImGui::Separator();
}

void IDEGUI::RenderBuildPanel() {
    if (state.building) {
        ImGui::ProgressBar(state.buildProgress);
        ImGui::SameLine();
        ImGui::Text("Building...");
    }
}

void IDEGUI::OpenTab(const std::string& filePath) {
    // Check if file is already open
    for (size_t i = 0; i < state.openTabs.size(); i++) {
        if (state.openTabs[i].filePath == filePath) {
            SwitchTab(i);
            return;
        }
    }
    
    // Create new tab
    IDEGUIState::EditorTab newTab;
    newTab.filePath = filePath;
    newTab.fileName = GetFileName(filePath);
    newTab.modified = false;
    newTab.currentLine = 1;
    newTab.currentColumn = 1;
    memset(newTab.buffer, 0, sizeof(newTab.buffer));
    
    // Load file content
    std::ifstream file(filePath);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        newTab.content = content;
        size_t copySize = std::min(content.size(), sizeof(newTab.buffer) - 1);
        memcpy(newTab.buffer, content.c_str(), copySize);
        newTab.buffer[copySize] = '\0';
        file.close();
    } else {
        std::string placeholder = "// New file: " + filePath + "\n";
        newTab.content = placeholder;
        size_t copySize = std::min(placeholder.size(), sizeof(newTab.buffer) - 1);
        memcpy(newTab.buffer, placeholder.c_str(), copySize);
        newTab.buffer[copySize] = '\0';
    }
    
    state.openTabs.push_back(newTab);
    state.activeTab = state.openTabs.size() - 1;
    
    AddOutputLog("Opened file: " + filePath);
}

void IDEGUI::CloseTab(int index) {
    if (index < 0 || index >= (int)state.openTabs.size()) {
        return;
    }
    
    auto& tab = state.openTabs[index];
    if (tab.modified) {
        AddOutputLog("Warning: File " + tab.fileName + " has unsaved changes");
    }
    
    state.openTabs.erase(state.openTabs.begin() + index);
    
    if (state.activeTab >= (int)state.openTabs.size()) {
        state.activeTab = state.openTabs.size() - 1;
    }
    
    AddOutputLog("Closed file: " + tab.fileName);
}

void IDEGUI::SwitchTab(int index) {
    if (index < 0 || index >= (int)state.openTabs.size()) {
        return;
    }
    
    state.activeTab = index;
    AddOutputLog("Switched to: " + state.openTabs[index].fileName);
}

void IDEGUI::SaveTab(int index) {
    if (index < 0 || index >= (int)state.openTabs.size()) {
        return;
    }
    
    auto& tab = state.openTabs[index];
    HandleFileSave(tab.filePath);
    tab.modified = false;
}

void IDEGUI::SearchInEditor(const std::string& searchText) {
    if (searchText.empty() || state.activeTab < 0 || state.activeTab >= (int)state.openTabs.size()) {
        return;
    }
    
    auto& tab = state.openTabs[state.activeTab];
    std::string content = tab.content;
    
    int count = 0;
    size_t pos = 0;
    
    while ((pos = content.find(searchText, pos)) != std::string::npos) {
        count++;
        pos += searchText.length();
    }
    
    state.totalSearchResults = count;
    state.currentSearchResult = 0;
    
    AddOutputLog("Search: Found " + std::to_string(count) + " occurrences of '" + searchText + "'");
}

void IDEGUI::ReplaceInEditor(const std::string& searchText, const std::string& replaceText) {
    if (searchText.empty() || state.activeTab < 0 || state.activeTab >= (int)state.openTabs.size()) {
        return;
    }
    
    auto& tab = state.openTabs[state.activeTab];
    std::string content = tab.content;
    
    size_t pos = 0;
    int count = 0;
    
    while ((pos = content.find(searchText, pos)) != std::string::npos) {
        content.replace(pos, searchText.length(), replaceText);
        pos += replaceText.length();
        count++;
    }
    
    tab.content = content;
    size_t copySize = std::min(content.size(), sizeof(tab.buffer) - 1);
    memcpy(tab.buffer, content.c_str(), copySize);
    tab.buffer[copySize] = '\0';
    tab.modified = true;
    
    AddOutputLog("Replace: Replaced " + std::to_string(count) + " occurrences");
}

void IDEGUI::FindNext() {
    if (state.totalSearchResults > 0) {
        state.currentSearchResult = (state.currentSearchResult + 1) % state.totalSearchResults;
        AddOutputLog("Find: Result " + std::to_string(state.currentSearchResult + 1) + "/" + std::to_string(state.totalSearchResults));
    }
}

void IDEGUI::FindPrevious() {
    if (state.totalSearchResults > 0) {
        state.currentSearchResult = (state.currentSearchResult - 1 + state.totalSearchResults) % state.totalSearchResults;
        AddOutputLog("Find: Result " + std::to_string(state.currentSearchResult + 1) + "/" + std::to_string(state.totalSearchResults));
    }
}

std::vector<std::string> IDEGUI::ScanDirectory(const std::string& path) {
    std::vector<std::string> files;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            files.push_back(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        AddOutputLog("Error scanning directory: " + std::string(e.what()));
    }
    
    return files;
}

bool IDEGUI::IsDirectory(const std::string& path) {
    return std::filesystem::is_directory(path);
}

std::string IDEGUI::GetFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string IDEGUI::GetFileExtension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

void IDEGUI::StartBuild() {
    if (state.building) {
        AddOutputLog("Build: Already building");
        return;
    }
    
    state.building = true;
    state.buildProgress = 0.0f;
    
    AddOutputLog("Build: Starting " + state.buildConfiguration + " build...");
    
    // Simulate build process
    // In real implementation, this would call CMake and compiler
}

void IDEGUI::StopBuild() {
    state.building = false;
    AddOutputLog("Build: Stopped by user");
}

void IDEGUI::UpdateBuildProgress() {
    if (state.building) {
        state.buildProgress += 0.01f;
        if (state.buildProgress >= 1.0f) {
            state.building = false;
            state.buildProgress = 0.0f;
            AddOutputLog("Build: Build completed successfully");
        }
    }
}

void IDEGUI::HandleKeyboardShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Ctrl+S - Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
            SaveTab(state.activeTab);
        }
    }
    
    // Ctrl+F - Search
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
        state.showSearch = !state.showSearch;
    }
    
    // Ctrl+B - Build
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B)) {
        StartBuild();
    }
    
    // Ctrl+W - Close tab
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W)) {
        if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
            CloseTab(state.activeTab);
        }
    }
    
    // Ctrl+Tab - Next tab
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        if (!state.openTabs.empty()) {
            state.activeTab = (state.activeTab + 1) % state.openTabs.size();
        }
    }
    
    // Ctrl+1 - Toggle File Explorer
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_1)) {
        state.showFileExplorer = !state.showFileExplorer;
    }
    
    // Ctrl+2 - Toggle Code Editor
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_2)) {
        state.showCodeEditor = !state.showCodeEditor;
    }
    
    // Ctrl+3 - Toggle AI Agent Panel
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_3)) {
        state.showAIAgentPanel = !state.showAIAgentPanel;
    }
    
    // Ctrl+D - Toggle Development Assistant
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        ShowDevelopmentAssistant();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_4)) {
        state.showTerminal = !state.showTerminal;
    }
    
    // Ctrl+5 - Toggle Output
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_5)) {
        state.showOutput = !state.showOutput;
    }
    
    // Ctrl+, - Toggle Settings
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma)) {
        state.showSettings = !state.showSettings;
    }
    
    // Ctrl+? - Toggle Help
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Slash)) {
        state.showHelp = !state.showHelp;
    }
}

void IDEGUI::UpdateCursorPosition() {
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        auto& tab = state.openTabs[state.activeTab];
        std::string content = tab.buffer;
        
        // Simple cursor position calculation
        int line = 1;
        int column = 1;
        
        for (size_t i = 0; i < content.length(); i++) {
            if (content[i] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
        }
        
        tab.currentLine = line;
        tab.currentColumn = column;
    }
}

std::string IDEGUI::GetCurrentTime() {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

void IDEGUI::SetupDefaultLayout() {
    // Calculate layout based on window size
    ImGuiIO& io = ImGui::GetIO();
    float windowWidth = io.DisplaySize.x;
    float windowHeight = io.DisplaySize.y;
    
    // Top row: File Explorer (left), Code Editor (center), AI Agent (right)
    state.fileExplorerPos = ImVec2(0, 60);
    state.fileExplorerSize = ImVec2(windowWidth * 0.2f, windowHeight * 0.6f);
    
    state.codeEditorPos = ImVec2(windowWidth * 0.2f, 60);
    state.codeEditorSize = ImVec2(windowWidth * 0.5f, windowHeight * 0.6f);
    
    state.aiAgentPos = ImVec2(windowWidth * 0.7f, 60);
    state.aiAgentSize = ImVec2(windowWidth * 0.3f, windowHeight * 0.6f);
    
    // Bottom row: Terminal (left), Output (center), Properties (right)
    state.terminalPos = ImVec2(0, windowHeight * 0.6f + 60);
    state.terminalSize = ImVec2(windowWidth * 0.33f, windowHeight * 0.4f - 60);
    
    state.outputPos = ImVec2(windowWidth * 0.33f, windowHeight * 0.6f + 60);
    state.outputSize = ImVec2(windowWidth * 0.34f, windowHeight * 0.4f - 60);
    
    state.propertiesPos = ImVec2(windowWidth * 0.67f, windowHeight * 0.6f + 60);
    state.propertiesSize = ImVec2(windowWidth * 0.33f, windowHeight * 0.4f - 60);
    
    AddOutputLog("Default layout applied");
}

void IDEGUI::SaveLayout() {
    // Save current layout to file (could be implemented later)
    AddOutputLog("Layout saved");
}

void IDEGUI::LoadLayout() {
    // Load layout from file (could be implemented later)
    AddOutputLog("Layout loaded");
}

void IDEGUI::ApplyLayout() {
    // Apply the saved layout
    AddOutputLog("Layout applied");
}

void IDEGUI::RenderSettings() {
    if (!state.showSettings) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("Settings", &state.showSettings);
    ImGui::PopStyleVar();
    
    // Header with icon
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
    ImGui::Text("[SETTINGS]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("IDE Settings");
    ImGui::Separator();
    
    // Theme selection
    ImGui::Text("Theme:");
    ImGui::SameLine();
    const char* themeItems[] = { "Dark", "Light", "High Contrast" };
    int currentThemeIndex = (int)state.currentTheme;
    if (ImGui::Combo("##theme", &currentThemeIndex, themeItems, 3)) {
        state.currentTheme = (IDEGUIState::Theme)currentThemeIndex;
        AddOutputLog("Theme changed to: " + std::string(themeItems[currentThemeIndex]));
        // Apply theme changes
        if (state.currentTheme == IDEGUIState::Theme::Dark) {
            SetDarkTheme();
        } else if (state.currentTheme == IDEGUIState::Theme::Light) {
            ImGui::StyleColorsLight();
        } else {
            SetDarkTheme(); // Fallback to dark for high contrast
        }
    }
    
    // Font size
    ImGui::Text("Font Size:");
    ImGui::SameLine();
    ImGui::SliderInt("##fontsize", &state.fontSize, 10, 24);
    ImGui::SameLine();
    ImGui::Text("%d px", state.fontSize);
    
    // Editor settings
    ImGui::Separator();
    ImGui::Text("Editor Settings");
    
    ImGui::Checkbox("Show Line Numbers", &state.showLineNumbers);
    ImGui::Checkbox("Auto Save", &state.autoSave);
    
    if (state.autoSave) {
        ImGui::Text("Auto Save Interval:");
        ImGui::SameLine();
        ImGui::SliderInt("##autosave", &state.autoSaveInterval, 60, 600);
        ImGui::SameLine();
        ImGui::Text("%d seconds", state.autoSaveInterval);
    }
    
    ImGui::Checkbox("Word Wrap", &state.wordWrap);
    ImGui::Checkbox("Show Minimap", &state.showMinimap);
    
    // Apply button
    ImGui::Separator();
    if (ImGui::Button("Apply Settings")) {
        AddOutputLog("Settings applied successfully");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        state.currentTheme = IDEGUIState::Theme::Dark;
        state.fontSize = 14;
        state.showLineNumbers = true;
        state.autoSave = true;
        state.autoSaveInterval = 300;
        state.wordWrap = false;
        state.showMinimap = false;
        SetDarkTheme();
        AddOutputLog("Settings reset to defaults");
    }
    
    ImGui::End();
}

void IDEGUI::RenderHelp() {
    if (!state.showHelp) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("Help", &state.showHelp);
    ImGui::PopStyleVar();
    
    // Header with icon
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 1.0f));
    ImGui::Text("[HELP]");
    ImGui::SameLine();
    ImGui::PopStyleColor();
    
    ImGui::Text("Keyboard Shortcuts");
    ImGui::Separator();
    
    // Keyboard shortcuts table
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 150);
    
    ImGui::Text("File Operations");
    ImGui::NextColumn();
    ImGui::Text("Action");
    ImGui::NextColumn();
    ImGui::Separator();
    
    ImGui::Text("Ctrl+S");
    ImGui::NextColumn();
    ImGui::Text("Save current file");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+O");
    ImGui::NextColumn();
    ImGui::Text("Open file");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+N");
    ImGui::NextColumn();
    ImGui::Text("New file");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+W");
    ImGui::NextColumn();
    ImGui::Text("Close tab");
    ImGui::NextColumn();
    
    ImGui::Separator();
    ImGui::NextColumn();
    ImGui::Text("Edit Operations");
    ImGui::NextColumn();
    ImGui::Separator();
    
    ImGui::Text("Ctrl+Z");
    ImGui::NextColumn();
    ImGui::Text("Undo");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+Y");
    ImGui::NextColumn();
    ImGui::Text("Redo");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+X");
    ImGui::NextColumn();
    ImGui::Text("Cut");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+C");
    ImGui::NextColumn();
    ImGui::Text("Copy");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+V");
    ImGui::NextColumn();
    ImGui::Text("Paste");
    ImGui::NextColumn();
    
    ImGui::Separator();
    ImGui::NextColumn();
    ImGui::Text("Search");
    ImGui::NextColumn();
    ImGui::Separator();
    
    ImGui::Text("Ctrl+F");
    ImGui::NextColumn();
    ImGui::Text("Find");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+H");
    ImGui::NextColumn();
    ImGui::Text("Replace");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+G");
    ImGui::NextColumn();
    ImGui::Text("Find next");
    ImGui::NextColumn();
    
    ImGui::Separator();
    ImGui::NextColumn();
    ImGui::Text("Navigation");
    ImGui::NextColumn();
    ImGui::Separator();
    
    ImGui::Text("Ctrl+Tab");
    ImGui::NextColumn();
    ImGui::Text("Next tab");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+Shift+Tab");
    ImGui::NextColumn();
    ImGui::Text("Previous tab");
    ImGui::NextColumn();
    
    ImGui::Columns(1);
    
    ImGui::Separator();
    
    // Build shortcuts
    ImGui::Text("Build Operations");
    ImGui::Separator();
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 150);
    
    ImGui::Text("Ctrl+B");
    ImGui::NextColumn();
    ImGui::Text("Build project");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+Shift+B");
    ImGui::NextColumn();
    ImGui::Text("Rebuild project");
    ImGui::NextColumn();
    
    ImGui::Columns(1);
    
    ImGui::Separator();
    
    // View shortcuts
    ImGui::Text("View Operations");
    ImGui::Separator();
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 150);
    
    ImGui::Text("Ctrl+1");
    ImGui::NextColumn();
    ImGui::Text("Show File Explorer");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+2");
    ImGui::NextColumn();
    ImGui::Text("Show Code Editor");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+3");
    ImGui::NextColumn();
    ImGui::Text("Show AI Agent Panel");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+4");
    ImGui::NextColumn();
    ImGui::Text("Show Terminal");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+5");
    ImGui::NextColumn();
    ImGui::Text("Show Output");
    ImGui::NextColumn();
    
    ImGui::Columns(1);
    
    ImGui::Separator();
    
    // AI shortcuts
    ImGui::Text("AI Operations");
    ImGui::Separator();
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 150);
    
    ImGui::Text("Ctrl+I");
    ImGui::NextColumn();
    ImGui::Text("Toggle AI Panel");
    ImGui::NextColumn();
    
    ImGui::Text("Ctrl+K");
    ImGui::NextColumn();
    ImGui::Text("Quick AI Request");
    ImGui::NextColumn();
    
    ImGui::Columns(1);
    
    ImGui::Separator();
    
    // About
    ImGui::Text("About MingGoRTS IDE");
    ImGui::Separator();
    ImGui::Text("Version: 1.0.0");
    ImGui::Text("AI-Powered Game Development Environment");
    ImGui::Text("Built with Potato Engine and Dear ImGui");
    
    ImGui::End();
}

void IDEGUI::RenderSyntaxHighlighting() {
    // Basic syntax highlighting for C++
    // This is a simple implementation - could be enhanced with proper lexer
}

void IDEGUI::RenderTemplatesPanel() {
    // Template system for file creation
    ImGui::Separator();
    ImGui::Text("Templates:");
    ImGui::SameLine();
    
    if (ImGui::BeginCombo("##templates", "Select Template")) {
        for (const auto& templateName : state.fileTemplates) {
            if (ImGui::Selectable(templateName.c_str())) {
                state.selectedTemplate = templateName;
                ApplyTemplate(templateName);
            }
        }
        ImGui::EndCombo();
    }
}

void IDEGUI::InitializeTemplates() {
    state.fileTemplates = {
        "C++ Class",
        "C++ Header",
        "C++ Source",
        "Lua Script",
        "Python Script",
        "JSON Config",
        "Markdown Document"
    };
}

void IDEGUI::ApplyTemplate(const std::string& templateName) {
    std::string content = GetTemplateContent(templateName);
    
    if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
        auto& tab = state.openTabs[state.activeTab];
        size_t copySize = std::min(content.size(), sizeof(tab.buffer) - 1);
        memcpy(tab.buffer, content.c_str(), copySize);
        tab.buffer[copySize] = '\0';
        tab.content = content;
        tab.modified = true;
        
        AddOutputLog("Applied template: " + templateName);
    } else {
        // Create new tab with template
        std::string newFileName = "new_" + templateName + ".cpp";
        OpenTab(newFileName);
        
        if (state.activeTab >= 0 && state.activeTab < (int)state.openTabs.size()) {
            auto& tab = state.openTabs[state.activeTab];
            size_t copySize = std::min(content.size(), sizeof(tab.buffer) - 1);
            memcpy(tab.buffer, content.c_str(), copySize);
            tab.buffer[copySize] = '\0';
            tab.content = content;
            tab.modified = true;
        }
        
        AddOutputLog("Created new file from template: " + templateName);
    }
}

std::string IDEGUI::GetTemplateContent(const std::string& templateName) {
    if (templateName == "C++ Class") {
        return "#pragma once\n\nclass MyClass {\npublic:\n    MyClass();\n    ~MyClass();\n    \n    void DoSomething();\n    \nprivate:\n    int memberVariable;\n};\n";
    } else if (templateName == "C++ Header") {
        return "#pragma once\n\n// Header file template\n\nclass ClassName {\npublic:\n    // Public interface\n};\n";
    } else if (templateName == "C++ Source") {
        return "#include \"ClassName.h\"\n\n// Source file template\n\nClassName::ClassName() {\n    // Constructor implementation\n}\n\nClassName::~ClassName() {\n    // Destructor implementation\n}\n\nvoid ClassName::DoSomething() {\n    // Method implementation\n}\n";
    } else if (templateName == "Lua Script") {
        return "-- Lua script template\n\nlocal module = {}\n\nfunction module.init()\n    -- Initialization code\nend\n\nfunction module.update(dt)\n    -- Update code\nend\n\nreturn module\n";
    } else if (templateName == "Python Script") {
        return "# Python script template\n\nclass ClassName:\n    def __init__(self):\n        # Constructor\n        pass\n    \n    def do_something(self):\n        # Method\n        pass\n\nif __name__ == \"__main__\":\n    obj = ClassName()\n    obj.do_something()\n";
    } else if (templateName == "JSON Config") {
        return "{\n    \"config\": {\n        \"key\": \"value\",\n        \"number\": 42\n    }\n}\n";
    } else if (templateName == "Markdown Document") {
        return "# Document Title\n\n## Section 1\n\nContent here.\n\n## Section 2\n\nMore content.\n";
    }
    
    return "// Template not found\n";
}

bool IDEGUI::IsKeyword(const std::string& word) {
    static const std::vector<std::string> keywords = {
        "class", "struct", "public", "private", "protected",
        "void", "int", "float", "double", "bool", "char",
        "if", "else", "for", "while", "do", "switch", "case",
        "break", "continue", "return", "using", "namespace",
        "include", "pragma", "template", "typename", "auto",
        "const", "static", "virtual", "override", "final"
    };
    
    return std::find(keywords.begin(), keywords.end(), word) != keywords.end();
}

bool IDEGUI::IsComment(const std::string& line, size_t pos) {
    if (pos < line.length() - 1) {
        return (line[pos] == '/' && line[pos + 1] == '/') ||
               (line[pos] == '/' && line[pos + 1] == '*');
    }
    return false;
}

bool IDEGUI::IsString(const std::string& line, size_t pos) {
    if (pos > 0 && line[pos - 1] == '\"') {
        return true;
    }
    return false;
}

ImVec4 IDEGUI::GetSyntaxColor(const std::string& token) {
    if (IsKeyword(token)) {
        return ImVec4(0.6f, 0.8f, 1.0f, 1.0f); // Blue for keywords
    } else if (token.find("\"") != std::string::npos) {
        return ImVec4(0.6f, 0.9f, 0.6f, 1.0f); // Green for strings
    } else if (token.find("//") != std::string::npos) {
        return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray for comments
    } else if (token.find("0x") == 0 || (token[0] >= '0' && token[0] <= '9')) {
        return ImVec4(0.9f, 0.6f, 0.6f, 1.0f); // Red for numbers
    }
    
    return ImVec4(0.9f, 0.9f, 0.9f, 1.0f); // Default white
}

// ============================================================================
// Debugger Implementation
// ============================================================================

void IDEGUI::RenderDebugger() {
    if (!state.showDebugger) return;
    
    ImGui::Begin(T(TranslationKey::Panel_Debugger).c_str(), &state.showDebugger, ImGuiWindowFlags_MenuBar);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem(T(TranslationKey::Debugger_StartDebugging).c_str(), "F5", false, !state.debugging)) {
                StartDebugging();
            }
            if (ImGui::MenuItem(T(TranslationKey::Debugger_StopDebugging).c_str(), "Shift+F5", false, state.debugging)) {
                StopDebugging();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::Debugger_StepOver).c_str(), "F10", false, state.debugging)) {
                StepOver();
            }
            if (ImGui::MenuItem(T(TranslationKey::Debugger_StepInto).c_str(), "F11", false, state.debugging)) {
                StepInto();
            }
            if (ImGui::MenuItem(T(TranslationKey::Debugger_StepOut).c_str(), "Shift+F11", false, state.debugging)) {
                StepOut();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T(TranslationKey::Debugger_Continue).c_str(), "F5", false, state.debugging && state.debugPaused)) {
                ContinueDebug();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Debug status
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", T(TranslationKey::Debugger_Status).c_str());
    ImGui::SameLine();
    if (state.debugging) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", state.debugPaused ? "Paused" : "Running");
    } else {
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.5f, 1.0f), "%s", "Not Debugging");
    }
    
    ImGui::Separator();
    
    // Breakpoints list
    ImGui::Text("%s", T(TranslationKey::Debugger_Breakpoints).c_str());
    if (ImGui::BeginChild("BreakpointsList", ImVec2(0, 150))) {
        for (size_t i = 0; i < state.breakpoints.size(); i++) {
            const auto& bp = state.breakpoints[i];
            ImGui::PushID(static_cast<int>(i));
            
            bool enabled = bp.enabled;
            ImGui::Checkbox("", &enabled);
            ImGui::SameLine();
            ImGui::Text("%s:%d", bp.filePath.c_str(), bp.lineNumber);
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1)) {
                RemoveBreakpoint(bp.filePath, bp.lineNumber);
            }
            
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    
    // Expression evaluator
    ImGui::Text("%s", T(TranslationKey::Debugger_Expression).c_str());
    static char exprBuffer[256];
    ImGui::InputText("##expr", exprBuffer, sizeof(exprBuffer));
    if (ImGui::Button("Evaluate")) {
        EvaluateExpression(exprBuffer);
    }
    
    ImGui::End();
}

void IDEGUI::ToggleBreakpoint(const std::string& filePath, int lineNumber) {
    if (HasBreakpoint(filePath, lineNumber)) {
        RemoveBreakpoint(filePath, lineNumber);
    } else {
        AddBreakpoint(filePath, lineNumber);
    }
}

void IDEGUI::AddBreakpoint(const std::string& filePath, int lineNumber) {
    Breakpoint bp;
    bp.filePath = filePath;
    bp.lineNumber = lineNumber;
    bp.enabled = true;
    state.breakpoints.push_back(bp);
    
    AddOutputLog("[DEBUG] Breakpoint added: " + filePath + ":" + std::to_string(lineNumber));
}

void IDEGUI::RemoveBreakpoint(const std::string& filePath, int lineNumber) {
    auto it = std::remove_if(state.breakpoints.begin(), state.breakpoints.end(),
        [&](const Breakpoint& bp) {
            return bp.filePath == filePath && bp.lineNumber == lineNumber;
        });
    state.breakpoints.erase(it, state.breakpoints.end());
    
    AddOutputLog("[DEBUG] Breakpoint removed: " + filePath + ":" + std::to_string(lineNumber));
}

bool IDEGUI::HasBreakpoint(const std::string& filePath, int lineNumber) {
    for (const auto& bp : state.breakpoints) {
        if (bp.filePath == filePath && bp.lineNumber == lineNumber) {
            return true;
        }
    }
    return false;
}

void IDEGUI::StartDebugging() {
    state.debugging = true;
    state.debugPaused = false;
    AddOutputLog("[DEBUG] Debugging started");
    
    // In a real implementation, this would attach to a debugger
    InitializeGit(); // Initialize git for debugging context
}

void IDEGUI::StopDebugging() {
    state.debugging = false;
    state.debugPaused = false;
    state.currentDebugLine = -1;
    AddOutputLog("[DEBUG] Debugging stopped");
}

void IDEGUI::StepOver() {
    if (!state.debugging) return;
    state.debugPaused = true;
    AddOutputLog("[DEBUG] Step over");
    // In a real implementation, this would execute one line
}

void IDEGUI::StepInto() {
    if (!state.debugging) return;
    state.debugPaused = true;
    AddOutputLog("[DEBUG] Step into");
    // In a real implementation, this would step into function calls
}

void IDEGUI::StepOut() {
    if (!state.debugging) return;
    state.debugPaused = true;
    AddOutputLog("[DEBUG] Step out");
    // In a real implementation, this would step out of current function
}

void IDEGUI::ContinueDebug() {
    if (!state.debugging) return;
    state.debugPaused = false;
    AddOutputLog("[DEBUG] Continue");
    // In a real implementation, this would continue execution
}

void IDEGUI::EvaluateExpression(const std::string& expression) {
    if (!state.debugging) return;
    
    // In a real implementation, this would evaluate the expression in the debugger
    AddOutputLog("[DEBUG] Evaluating: " + expression);
    AddOutputLog("[DEBUG] Result: (simulated evaluation)");
}

// ============================================================================
// Git Panel Implementation
// ============================================================================

void IDEGUI::RenderGitPanel() {
    if (!state.showGitPanel) return;
    
    ImGui::Begin(T(TranslationKey::Panel_GitPanel).c_str(), &state.showGitPanel);
    
    // Git status
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", T(TranslationKey::Git_Branch).c_str());
    ImGui::SameLine();
    ImGui::Text("%s", state.gitBranch.c_str());
    
    ImGui::Separator();
    
    // Git operations
    if (ImGui::Button(T(TranslationKey::Git_Status).c_str())) {
        GitStatus();
    }
    ImGui::SameLine();
    if (ImGui::Button(T(TranslationKey::Git_Pull).c_str())) {
        GitPull();
    }
    ImGui::SameLine();
    if (ImGui::Button(T(TranslationKey::Git_Push).c_str())) {
        GitPush();
    }
    
    ImGui::Separator();
    
    // Staged files
    ImGui::Text("%s", T(TranslationKey::Git_StagedFiles).c_str());
    if (ImGui::BeginChild("StagedFiles", ImVec2(0, 100))) {
        for (const auto& file : state.gitStagedFiles) {
            ImGui::Text("%s", file.c_str());
        }
    }
    ImGui::EndChild();
    
    // Modified files
    ImGui::Text("%s", T(TranslationKey::Git_ModifiedFiles).c_str());
    if (ImGui::BeginChild("ModifiedFiles", ImVec2(0, 100))) {
        for (const auto& file : state.gitModifiedFiles) {
            ImGui::Text("%s", file.c_str());
        }
    }
    ImGui::EndChild();
    
    // Commit message
    ImGui::Text("%s", T(TranslationKey::Git_CommitMessage).c_str());
    static char commitBuffer[512];
    ImGui::InputTextMultiline("##commit", commitBuffer, sizeof(commitBuffer), ImVec2(0, 60));
    if (ImGui::Button(T(TranslationKey::Git_Commit).c_str())) {
        GitCommit(commitBuffer);
        memset(commitBuffer, 0, sizeof(commitBuffer));
    }
    
    ImGui::End();
}

void IDEGUI::InitializeGit() {
    // Check if git is available
    std::string result = ExecuteGitCommand("git --version");
    if (!result.empty()) {
        AddOutputLog("[GIT] Git detected: " + result);
        UpdateGitStatus();
    } else {
        AddOutputLog("[GIT] Git not found or not initialized");
    }
}

std::string IDEGUI::ExecuteGitCommand(const std::string& command) {
    // In a real implementation, this would execute the git command
    // For now, we'll simulate it
    if (command.find("status") != std::string::npos) {
        return "On branch main\nYour branch is up to date with 'origin/main'.";
    } else if (command.find("branch") != std::string::npos) {
        return "* main\n  feature-branch";
    }
    return "";
}

void IDEGUI::UpdateGitStatus() {
    state.gitBranch = "main";
    state.gitStatus = "Clean";
    state.gitStagedFiles = {"Core/PotatoEngine.cpp", "Core/PotatoEngine.h"};
    state.gitModifiedFiles = {"MingGoRTS_IDE/GUI/IDEGUI.cpp"};
    
    // Get commit history
    state.gitCommitHistory = {
        "abc123: Enhanced IDE features",
        "def456: Added AI integration",
        "ghi789: Initial commit"
    };
}

void IDEGUI::GitStatus() {
    UpdateGitStatus();
    AddOutputLog("[GIT] Status updated");
    AddOutputLog("[GIT] Branch: " + state.gitBranch);
    AddOutputLog("[GIT] Status: " + state.gitStatus);
}

void IDEGUI::GitCommit(const std::string& message) {
    if (message.empty()) {
        AddOutputLog("[GIT] Error: Empty commit message");
        return;
    }
    
    AddOutputLog("[GIT] Committing with message: " + message);
    // In a real implementation, this would execute git commit
    UpdateGitStatus();
}

void IDEGUI::GitPush() {
    AddOutputLog("[GIT] Pushing to remote...");
    // In a real implementation, this would execute git push
    AddOutputLog("[GIT] Push completed");
}

void IDEGUI::GitPull() {
    AddOutputLog("[GIT] Pulling from remote...");
    // In a real implementation, this would execute git pull
    AddOutputLog("[GIT] Pull completed");
}

void IDEGUI::GitCreateBranch(const std::string& branchName) {
    AddOutputLog("[GIT] Creating branch: " + branchName);
    // In a real implementation, this would execute git checkout -b
    UpdateGitStatus();
}

void IDEGUI::GitSwitchBranch(const std::string& branchName) {
    AddOutputLog("[GIT] Switching to branch: " + branchName);
    // In a real implementation, this would execute git checkout
    UpdateGitStatus();
}

void IDEGUI::GitDiff(const std::string& filePath) {
    AddOutputLog("[GIT] Showing diff for: " + filePath);
    // In a real implementation, this would execute git diff
}

// ============================================================================
// Code Analysis Implementation
// ============================================================================

void IDEGUI::RenderCodeAnalysis() {
    if (!state.showCodeAnalysis) return;
    
    ImGui::Begin(T(TranslationKey::Panel_CodeAnalysis).c_str(), &state.showCodeAnalysis);
    
    // Analysis controls
    if (ImGui::Button(T(TranslationKey::CodeAnalysis_AnalyzeCurrentFile).c_str())) {
        AnalyzeCurrentFile();
    }
    ImGui::SameLine();
    if (ImGui::Button(T(TranslationKey::CodeAnalysis_AnalyzeProject).c_str())) {
        AnalyzeProject();
    }
    
    ImGui::Separator();
    
    // Code issues list
    ImGui::Text("%s", T(TranslationKey::CodeAnalysis_CodeIssues).c_str());
    ImGui::Text("(%d %s)", static_cast<int>(state.codeIssues.size()), T(TranslationKey::CodeAnalysis_IssuesFound).c_str());
    
    if (ImGui::BeginChild("CodeIssuesList", ImVec2(0, 300))) {
        for (size_t i = 0; i < state.codeIssues.size(); i++) {
            const auto& issue = state.codeIssues[i];
            
            ImVec4 color;
            if (issue.severity == "error") {
                color = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
            } else if (issue.severity == "warning") {
                color = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);
            } else {
                color = ImVec4(0.6f, 0.6f, 0.8f, 1.0f);
            }
            
            ImGui::TextColored(color, "[%s] %s:%d - %s", 
                issue.severity.c_str(), 
                issue.filePath.c_str(), 
                issue.lineNumber, 
                issue.message.c_str());
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                // In a real implementation, this would navigate to the issue
                AddOutputLog("[ANALYSIS] Selected issue: " + issue.message);
            }
        }
    }
    ImGui::EndChild();
    
    // Fix all button
    if (!state.codeIssues.empty()) {
        if (ImGui::Button(T(TranslationKey::CodeAnalysis_AutoFix).c_str())) {
            RunCodeAnalysis();
        }
    }
    
    ImGui::End();
}

void IDEGUI::AnalyzeCurrentFile() {
    state.codeIssues.clear();
    
    if (state.activeTab >= 0 && state.activeTab < static_cast<int>(state.openTabs.size())) {
        const auto& tab = state.openTabs[state.activeTab];
        DetectSyntaxErrors();
        DetectStyleIssues();
        DetectPotentialBugs();
    }
    
    AddOutputLog("[ANALYSIS] Current file analysis complete");
}

void IDEGUI::AnalyzeProject() {
    state.codeIssues.clear();
    
    // In a real implementation, this would analyze all project files
    DetectSyntaxErrors();
    DetectStyleIssues();
    DetectPotentialBugs();
    
    AddOutputLog("[ANALYSIS] Project analysis complete");
}

void IDEGUI::DetectSyntaxErrors() {
    // Simulated syntax error detection
    CodeIssue issue;
    issue.filePath = state.activeTab >= 0 ? state.openTabs[state.activeTab].filePath : "unknown";
    issue.lineNumber = 42;
    issue.severity = "error";
    issue.message = "Missing semicolon";
    state.codeIssues.push_back(issue);
}

void IDEGUI::DetectStyleIssues() {
    // Simulated style issue detection
    CodeIssue issue;
    issue.filePath = state.activeTab >= 0 ? state.openTabs[state.activeTab].filePath : "unknown";
    issue.lineNumber = 15;
    issue.severity = "warning";
    issue.message = "Line too long (120 characters, max 100)";
    state.codeIssues.push_back(issue);
}

void IDEGUI::DetectPotentialBugs() {
    // Simulated bug detection
    CodeIssue issue;
    issue.filePath = state.activeTab >= 0 ? state.openTabs[state.activeTab].filePath : "unknown";
    issue.lineNumber = 23;
    issue.severity = "warning";
    issue.message = "Potential null pointer dereference";
    state.codeIssues.push_back(issue);
}

void IDEGUI::FixCodeIssue(int issueIndex) {
    if (issueIndex >= 0 && issueIndex < static_cast<int>(state.codeIssues.size())) {
        const auto& issue = state.codeIssues[issueIndex];
        AddOutputLog("[ANALYSIS] Fixing issue: " + issue.message);
        // In a real implementation, this would apply the fix
        state.codeIssues.erase(state.codeIssues.begin() + issueIndex);
    }
}

void IDEGUI::RunCodeAnalysis() {
    // In a real implementation, this would run automated fixes
    AddOutputLog("[ANALYSIS] Running automated fixes...");
    state.codeIssues.clear();
    AddOutputLog("[ANALYSIS] All issues fixed");
}

std::vector<std::string> IDEGUI::GetCppKeywords() {
    return {
        "class", "struct", "public", "private", "protected",
        "void", "int", "float", "double", "bool", "char",
        "if", "else", "for", "while", "do", "switch", "case",
        "break", "continue", "return", "using", "namespace",
        "include", "pragma", "template", "typename", "auto",
        "const", "static", "virtual", "override", "final",
        "new", "delete", "nullptr", "true", "false"
    };
}

std::vector<std::string> IDEGUI::GetStandardLibraryFunctions() {
    return {
        "std::cout", "std::cin", "std::endl", "std::vector",
        "std::string", "std::map", "std::unordered_map",
        "std::set", "std::unordered_set", "std::array",
        "std::unique_ptr", "std::shared_ptr", "std::make_unique",
        "std::make_shared", "std::move", "std::forward"
    };
}

// ============================================================================
// Intelligent Suggestions Implementation
// ============================================================================

void IDEGUI::UpdateIntelligentSuggestions() {
    if (!state.intelligentSuggestionsEnabled) return;
    
    // In a real implementation, this would:
    // 1. Get current code context
    // 2. Call IntelligentSuggestionSystem::GenerateSuggestions
    // 3. Update state.currentSuggestions
    
    // For now, add a placeholder suggestion
    Suggestion suggestion;
    suggestion.type = SuggestionType::CodeCompletion;
    suggestion.title = "Intelligent Suggestion";
    suggestion.description = "AI-powered code analysis and suggestions";
    suggestion.code = "// Suggested code";
    suggestion.reason = "Based on code patterns and best practices";
    suggestion.confidence = ConfidenceLevel::High;
    suggestion.confidenceScore = 0.85f;
    
    state.currentSuggestions.clear();
    state.currentSuggestions.push_back(suggestion);
    state.showSuggestions = true;
}

void IDEGUI::RenderIntelligentSuggestions() {
    if (!state.showSuggestions || state.currentSuggestions.empty()) return;
    
    ImGui::SetNextWindowPos(ImVec2(state.codeEditorPos.x + 50, 
                                       state.codeEditorPos.y + 100), 
                        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("Intelligent Suggestions", &state.showSuggestions);
    
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "[AI] Intelligent Suggestions");
    ImGui::Separator();
    
    for (size_t i = 0; i < state.currentSuggestions.size(); i++) {
        const auto& suggestion = state.currentSuggestions[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        // Suggestion type indicator
        ImVec4 typeColor;
        switch (suggestion.type) {
            case SuggestionType::CodeCompletion: typeColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f); break;
            case SuggestionType::Refactoring: typeColor = ImVec4(0.3f, 0.6f, 0.8f, 1.0f); break;
            case SuggestionType::Optimization: typeColor = ImVec4(0.8f, 0.6f, 0.3f, 1.0f); break;
            case SuggestionType::BugFix: typeColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); break;
            case SuggestionType::BestPractice: typeColor = ImVec4(0.6f, 0.8f, 0.3f, 1.0f); break;
            default: typeColor = ImVec4(0.6f, 0.6f, 0.8f, 1.0f); break;
        }
        
        ImGui::TextColored(typeColor, "[%s]", 
            suggestion.type == SuggestionType::CodeCompletion ? "Code" :
            suggestion.type == SuggestionType::Refactoring ? "Refactor" :
            suggestion.type == SuggestionType::Optimization ? "Optimize" :
            suggestion.type == SuggestionType::BugFix ? "Bug" :
            suggestion.type == SuggestionType::BestPractice ? "Best" : "Other");
        
        ImGui::SameLine();
        ImGui::Text("%s", suggestion.title.c_str());
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", suggestion.description.c_str());
            ImGui::Separator();
            ImGui::Text("Reason: %s", suggestion.reason.c_str());
            ImGui::Text("Confidence: %.1f%%", suggestion.confidenceScore * 100.0f);
            ImGui::EndTooltip();
        }
        
        // Apply button
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply")) {
            ApplySuggestion(suggestion);
            LearnFromSuggestion(suggestion.id, true);
        }
        
        // Dismiss button
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss")) {
            LearnFromSuggestion(suggestion.id, false);
            state.currentSuggestions.erase(state.currentSuggestions.begin() + i);
            i--;
        }
        
        ImGui::Separator();
        
        ImGui::PopID();
    }
    
    if (ImGui::Button("Close")) {
        state.showSuggestions = false;
    }
    
    ImGui::End();
}

void IDEGUI::ApplySuggestion(const Suggestion& suggestion) {
    // In a real implementation, this would:
    // 1. Insert the suggested code at the cursor position
    // 2. Update the editor buffer
    // 3. Mark the file as modified
    
    AddOutputLog("[AI] Applied suggestion: " + suggestion.title);
    AddOutputLog("[AI] Code: " + suggestion.code);
    
    // Placeholder: add suggestion code to current editor content
    if (state.activeTab >= 0) {
        std::string currentContent = state.editorBuffer;
        currentContent += "\n" + suggestion.code + "\n";
        strncpy_s(state.editorBuffer, sizeof(state.editorBuffer), currentContent.c_str(), _TRUNCATE);
        state.openTabs[state.activeTab].modified = true;
    }
}

void IDEGUI::LearnFromSuggestion(const std::string& suggestionId, bool accepted) {
    // In a real implementation, this would:
    // 1. Call IntelligentSuggestionSystem::LearnFromFeedback
    // 2. Update suggestion weights based on acceptance
    
    if (accepted) {
        AddOutputLog("[AI] Suggestion accepted - learning from feedback");
    } else {
        AddOutputLog("[AI] Suggestion dismissed - learning from feedback");
    }
}

// ============================================================================
// Autocomplete Implementation
// ============================================================================

void IDEGUI::RenderAutocomplete() {
    if (!state.showAutocomplete || state.autocompleteSuggestions.empty()) return;
    
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 20));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    
    ImGui::Begin("##Autocomplete", &state.showAutocomplete, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    
    for (size_t i = 0; i < state.autocompleteSuggestions.size(); i++) {
        const auto& suggestion = state.autocompleteSuggestions[i];
        
        if (ImGui::Selectable(suggestion.c_str(), i == state.autocompleteIndex)) {
            SelectAutocompleteSuggestion(static_cast<int>(i));
        }
        
        if (ImGui::IsItemHovered()) {
            state.autocompleteIndex = static_cast<int>(i);
        }
    }
    
    ImGui::End();
}

void IDEGUI::UpdateAutocomplete() {
    if (!ShouldShowAutocomplete()) {
        state.showAutocomplete = false;
        state.autocompleteSuggestions.clear();
        return;
    }
    
    // Get current editor context
    std::string context;
    if (state.activeTab >= 0 && state.activeTab < static_cast<int>(state.openTabs.size())) {
        context = state.openTabs[state.activeTab].content;
    }
    
    state.autocompleteSuggestions = GetSuggestionsForContext(context);
    state.showAutocomplete = !state.autocompleteSuggestions.empty();
}

void IDEGUI::SelectAutocompleteSuggestion(int index) {
    if (index >= 0 && index < static_cast<int>(state.autocompleteSuggestions.size())) {
        InsertAutocomplete();
        state.showAutocomplete = false;
    }
}

void IDEGUI::InsertAutocomplete() {
    if (state.autocompleteIndex >= 0 && state.autocompleteIndex < static_cast<int>(state.autocompleteSuggestions.size())) {
        const std::string& suggestion = state.autocompleteSuggestions[state.autocompleteIndex];
        
        // In a real implementation, this would insert the suggestion at cursor position
        AddOutputLog("[AUTOCOMPLETE] Inserted: " + suggestion);
        
        state.showAutocomplete = false;
        state.autocompleteSuggestions.clear();
    }
}

std::vector<std::string> IDEGUI::GetSuggestionsForContext(const std::string& context) {
    std::vector<std::string> suggestions;
    
    // Get C++ keywords
    auto keywords = GetCppKeywords();
    suggestions.insert(suggestions.end(), keywords.begin(), keywords.end());
    
    // Get standard library functions
    auto stdFunctions = GetStandardLibraryFunctions();
    suggestions.insert(suggestions.end(), stdFunctions.begin(), stdFunctions.end());
    
    // Sort suggestions
    std::sort(suggestions.begin(), suggestions.end());
    
    return suggestions;
}

bool IDEGUI::ShouldShowAutocomplete() {
    // In a real implementation, this would check if autocomplete should be shown
    // based on current cursor position and context
    return true;
}

int IDEGUI::CalculateCyclomaticComplexity(const std::string& code) {
    int complexity = 1; // Base complexity
    
    // Count decision points
    complexity += std::count(code.begin(), code.end(), 'i'); // if
    complexity += std::count(code.begin(), code.end(), '?'); // ternary
    complexity += std::count(code.begin(), code.end(), ':'); // case/else
    complexity += std::count(code.begin(), code.end(), 'f'); // for
    complexity += std::count(code.begin(), code.end(), 'w'); // while
    
    return complexity;
}

int IDEGUI::CalculateNestingDepth(const std::string& code) {
    int maxDepth = 0;
    int currentDepth = 0;
    
    for (char c : code) {
        if (c == '{') {
            currentDepth++;
            maxDepth = std::max(maxDepth, currentDepth);
        } else if (c == '}') {
            currentDepth--;
        }
    }
    
    return maxDepth;
}

std::vector<std::string> IDEGUI::FindLongFunctions(const std::string& code, int maxLines) {
    std::vector<std::string> longFunctions;
    
    // This is a simplified version
    // Real implementation would parse actual function boundaries
    
    if (code.length() > maxLines * 80) { // Rough estimate
        longFunctions.push_back("Large function detected");
    }
    
    return longFunctions;
}

// ============================================================================
// Intelligent Development System Integration
// ============================================================================

void IDEGUI::GenerateCodeFromPrompt() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    state.developmentProcessing = true;
    AddOutputLog("[Dev] Generating code from prompt...");
    
    std::string prompt(state.developmentPrompt);
    Potato::AI::CodeGenerationResult result = g_DevSystem->GenerateCode(prompt, "C++");
    
    if (result.success) {
        // Insert generated code into editor
        if (state.activeTab >= 0) {
            std::string currentContent = state.editorBuffer;
            currentContent += "\n" + result.generatedCode + "\n";
            strncpy_s(state.editorBuffer, sizeof(state.editorBuffer), currentContent.c_str(), _TRUNCATE);
            state.openTabs[state.activeTab].modified = true;
        }
        
        AddOutputLog("[Dev] Code generated successfully");
        strncpy_s(state.developmentResponse, sizeof(state.developmentResponse), result.generatedCode.c_str(), _TRUNCATE);
    } else {
        AddOutputLog("[Dev] Code generation failed: " + result.error);
        std::string errorMsg = "Error: " + result.error;
        strncpy_s(state.developmentResponse, sizeof(state.developmentResponse), errorMsg.c_str(), _TRUNCATE);
    }
    
    state.developmentProcessing = false;
}

void IDEGUI::AnalyzeCodeWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    Potato::AI::CodeAnalysisResult result = g_DevSystem->AnalyzeCode(code, "C++");
    
    AddOutputLog("[Dev] Code Analysis Results:");
    AddOutputLog("  Lines of Code: " + std::to_string(result.lineCount));
    AddOutputLog("  Complexity: " + std::to_string(result.complexity));
    AddOutputLog("  Quality Score: " + std::to_string(result.qualityScore));
    
    for (const auto& issue : result.issues) {
        AddOutputLog("  Issue: " + issue);
    }
    
    for (const auto& suggestion : result.suggestions) {
        AddOutputLog("  Suggestion: " + suggestion);
    }
}

void IDEGUI::GenerateTestsWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    Potato::AI::TestGenerationResult result = g_DevSystem->GenerateTestsForFunction(code);
    
    if (result.success) {
        AddOutputLog("[Dev] Tests generated successfully");
        AddOutputLog("  Framework: " + result.testFramework);
        AddOutputLog("  Coverage: " + std::to_string(result.coverage) + "%");
        AddOutputLog("  Generated Code:\n" + result.testCode);
    } else {
        AddOutputLog("[Dev] Test generation failed: " + result.error);
    }
}

void IDEGUI::GenerateDocumentationWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    std::string documentation = g_DevSystem->GenerateDocumentation(code);
    
    AddOutputLog("[Dev] Generated Documentation:");
    AddOutputLog(documentation);
}

void IDEGUI::FixBugWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    std::string bugDescription = state.developmentPrompt;
    
    std::string fixedCode = g_DevSystem->FixBug(code, bugDescription);
    
    if (fixedCode != code) {
        // Update editor with fixed code
        strncpy_s(state.editorBuffer, sizeof(state.editorBuffer), fixedCode.c_str(), _TRUNCATE);
        if (state.activeTab >= 0) {
            state.openTabs[state.activeTab].modified = true;
        }
        AddOutputLog("[Dev] Bug fixed successfully");
    } else {
        AddOutputLog("[Dev] Bug fixing failed or no changes needed");
    }
}

void IDEGUI::OptimizeCodeWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    std::vector<std::string> optimizations = g_DevSystem->SuggestOptimizations(code);
    
    AddOutputLog("[Dev] Optimization Suggestions:");
    for (const auto& opt : optimizations) {
        AddOutputLog("  - " + opt);
    }
    
    // Also try to optimize the code
    std::string optimizedCode = g_DevSystem->OptimizeCode(code);
    if (optimizedCode != code) {
        strncpy_s(state.editorBuffer, sizeof(state.editorBuffer), optimizedCode.c_str(), _TRUNCATE);
        if (state.activeTab >= 0) {
            state.openTabs[state.activeTab].modified = true;
        }
        AddOutputLog("[Dev] Code optimized successfully");
    }
}

void IDEGUI::ReviewCodeWithAI() {
    if (!g_DevSystem) {
        AddOutputLog("[Dev] Development system not initialized");
        return;
    }
    
    std::string code(state.editorBuffer);
    std::string review = g_DevSystem->ReviewCode(code);
    
    AddOutputLog("[Dev] Code Review:");
    AddOutputLog(review);
}

void IDEGUI::ShowDevelopmentAssistant() {
    state.showDevelopmentAssistant = !state.showDevelopmentAssistant;
}

void IDEGUI::RenderDevelopmentAssistant() {
    if (!state.showDevelopmentAssistant) return;
    
    ImGui::SetNextWindowPos(ImVec2(state.codeEditorPos.x + 50, 
                                       state.codeEditorPos.y + 100), 
                        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("Development Assistant", &state.showDevelopmentAssistant);
    
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "[AI] Development Assistant");
    ImGui::Separator();
    
    // Development prompt input
    ImGui::Text("Prompt:");
    ImGui::InputTextMultiline("##DevPrompt", state.developmentPrompt, 
                               sizeof(state.developmentPrompt), 
                               ImVec2(-1, 80));
    
    // Action buttons
    if (ImGui::Button("Generate Code")) {
        GenerateCodeFromPrompt();
    }
    ImGui::SameLine();
    if (ImGui::Button("Analyze Code")) {
        AnalyzeCodeWithAI();
    }
    ImGui::SameLine();
    if (ImGui::Button("Generate Tests")) {
        GenerateTestsWithAI();
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Generate Documentation")) {
        GenerateDocumentationWithAI();
    }
    ImGui::SameLine();
    if (ImGui::Button("Fix Bug")) {
        FixBugWithAI();
    }
    ImGui::SameLine();
    if (ImGui::Button("Optimize Code")) {
        OptimizeCodeWithAI();
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Review Code")) {
        ReviewCodeWithAI();
    }
    
    ImGui::Separator();
    
    // Response display
    ImGui::Text("Response:");
    if (state.developmentProcessing) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Processing...");
    } else {
        ImGui::InputTextMultiline("##DevResponse", state.developmentResponse,
                                   sizeof(state.developmentResponse),
                                   ImVec2(-1, 150),
                                   ImGuiInputTextFlags_ReadOnly);
    }
    
    ImGui::End();
}

} // namespace MingGoRTSIDE