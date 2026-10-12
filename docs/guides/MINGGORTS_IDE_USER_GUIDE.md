# MingGoRTS IDE 使用指南

## 🚀 快速開始

### 1. 啟動 IDE

```batch
cd C:\HWC\MingGoRTS\build\bin\Release
.\MingGoRTS_IDE.exe
```

### 2. 程序輸出說明

程序啟動後會顯示以下內容：

```
=== MingGoRTS IDE - AI-Powered Game Development Environment ===
Powered by Potato Engine AI Agent System

Initializing MingGoRTS IDE...
Setting root path: C:\HWC\MingGoRTS
Initializing AI Agent Interface...
Initializing IDE Agent Manager...
IDE Agent Manager initialized successfully
AI Agent Interface initialized successfully
AI Assistant enabled
MingGoRTS IDE initialized successfully

--- IDE Components Initialized ---
Workspace: C:\HWC\MingGoRTS
Project: MingGoRTS Game Project
AI Assistance: Enabled
Multi-Agent: Enabled
```

## 📋 IDE 組件說明

### 1. 專案管理
- 自動打開指定工作區的專案
- 支持專案文件管理
- 顯示專案路徑和名稱

### 2. 檔案操作
- 打開檔案：`OpenFile(filePath)`
- 保存檔案：`SaveFile(filePath)`
- 關閉檔案：`CloseFile(filePath)`
- 查看已打開的檔案列表

### 3. 檔案導航
- 設置根路徑：`SetRootPath(path)`
- 獲取目錄內容：`GetDirectoryContents(path)`
- 搜索檔案：`SearchFiles(pattern)`

### 4. 建置系統
- 配置建置系統：`ConfigureBuildSystem(buildType)`
- 建置專案：`BuildProject(configuration)`
- 清理專案：`CleanProject()`

### 5. AI Agent 交互
- 代理創建和管理
- 代碼生成請求
- 資產優化請求
- 關卡設計請求
- 性能分析請求
- 建置優化請求

## 🤖 AI Agent 使用方法

### 專門化遊戲開發代理

#### 1. EngineCodeAgent（引擎代碼代理）
- **功能**：遊戲引擎代碼生成和優化
- **用途**：生成遊戲引擎相關代碼、優化引擎性能
- **示例**：
  ```cpp
  agentManager->GenerateCode("Player movement system", GameDevAgentType::EngineCode);
  ```

#### 2. AssetAgent（資產代理）
- **功能**：資產優化和處理
- **用途**：優化紋理、模型、音頻等遊戲資產
- **示例**：
  ```cpp
  agentManager->OptimizeAsset("texture_player.png");
  ```

#### 3. LevelDesignAgent（關卡設計代理）
- **功能**：關卡設計和場景組成
- **用途**：設計遊戲關卡、放置實體、平衡遊戲性
- **示例**：
  ```cpp
  agentManager->DesignLevel("Forest level with enemy encounters");
  ```

#### 4. PerformanceAgent（性能代理）
- **功能**：性能分析和優化
- **用途**：分析代碼性能、優化渲染循環、減少延遲
- **示例**：
  ```cpp
  agentManager->AnalyzePerformance("Render loop optimization");
  ```

#### 5. BuildAgent（建置代理）
- **功能**：建置系統優化管理
- **用途**：優化 CMake 配置、管理建置流程
- **示例**：
  ```cpp
  agentManager->OptimizeBuild("CMake Release configuration");
  ```

## 💻 使用示例

### 示例 1：基本 IDE 操作

```cpp
#include "IDECore.h"
#include "AIIntegration.h"

using namespace MingGoRTSIDE;

int main() {
    // 配置 IDE
    IDEConfig config;
    config.workspacePath = "C:\\MyGameProject";
    config.projectName = "MyGame";
    config.enableAIAssistance = true;
    
    // 初始化 IDE
    IDECore ideCore;
    ideCore.Initialize(config);
    
    // 打開專案
    IDEProject* project = ideCore.GetProject();
    project->OpenProject("C:\\MyGameProject");
    
    // 打開檔案
    IDEEditor* editor = ideCore.GetEditor();
    editor->OpenFile("MainGame.cpp");
    editor->OpenFile("PlayerController.h");
    
    // 查看已打開的檔案
    auto openFiles = editor->GetOpenFiles();
    for (const auto& file : openFiles) {
        std::cout << "Open: " << file << std::endl;
    }
    
    // 關閉 IDE
    ideCore.Shutdown();
    
    return 0;
}
```

### 示例 2：使用 AI Agent 生成代碼

```cpp
// 獲取 AI 接口
AIAgentInterface* aiInterface = ideCore.GetAIInterface();
IDEAgentManager* agentManager = aiInterface->GetAgentManager();

// 創建專門化代理
agentManager->CreateAgent(GameDevAgentType::EngineCode, "CodeGenerator");

// 生成代碼
std::string codeResult = agentManager->GenerateCode(
    "Create a player controller with movement and jump mechanics",
    GameDevAgentType::EngineCode
);

std::cout << "Generated code: " << codeResult << std::endl;
```

### 示例 3：優化遊戲資產

```cpp
// 創建資產代理
agentManager->CreateAgent(GameDevAgentType::AssetAgent, "AssetOptimizer");

// 優化紋理
std::string result = agentManager->OptimizeAsset("assets/textures/character.png");
std::cout << "Asset optimization: " << result << std::endl;

// 優化模型
result = agentManager->OptimizeAsset("assets/models/character.fbx");
std::cout << "Asset optimization: " << result << std::endl;
```

### 示例 4：設計遊戲關卡

```cpp
// 創建關卡設計代理
agentManager->CreateAgent(GameDevAgentType::LevelDesign, "LevelDesigner");

// 設計關卡
std::string levelDesign = agentManager->DesignLevel(
    "Create a forest level with:\n"
    "- Starting area with campfire\n"
    "- Enemy encounters in the forest\n"
    "- Boss arena at the end\n"
    "- Hidden treasure locations"
);

std::cout << "Level design: " << levelDesign << std::endl;
```

### 示例 5：性能分析和優化

```cpp
// 創建性能代理
agentManager->CreateAgent(GameDevAgentType::Performance, "PerfAnalyzer");

// 分析性能
std::string perfAnalysis = agentManager->AnalyzePerformance(
    "Analyze render loop for bottlenecks and suggest optimizations"
);

std::cout << "Performance analysis: " << perfAnalysis << std::endl;
```

### 示例 6：建置系統優化

```cpp
// 創建建置代理
agentManager->CreateAgent(GameDevAgentType::BuildAgent, "BuildOptimizer");

// 優化建置配置
std::string buildOpt = agentManager->OptimizeBuild(
    "Optimize CMake configuration for faster Release builds"
);

std::cout << "Build optimization: " << buildOpt << std::endl;
```

## 🔧 當前演示版本說明

### 簡化 AI 系統
當前版本使用簡化的 AI 系統進行演示，原因是：
- 原始 `AI/AIAgentSystem.h` 存在編碼問題（C4819 警告）
- 存在枚舉語法錯誤
- 為了確保演示版本能夠正常編譯和運行

### 功能限制
- AI 代理返回模擬響應
- 實際代碼生成、資產優化等功能需要接入真實 AI 模型
- 工具系統使用簡化實現

### 未來改進
1. 修復 `AI/AIAgentSystem.h` 的編碼和語法問題
2. 整合完整的 Potato Engine AI Agent System
3. 接入真實的 LLM API（如 GPT-4、Claude 等）
4. 實現實際的代碼生成和執行
5. 添加圖形用戶界面（GUI）

## 🎯 推薦使用流程

### 1. 初始化階段
```cpp
// 配置 IDE
IDEConfig config;
config.workspacePath = "你的專案路徑";
config.enableAIAssistance = true;

// 初始化
IDECore ideCore;
ideCore.Initialize(config);
```

### 2. 創建專門化代理
```cpp
IDEAgentManager* agentManager = ideCore.GetAIInterface()->GetAgentManager();

agentManager->CreateAgent(GameDevAgentType::EngineCode, "EngineCodeAgent");
agentManager->CreateAgent(GameDevAgentType::AssetAgent, "AssetAgent");
agentManager->CreateAgent(GameDevAgentType::LevelDesign, "LevelDesignAgent");
agentManager->CreateAgent(GameDevAgentType::Performance, "PerformanceAgent");
agentManager->CreateAgent(GameDevAgentType::BuildAgent, "BuildAgent");
```

### 3. 使用 AI 功能
```cpp
// 根據需求選擇相應的代理
agentManager->GenerateCode("代碼描述", GameDevAgentType::EngineCode);
agentManager->OptimizeAsset("資產路徑");
agentManager->DesignLevel("關卡描述");
agentManager->AnalyzePerformance("性能分析請求");
agentManager->OptimizeBuild("建置配置");
```

### 4. 查看協作狀態
```cpp
std::string collabStatus = agentManager->GetCollaborationStatus();
std::cout << collabStatus << std::endl;
```

### 5. 清理資源
```cpp
ideCore.Shutdown();
```

## 📝 注意事項

1. **當前版本是演示版本**：AI 代理返回模擬響應，不進行實際的代碼生成或資產處理

2. **需要實際 AI 模型**：要實現真正的 AI 功能，需要接入 LLM API

3. **圖形界面待開發**：當前版本是命令行演示，GUI 版本待開發

4. **文件路徑**：確保使用正確的文件路徑格式（Windows 使用反斜杠或雙反斜杠）

5. **編碼問題**：如果遇到編碼問題，確保源文件使用 UTF-8 編碼

## 🚀 下一步發展

1. **修復 AI 系統編碼問題**：解決 `AI/AIAgentSystem.h` 的 C4819 警告
2. **整合真實 AI 模型**：接入 GPT-4、Claude 等 LLM API
3. **開發圖形界面**：創建 VS Code 風格的 GUI IDE
4. **實現實際工具**：實現真正的代碼生成、資產處理等功能
5. **添加更多代理類型**：擴展更多遊戲開發專門化代理

## 📞 技術支持

如有問題，請查看：
- 架構文檔：`docs/MINGGORTS_IDE_ARCHITECTURE.md`
- 完成報告：`docs/MINGGORTS_IDE_COMPLETION_REPORT.md`
- 源代碼：`MingGoRTS_IDE/` 目錄

**🥔 享受 AI 驅動的遊戲開發體驗！**
