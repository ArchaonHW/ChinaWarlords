/**
 * MingGoRTS IDE Core Framework
 * Basic IDE structure for game development with AI Agent integration
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace MingGoRTSIDE {

// Forward declarations
class IDEAgentManager;
class IDEContextManager;

// Forward declare GameDevAgentType to avoid circular dependency
enum class GameDevAgentType {
    EngineCode,
    AssetAgent,
    LevelDesign,
    Performance,
    BuildAgent,
    GameDesigner,
    GameTester,
    GameArchitect
};

// Basic IDE components
class IDEProject;
class IDEEditor;
class IDENavigator;
class IDEBuilder;
class AIAgentInterface;

/**
 * IDE Configuration
 */
struct IDEConfig {
    std::string workspacePath;
    std::string projectName;
    std::string enginePath;
    bool enableAIAssistance;
    bool enableAutocompletion;
    bool enableMultiAgent;
    
    IDEConfig()
        : enableAIAssistance(true)
        , enableAutocompletion(true)
        , enableMultiAgent(true) {
    }
};

/**
 * IDE Event Types
 */
enum class IDEEventType {
    ProjectOpened,
    ProjectClosed,
    FileOpened,
    FileSaved,
    FileModified,
    BuildStarted,
    BuildCompleted,
    BuildFailed,
    AIRequest,
    AIResponse,
    Error,
    Warning,
    Info
};

/**
 * IDE Event Data
 */
struct IDEEvent {
    IDEEventType type;
    std::string source;
    std::string message;
    std::unordered_map<std::string, std::string> data;
    uint64_t timestamp;
    
    IDEEvent()
        : type(IDEEventType::Info)
        , timestamp(0) {
    }
};

/**
 * IDE Event Handler
 */
using IDEEventHandler = std::function<void(const IDEEvent&)>;

/**
 * IDE Core Framework
 */
class IDECore {
public:
    IDECore();
    ~IDECore();
    
    bool Initialize(const IDEConfig& config);
    void Shutdown();
    
    void Run();
    void Stop();
    
    // Event System
    void RegisterEventHandler(IDEEventType type, IDEEventHandler handler);
    void EmitEvent(const IDEEvent& event);
    
    // Component Access
    IDEProject* GetProject() const { return project.get(); }
    IDEEditor* GetEditor() const { return editor.get(); }
    IDENavigator* GetNavigator() const { return navigator.get(); }
    IDEBuilder* GetBuilder() const { return builder.get(); }
    AIAgentInterface* GetAIInterface() const { return aiInterface.get(); }
    
    // State
    bool IsRunning() const { return running; }
    const IDEConfig& GetConfig() const { return config; }
    
private:
    IDEConfig config;
    bool running;
    
    std::unique_ptr<IDEProject> project;
    std::unique_ptr<IDEEditor> editor;
    std::unique_ptr<IDENavigator> navigator;
    std::unique_ptr<IDEBuilder> builder;
    std::unique_ptr<AIAgentInterface> aiInterface;
    
    std::unordered_map<IDEEventType, std::vector<IDEEventHandler>> eventHandlers;
};

/**
 * IDE Project Manager
 */
class IDEProject {
public:
    IDEProject();
    ~IDEProject();
    
    bool CreateProject(const std::string& path, const std::string& name);
    bool OpenProject(const std::string& path);
    bool CloseProject();
    
    std::string GetProjectPath() const { return projectPath; }
    std::string GetProjectName() const { return projectName; }
    bool IsOpen() const { return isOpen; }
    
    std::vector<std::string> GetProjectFiles() const;
    bool AddFile(const std::string& filePath);
    bool RemoveFile(const std::string& filePath);
    
private:
    std::string projectPath;
    std::string projectName;
    bool isOpen;
    std::vector<std::string> projectFiles;
};

/**
 * IDE Editor
 */
class IDEEditor {
public:
    IDEEditor();
    ~IDEEditor();
    
    bool OpenFile(const std::string& filePath);
    bool SaveFile(const std::string& filePath);
    bool CloseFile(const std::string& filePath);
    
    std::string GetCurrentFile() const { return currentFile; }
    std::string GetFileContent(const std::string& filePath) const;
    bool SetFileContent(const std::string& filePath, const std::string& content);
    
    std::vector<std::string> GetOpenFiles() const;
    
private:
    std::string currentFile;
    std::vector<std::string> openFiles;
    std::unordered_map<std::string, std::string> fileContents;
};

/**
 * IDE Navigator (File Explorer)
 */
class IDENavigator {
public:
    IDENavigator();
    ~IDENavigator();
    
    bool SetRootPath(const std::string& path);
    std::string GetRootPath() const { return rootPath; }
    
    std::vector<std::string> GetDirectoryContents(const std::string& path) const;
    bool CreateDirectory(const std::string& path);
    bool DeleteDirectory(const std::string& path);
    
    std::vector<std::string> SearchFiles(const std::string& pattern) const;
    
private:
    std::string rootPath;
};

/**
 * IDE Builder
 */
class IDEBuilder {
public:
    IDEBuilder();
    ~IDEBuilder();
    
    bool ConfigureBuildSystem(const std::string& buildType);
    bool BuildProject(const std::string& configuration);
    bool CleanProject();
    
    std::string GetBuildStatus() const { return buildStatus; }
    std::vector<std::string> GetBuildErrors() const { return buildErrors; }
    std::vector<std::string> GetBuildWarnings() const { return buildWarnings; }
    
private:
    std::string buildType;
    std::string buildStatus;
    std::vector<std::string> buildErrors;
    std::vector<std::string> buildWarnings;
};

/**
 * AI Agent Interface
 */
class AIAgentInterface {
public:
    AIAgentInterface();
    ~AIAgentInterface();
    
    bool Initialize();
    void Shutdown();
    
    std::string ProcessRequest(const std::string& request);
    std::string ProcessCodeRequest(const std::string& code, const std::string& context);
    std::string ProcessBuildRequest(const std::string& buildConfig);
    
    bool IsAvailable() const { return available; }
    size_t GetActiveAgentCount() const;
    
    IDEAgentManager* GetAgentManager() const { return agentManager.get(); }
    IDEContextManager* GetContextManager() const { return contextManager.get(); }
    
private:
    bool available;
    std::unique_ptr<IDEAgentManager> agentManager;
    std::unique_ptr<IDEContextManager> contextManager;
};

} // namespace MingGoRTSIDE