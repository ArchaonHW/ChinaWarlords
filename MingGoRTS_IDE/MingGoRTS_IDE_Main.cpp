/**
 * MingGoRTS IDE Main Application
 * Demonstrates AI-powered game development IDE
 */

#include "IDECore.h"
#include "AIIntegration.h"
#include "WorkspacePath.h"
#include <iostream>
#include <vector>
#include <string>

using namespace MingGoRTSIDE;

int main(int argc, char** argv) {
    std::cout << "=== MingGoRTS IDE - AI-Powered Game Development Environment ===" << std::endl;
    std::cout << "Powered by Potato Engine AI Agent System" << std::endl;
    std::cout << std::endl;
    
    // Configure IDE
    IDEConfig config;
    // --workspace <dir> / MINGGORTS_WORKSPACE / exe-relative repo root
    config.workspacePath = ResolveWorkspacePath(argc, argv);
    config.projectName = "ChinaWarlords (民國史詩)";
    config.enginePath = config.workspacePath;
    config.enableAIAssistance = true;
    config.enableAutocompletion = true;
    config.enableMultiAgent = true;
    
    // Initialize IDE Core
    IDECore ideCore;
    if (!ideCore.Initialize(config)) {
        std::cerr << "Failed to initialize IDE" << std::endl;
        return 1;
    }
    
    std::cout << "\n--- IDE Components Initialized ---" << std::endl;
    std::cout << "Workspace: " << config.workspacePath << std::endl;
    std::cout << "Project: " << config.projectName << std::endl;
    std::cout << "AI Assistance: " << (config.enableAIAssistance ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Multi-Agent: " << (config.enableMultiAgent ? "Enabled" : "Disabled") << std::endl;
    
    // Create/Open project
    std::cout << "\n--- Project Management ---" << std::endl;
    IDEProject* project = ideCore.GetProject();
    if (project) {
        project->OpenProject(config.workspacePath);
        std::cout << "Project opened: " << project->GetProjectName() << std::endl;
    }
    
    // File operations
    std::cout << "\n--- File Operations ---" << std::endl;
    IDEEditor* editor = ideCore.GetEditor();
    if (editor) {
        editor->OpenFile("MainGame.cpp");
        editor->OpenFile("GameEngine.h");
        editor->OpenFile("PlayerController.cpp");
        
        std::cout << "Open files: " << editor->GetOpenFiles().size() << std::endl;
        for (const auto& file : editor->GetOpenFiles()) {
            std::cout << "  - " << file << std::endl;
        }
    }
    
    // AI Agent interactions
    std::cout << "\n--- AI Agent Interactions ---" << std::endl;
    AIAgentInterface* aiInterface = ideCore.GetAIInterface();
    if (aiInterface && aiInterface->IsAvailable()) {
        std::cout << "AI Assistant is available" << std::endl;
        std::cout << "Active agents: " << aiInterface->GetActiveAgentCount() << std::endl;
        
        // Get agent manager
        IDEAgentManager* agentManager = aiInterface->GetAgentManager();
        if (agentManager) {
            // Create specialized game development agents
            std::cout << "\nCreating specialized game development agents..." << std::endl;
            
            agentManager->CreateAgent(GameDevAgentType::EngineCode, "EngineCodeAgent");
            agentManager->CreateAgent(GameDevAgentType::AssetAgent, "AssetAgent");
            agentManager->CreateAgent(GameDevAgentType::LevelDesign, "LevelDesignAgent");
            agentManager->CreateAgent(GameDevAgentType::Performance, "PerformanceAgent");
            agentManager->CreateAgent(GameDevAgentType::BuildAgent, "BuildAgent");
            
            std::cout << "Total agents: " << agentManager->GetAllAgents().size() << std::endl;
            
            // Demonstrate agent-specific capabilities
            std::cout << "\n--- Agent-Specific Capabilities ---" << std::endl;

            const SimplifiedAI::Context ctx = agentManager->GetGlobalContext();
            std::string codeGen = agentManager->GenerateCode("Player movement system", GameDevAgentType::EngineCode, ctx);
            std::cout << "Code Generation: " << codeGen << std::endl;

            std::string assetOpt = agentManager->OptimizeAsset("texture_player.png", ctx);
            std::cout << "Asset Optimization: " << assetOpt << std::endl;

            std::string levelDesign = agentManager->DesignLevel("Forest level with enemy encounters", ctx);
            std::cout << "Level Design: " << levelDesign << std::endl;

            std::string perfAnalysis = agentManager->AnalyzePerformance("Render loop optimization", ctx);
            std::cout << "Performance Analysis: " << perfAnalysis << std::endl;

            std::string buildOpt = agentManager->OptimizeBuild("CMake Release configuration", ctx);
            std::cout << "Build Optimization: " << buildOpt << std::endl;
            
            // Show collaboration status
            std::cout << "\n--- Collaboration Status ---" << std::endl;
            std::cout << agentManager->GetCollaborationStatus() << std::endl;
        }
        
        // Demonstrate AI capabilities
        std::string codeRequest = "Generate a player controller class";
        std::string codeResponse = aiInterface->ProcessCodeRequest(codeRequest, "Game Development");
        std::cout << "Code request: " << codeRequest << std::endl;
        std::cout << "AI response: " << codeResponse << std::endl;
        
        std::string buildRequest = "Optimize CMake configuration for performance";
        std::string buildResponse = aiInterface->ProcessBuildRequest(buildRequest);
        std::cout << "Build request: " << buildRequest << std::endl;
        std::cout << "AI response: " << buildResponse << std::endl;
    }
    
    // Build system
    std::cout << "\n--- Build System ---" << std::endl;
    IDEBuilder* builder = ideCore.GetBuilder();
    if (builder) {
        builder->ConfigureBuildSystem("CMake");
        std::cout << "Build system configured: CMake" << std::endl;
        
        builder->BuildProject("Release");
        std::cout << "Build status: " << builder->GetBuildStatus() << std::endl;
        
        std::vector<std::string> warnings = builder->GetBuildWarnings();
        if (!warnings.empty()) {
            std::cout << "Build warnings: " << warnings.size() << std::endl;
            for (const auto& warning : warnings) {
                std::cout << "  - " << warning << std::endl;
            }
        }
    }
    
    // File navigation
    std::cout << "\n--- File Navigation ---" << std::endl;
    IDENavigator* navigator = ideCore.GetNavigator();
    if (navigator) {
        navigator->SetRootPath(config.workspacePath);
        std::cout << "Root path set: " << navigator->GetRootPath() << std::endl;
        
        std::vector<std::string> contents = navigator->GetDirectoryContents(".");
        std::cout << "Directory contents (" << contents.size() << " items):" << std::endl;
        for (const auto& item : contents) {
            std::cout << "  - " << item << std::endl;
        }
        
        std::vector<std::string> searchResults = navigator->SearchFiles("*.cpp");
        std::cout << "Search results for *.cpp: " << searchResults.size() << " files" << std::endl;
        for (const auto& result : searchResults) {
            std::cout << "  - " << result << std::endl;
        }
    }
    
    // Demonstrate IDE features
    std::cout << "\n=== MingGoRTS IDE Features ===" << std::endl;
    std::cout << "1. AI-Powered Code Assistance" << std::endl;
    std::cout << "2. Multi-Agent Game Development" << std::endl;
    std::cout << "3. Game Engine Integration" << std::endl;
    std::cout << "4. Asset Pipeline Management" << std::endl;
    std::cout << "5. Build System Integration" << std::endl;
    std::cout << "6. Project Management" << std::endl;
    std::cout << "7. File Navigation" << std::endl;
    std::cout << "8. Real-time Collaboration" << std::endl;
    
    std::cout << "\n=== IDE Components ===" << std::endl;
    std::cout << "Potato Engine AI Agent System Integration" << std::endl;
    std::cout << "Specialized Game Development Agents:" << std::endl;
    std::cout << "  - Engine Code Agent" << std::endl;
    std::cout << "  - Asset Agent" << std::endl;
    std::cout << "  - Level Design Agent" << std::endl;
    std::cout << "  - Performance Agent" << std::endl;
    std::cout << "  - Build Agent" << std::endl;
    
    std::cout << "\n=== MingGoRTS IDE Startup Complete ===" << std::endl;
    std::cout << "Ready for AI-powered game development!" << std::endl;
    
    // Run IDE (in real implementation, this would start the GUI)
    // ideCore.Run();
    
    // For demo purposes, we'll just shut down
    ideCore.Shutdown();
    
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
