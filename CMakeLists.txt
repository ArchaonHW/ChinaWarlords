# Potato Engine - CMake 建置配置
# 完全獨立的遊戲引擎，不依賴 UE5

cmake_minimum_required(VERSION 3.15)
project(PotatoEngine VERSION 1.0.0 LANGUAGES C CXX)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 設置輸出目錄
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 編譯選項
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# 安全強化旗標 (Security hardening)
# 可用 -DPOTATO_SECURITY_HARDENING=OFF 關閉
# ============================================================================
option(POTATO_SECURITY_HARDENING "Enable security hardening flags" ON)
if(POTATO_SECURITY_HARDENING)
    if(MSVC)
        # /sdl: SDL 安全檢查, /GS: 堆疊保護, /guard:cf: 控制流防護
        add_compile_options(/sdl /GS /guard:cf)
        # ASLR + DEP + 高位址熵 + CFG + CET 陰影堆疊
        add_link_options(/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /GUARD:CF /CETCOMPAT)
    else()
        # 堆疊保護 + PIE (位置無關執行檔，啟用 ASLR)
        add_compile_options(-fstack-protector-strong -fstack-clash-protection -fPIE)
        if(WIN32)
            # MinGW: PE 格式加固(-pie 只能套用於執行檔,SHARED/MODULE 庫加上會導致要求 WinMain)
            add_link_options(
                $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-pie>
                -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va)
        else()
            # Linux/macOS: ELF 加固
            add_compile_definitions(_FORTIFY_SOURCE=2)
            add_link_options(
                $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-pie>
                -Wl,-z,relro,-z,now -Wl,-z,noexecstack)
        endif()
    endif()
endif()

# MinGW: 靜態連結 GCC 執行時（libgcc/libstdc++/winpthread），
# 讓產物不依賴 MinGW bin 目錄的 DLL,可直接執行
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

# 查找依賴
find_package(OpenGL REQUIRED)

# GLFW - 窗口和輸入管理
# 優先使用本機安裝的 GLFW（設 GLFW_ROOT 或 CMAKE_PREFIX_PATH 指向安裝目錄）,
# 找不到才從 GitHub FetchContent
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    find_path(GLFW3_INCLUDE_DIR GLFW/glfw3.h
        HINTS ${GLFW_ROOT} $ENV{GLFW_ROOT}
        PATH_SUFFIXES include)
    # 依工具鏈選擇搜尋順序——MSVC 連 MinGW 的 libglfw3.a 會
    # 出現 __mingw_* 未解析符號；MinGW 連 VC 的 .lib 亦然
    if(MSVC)
        set(_glfw_lib_suffixes lib lib-vc2026 lib-vc2022 lib-vc2019 lib-vc2017)
    else()
        set(_glfw_lib_suffixes lib lib-mingw-w64 lib-static-ucrt)
    endif()
    find_library(GLFW3_LIBRARY NAMES glfw3 glfw
        HINTS ${GLFW_ROOT} $ENV{GLFW_ROOT}
        PATH_SUFFIXES ${_glfw_lib_suffixes})
    if(GLFW3_INCLUDE_DIR AND GLFW3_LIBRARY)
        add_library(glfw STATIC IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_LOCATION "${GLFW3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIR}")
        set(glfw3_FOUND TRUE)
        message(STATUS "Using local GLFW: ${GLFW3_LIBRARY}")
    endif()
endif()
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLAD - OpenGL 加載器
# 使用本地生成的 glad1 綁定（external/glad_gen）,提供 <glad/glad.h>
# 注意:glad v2.x 的頭文件是 <glad/gl.h>,與引擎代碼不相容,勿改回 FetchContent
add_library(glad STATIC external/glad_gen/src/glad.c)
target_include_directories(glad PUBLIC external/glad_gen/include)

# miniaudio - F-4 音訊後端（external/miniaudio 單標頭庫,public domain/MIT-0;
# MINIAUDIO_IMPLEMENTATION 集中 Audio/MiniaudioImpl.cpp,隨 Audio/*.cpp
# glob 進 PotatoEngine,不需獨立 target)

# 包含目錄
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/Physics
    ${CMAKE_CURRENT_SOURCE_DIR}/Audio
    ${CMAKE_CURRENT_SOURCE_DIR}/Input
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources
    ${CMAKE_CURRENT_SOURCE_DIR}/ECS
    ${CMAKE_CURRENT_SOURCE_DIR}/GameObject
    ${CMAKE_CURRENT_SOURCE_DIR}/Events
    ${CMAKE_CURRENT_SOURCE_DIR}/FileSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/Logging
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils
    ${CMAKE_CURRENT_SOURCE_DIR}/Memory
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform
    ${CMAKE_CURRENT_SOURCE_DIR}/Scene
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/Time
    ${CMAKE_CURRENT_SOURCE_DIR}/AI
    ${CMAKE_CURRENT_SOURCE_DIR}/Security
    ${OPENGL_INCLUDE_DIRS}
)

# Project files - 所有引擎模組
file(GLOB_RECURSE ENGINE_SOURCES 
    "Core/*.cpp"
    "Security/*.cpp"      # 安全模組 - 自包含，無引擎依賴
    "Events/*.cpp"
    "FileSystem/*.cpp"
    "Logging/*.cpp"
    "Memory/*.cpp"
    "Platform/*.cpp"
    "Scene/*.cpp"
    "Serialization/*.cpp"
    "Time/*.cpp"
    "Rendering/*.cpp"
    "Physics/*.cpp"
    "Audio/*.cpp"
    "Input/*.cpp"
    "Resources/*.cpp"
    "ECS/*.cpp"
    "GameObject/*.cpp"
    "Networking/*.cpp"
    # "GUI/*.cpp"      # AgentGUI 依賴遊戲側 AIAgentSystem,不進引擎
    # "MathUtils/*.cpp"      # 全 header-only
    # "AI/*.cpp"      # AI 模組另有獨立靜態庫目標,避免重複編譯
)

file(GLOB_RECURSE ENGINE_HEADERS
    "Core/*.h"
    "Rendering/*.h"
    "Physics/*.h"
    "Audio/*.h"
    "Input/*.h"
    "Resources/*.h"
    "ECS/*.h"
    "GameObject/*.h"
    "Events/*.h"
    "FileSystem/*.h"
    "Logging/*.h"
    "Memory/*.h"
    "Platform/*.h"
    "Scene/*.h"
    "Serialization/*.h"
    "Time/*.h"
    "AI/*.h"
    "MathUtils/*.h"
    "Security/*.h"
    "Networking/*.h"
)

# 創建引擎庫
add_library(PotatoEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(PotatoEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils
)

# tinygltf/miniaudio 實作單元只需私有 include 路徑；
# PUBLIC 會把 header-only 庫的 include 污染給所有下游 target
target_include_directories(PotatoEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/external/miniaudio
)

target_link_libraries(PotatoEngine
    ${OPENGL_LIBRARIES}
    glfw
    glad
    ${CMAKE_DL_LIBS}
    NeuralNetwork # NeuralGraphics 深度學習繪圖依賴
)

# Windows: psapi.lib（Security 模組的模組枚舉用；MSVC 由 #pragma comment 處理，MinGW 需顯式連結）
# wintrust: Authenticode 簽章驗證;bcrypt: BCryptGenRandom 隨機數
# ws2_32: Networking UdpSocket 的 Winsock2
if(WIN32 AND NOT MSVC)
    target_link_libraries(PotatoEngine psapi wintrust bcrypt ws2_32)
elseif(MSVC)
    target_link_libraries(PotatoEngine ws2_32)
endif()

target_compile_definitions(PotatoEngine PRIVATE POTATO_ENGINE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(PotatoEngine PRIVATE _USE_MATH_DEFINES)
# 防止 GLFW 自動引入系統 gl.h,與 glad 衝突(引擎與所有連結目標都需要)
target_compile_definitions(PotatoEngine PUBLIC GLFW_INCLUDE_NONE)

# ============================================================================
# 引擎附屬函式庫（消費端可連結）——置於 standalone 守衛外
# ============================================================================

# AI 增強模塊 - 神經網絡
add_library(NeuralNetwork STATIC AI/NeuralNetwork.cpp)
target_include_directories(NeuralNetwork PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)

# 量子模擬層 - 態向量暫存器/qudit（疊加態/糾纏/Born rule 測量）
add_library(Quantum STATIC
    Quantum/QubitRegister.cpp
    Quantum/Qudit.cpp
)
target_include_directories(Quantum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 影音生成管線共用層（AD-1/AD-3/AD-4）
# HeadlessCapture.cpp 含 GL——static lib 只在 exe 實際參考時才拉入，
# 用到它的工具自行連 glad/glfw；lib 本身不宣告 GL 依賴 實際參考時才拉入，
# 用到它的工具自行連 glad/glfw；lib 本身不宣告 GL 依賴
add_library(Media STATIC
    Media/DatasetManifest.cpp
    Media/VideoEncoder.cpp
    Media/PipelineRunner.cpp
    Media/HeadlessCapture.cpp
)
target_include_directories(Media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Media PUBLIC PotatoEngine)

# ============================================================================
# Standalone 守衛：examples/tests/install 只在引擎 repo 獨立建置時註冊；
# 被遊戲 repo 以 add_subdirectory 消費時只產出函式庫 target。
# ============================================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

# 示例程序
# add_executable(SimpleExample Examples/SimpleExample.cpp)
# target_link_libraries(SimpleExample PotatoEngine)

# 最小測試程序
add_executable(MinimalTest Examples/MinimalTest.cpp)
target_include_directories(MinimalTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MinimalTest)

# 引擎示例程序
add_executable(EngineExample Examples/EngineExample.cpp)
target_include_directories(EngineExample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EngineExample PotatoEngine)

# AI 演示程序（編碼兼容版本）
add_executable(AIDemo Examples/AIDemo.cpp)
target_link_libraries(AIDemo)

# 優化 AI 演示程序（性能優化版本）
add_executable(OptimizedAIDemo Examples/OptimizedAIDemo.cpp)
target_link_libraries(OptimizedAIDemo)

# AI 測試套件（完整測試驗證）
add_executable(AITestSuite Examples/AITestSuite.cpp)
target_link_libraries(AITestSuite)

# AI 增強模塊 - 強化學習
add_library(ReinforcementLearning STATIC AI/ReinforcementLearning.cpp)
target_include_directories(ReinforcementLearning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
target_link_libraries(ReinforcementLearning NeuralNetwork)

# AI 增強模塊 - 自然語言處理
add_library(NaturalLanguageProcessing STATIC AI/NaturalLanguageProcessing.cpp)
target_include_directories(NaturalLanguageProcessing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)

# AI Agent 系統
add_library(AIAgentSystem STATIC AI/AIAgentSystem.cpp)
target_include_directories(AIAgentSystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)

# AI 增強模塊 - LLM 集成
add_library(LLMIntegration STATIC AI/LLMIntegration.cpp)
target_include_directories(LLMIntegration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
# WinHTTP transport：MSVC 由 #pragma comment 處理，MinGW 需顯式連結
if(WIN32 AND NOT MSVC)
    target_link_libraries(LLMIntegration winhttp)
endif()

# AI 增強模塊 - RAG 系統
add_library(RAGSystem STATIC AI/RAGSystem.cpp)
target_include_directories(RAGSystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
target_link_libraries(RAGSystem LLMIntegration)

# AI 增強模塊 - Agent 鏈
add_library(AgentChain STATIC AI/AgentChain.cpp)
target_include_directories(AgentChain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
target_link_libraries(AgentChain LLMIntegration)

# AI 增強模塊 - 工具框架
add_library(ToolFramework STATIC AI/ToolFramework.cpp)
target_include_directories(ToolFramework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
target_link_libraries(ToolFramework LLMIntegration)

# AI 增強模塊 - 知識圖譜（長期記憶與關係推理）
add_library(KnowledgeGraph STATIC AI/KnowledgeGraph.cpp)
target_include_directories(KnowledgeGraph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)

# AI 增強模塊 - 自我反思引擎（元認知與決策校準）
add_library(SelfReflection STATIC AI/SelfReflection.cpp)
target_include_directories(SelfReflection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)

# AI 增強模塊 - 智能開發系統
add_library(IntelligentDevelopmentSystem STATIC AI/IntelligentDevelopmentSystem.cpp)
target_include_directories(IntelligentDevelopmentSystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/AI)
target_link_libraries(IntelligentDevelopmentSystem LLMIntegration RAGSystem AgentChain ToolFramework AIAgentSystem NeuralNetwork NaturalLanguageProcessing KnowledgeGraph SelfReflection)

# MingGoRTS IDE 智能建議系統
add_library(IntelligentSuggestion STATIC MingGoRTS_IDE/IntelligentSuggestion.cpp)
target_include_directories(IntelligentSuggestion PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/MingGoRTS_IDE
)

# AI 增強演示程序
add_executable(AIEnhancedDemo Examples/AIEnhancedDemo.cpp)
target_include_directories(AIEnhancedDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AIEnhancedDemo NeuralNetwork ReinforcementLearning NaturalLanguageProcessing)

# 終極 AI Agent 演示程序
add_executable(UltimateAIAgentDemo Examples/UltimateAIAgentDemo.cpp)
target_include_directories(UltimateAIAgentDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UltimateAIAgentDemo AIAgentSystem LLMIntegration RAGSystem AgentChain ToolFramework NeuralNetwork NaturalLanguageProcessing)

# 智能開發系統演示程序
add_executable(IntelligentDevelopmentDemo Examples/IntelligentDevelopmentDemo.cpp)
target_include_directories(IntelligentDevelopmentDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(IntelligentDevelopmentDemo IntelligentDevelopmentSystem LLMIntegration AIAgentSystem)

# 認知增強演示（知識圖譜 + 自我反思）
add_executable(CognitiveDemo Examples/CognitiveDemo.cpp)
target_include_directories(CognitiveDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CognitiveDemo KnowledgeGraph SelfReflection)

# 開發助手本地管線煙霧測試（無 LLM 客戶端亦可生成）
add_executable(DevAssistantSmoke Examples/DevAssistantSmoke.cpp)
target_include_directories(DevAssistantSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DevAssistantSmoke
    IntelligentDevelopmentSystem
    IntelligentSuggestion
    KnowledgeGraph
    SelfReflection
    LLMIntegration
    RAGSystem
    AgentChain
    ToolFramework
    AIAgentSystem
    NeuralNetwork
    NaturalLanguageProcessing
)

# 智能建議規則測試（命中行號/anti-pattern 接線/優化偵測/信心保留）
add_executable(SuggestionRulesTest Examples/SuggestionRulesTest.cpp)
target_include_directories(SuggestionRulesTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SuggestionRulesTest IntelligentSuggestion)

# 戰鬥玩法層 - 小隊/flow field/doctrine 腳本/戰鬥控制器
add_library(Gameplay STATIC
    Gameplay/FlowField.cpp
    Gameplay/Squad.cpp
    Gameplay/Doctrine.cpp
    Gameplay/BattleController.cpp
    Gameplay/BattlePlanner.cpp
    Gameplay/BattleSceneSync.cpp
    Gameplay/BattleMap.cpp
    Gameplay/EnemyGeneral.cpp
    Gameplay/BattleResources.cpp
    Gameplay/BattleRecorder.cpp
    Gameplay/Roster.cpp
    Gameplay/SageCommand.cpp
    Gameplay/QuantumFog.cpp
    Gameplay/BattlePicker.cpp
    Gameplay/PlanningDeck.cpp
    Gameplay/PostBattle.cpp
    Gameplay/RefitCamp.cpp
    Gameplay/MapGenerator.cpp
    Gameplay/SquadTemplate.cpp
    Gameplay/BattlePlan.cpp
    Gameplay/HistorianReport.cpp
    Gameplay/RivalDeck.cpp
    Gameplay/CampaignLedger.cpp
    Gameplay/GeneralDossier.cpp
    Gameplay/MythLog.cpp
    Gameplay/ChapterConventions.cpp
    Gameplay/Ledger.cpp
    Gameplay/GovernanceField.cpp
    Gameplay/ShrineField.cpp
    Gameplay/MythIncursion.cpp
    Gameplay/SeepageStage.cpp
    Gameplay/NoBattleAdvantage.cpp
    Gameplay/SubversionDefect.cpp
    Gameplay/CardValidator.cpp
    Gameplay/CardEditor.cpp
    Gameplay/EnemyDeck.cpp
    Gameplay/BattleCommandAI.cpp
    Gameplay/BattleCommandEnv.cpp
    Gameplay/NarrativePack.cpp
    Gameplay/DoctrineLibrary.cpp
)
target_include_directories(Gameplay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Gameplay)
# BattleSceneSync 需要場景圖/渲染類型;PotatoEngine 不依賴 Gameplay,無循環
# QuantumFog 需要量子態模擬（疊加態敵情霧）
# BattleCommandEnv/CommanderAI 需要 DQNAgent/RLEnvironment（訓練-推論共用契約）
target_link_libraries(Gameplay PUBLIC PotatoEngine Quantum ReinforcementLearning)

# E-7 戰役層：跨章節狀態 facade（potato.campaign/1 聚合存檔）
# 分層：PotatoEngine ← Gameplay ← Campaign，Campaign 不回流 Gameplay
add_library(Campaign STATIC
    Campaign/CampaignState.cpp
    Campaign/ChapterLibrary.cpp
    Campaign/Governance.cpp
    Campaign/MythLayer.cpp
    Campaign/PeaceHazard.cpp
    Campaign/CooperationDynamics.cpp
    Campaign/FateExchange.cpp
    Campaign/NoBattleResolver.cpp
    Campaign/EndingPage.cpp
    Campaign/PlayerDeck.cpp
    Campaign/TutorialFlow.cpp
)
target_include_directories(Campaign PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Campaign PUBLIC Gameplay)

# Doctrine 戰鬥演示（無頭模式：規劃階段寫腳本，即時層自動執行）
add_executable(DoctrineBattleDemo Examples/DoctrineBattleDemo.cpp)
target_include_directories(DoctrineBattleDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DoctrineBattleDemo Gameplay)

# 智能規劃演示（AI 參謀自動生成 doctrine 後執行戰鬥）
add_executable(AutoPlannerDemo Examples/AutoPlannerDemo.cpp)
target_include_directories(AutoPlannerDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AutoPlannerDemo Gameplay)

# 斷橋原型（T-4~T-8：地圖JSON/敵將腳本/資源/錄製回放/名冊）
add_executable(DuanqiaoDemo Examples/DuanqiaoDemo.cpp)
target_include_directories(DuanqiaoDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DuanqiaoDemo Gameplay)

# 至聖者指揮學邏輯測試（headless：三權/正逆六策/墮落/五行/治平）
add_executable(SageCommandTest Examples/SageCommandTest.cpp)
target_include_directories(SageCommandTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SageCommandTest Gameplay)

# 至聖者指揮畫面（ImGui 五區佈局：道權/戰場/策權/兵權/治平儀表）
# imgui 源碼比照 MingGoRTS_IDE_GUI 區塊
add_executable(SageCommandDemo
    Examples/SageCommandDemo.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(SageCommandDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(SageCommandDemo glfw glad ${OPENGL_LIBRARIES} Gameplay)

# 量子模組測試（headless：暫存器/qudit/敵情霧的 Born rule 與時效）
add_executable(QuantumTest Examples/QuantumTest.cpp)
target_include_directories(QuantumTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuantumTest Quantum Gameplay)

# 準隨機/藍噪取樣測試（headless：差異度、最小間距、種子決定性）
add_executable(QuasiRandomTest Examples/QuasiRandomTest.cpp)
target_include_directories(QuasiRandomTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuasiRandomTest Gameplay)

# Collatz 雜湊/擁擠阻塞模型測試（headless,純引擎層,可同步上游）
add_executable(QuasiModelsTest Examples/QuasiModelsTest.cpp)
target_include_directories(QuasiModelsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuasiModelsTest PotatoEngine)

# 第三批引擎件測試（headless:GustField 間歇場、GolombRuler 全異距）
add_executable(UnsolvedBatch3Test Examples/UnsolvedBatch3Test.cpp)
target_include_directories(UnsolvedBatch3Test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UnsolvedBatch3Test PotatoEngine)

# RTS 點選層測試（headless：螢幕→射線→地面/小隊包圍球）
add_executable(BattlePickerTest Examples/BattlePickerTest.cpp)
target_include_directories(BattlePickerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattlePickerTest Gameplay)

# T-9 回合層牌組測試（headless：卡槽編輯/驗證/AI 模板/Commit）
add_executable(PlanningDeckTest Examples/PlanningDeckTest.cpp)
target_include_directories(PlanningDeckTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PlanningDeckTest Gameplay)

# 斷橋可玩 demo（E4-3：LMB 選取/RMB 下令走 CP 介入；A-4 膠囊體+選取環+血條）
# 互動視窗程式——無顯示環境自行 [SKIP]，不進 POTATO_TESTS
add_executable(DuanqiaoPlayable
    Examples/DuanqiaoPlayable.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(DuanqiaoPlayable PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(DuanqiaoPlayable glfw glad ${OPENGL_LIBRARIES} Gameplay Campaign)

# Q-1 敵情霧戰鬥整合測試（headless：觀測塌縮/時效回雲/接觸揭露/Scout）
add_executable(QuantumFogBattleTest Examples/QuantumFogBattleTest.cpp)
target_include_directories(QuantumFogBattleTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuantumFogBattleTest Gameplay)

# Q-8 疊加態效能基準（無頭、入 ctest）
add_executable(QuantumBench Examples/QuantumBench.cpp)
target_include_directories(QuantumBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuantumBench Gameplay)

# Q-9 量子偵查教學情境（ImGui：三段引導 observe→probe→時效）
add_executable(DuanqiaoTutorial
    Examples/DuanqiaoTutorial.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(DuanqiaoTutorial PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(DuanqiaoTutorial glfw glad ${OPENGL_LIBRARIES} Gameplay)

# Q-9 教學狀態機無頭測試
add_executable(TutorialScriptTest Examples/TutorialScriptTest.cpp)
target_include_directories(TutorialScriptTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TutorialScriptTest Gameplay)

# 量子敵情霧可視化（ImGui：機率雲/觀測塌縮/情報時效）
add_executable(QuantumFogDemo
    Examples/QuantumFogDemo.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(QuantumFogDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(QuantumFogDemo glfw glad ${OPENGL_LIBRARIES} Gameplay)

# AI Agent 介面無頭煙霧測試（IDECore+AIIntegration 自包含，無 GUI 依賴）
add_executable(AIAgentSmoke Examples/AIAgentSmoke.cpp MingGoRTS_IDE/IDECore.cpp MingGoRTS_IDE/AIIntegration.cpp)
target_include_directories(AIAgentSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# MingGoRTS IDE（AI Agent 整合 IDE - 命令行版本）
add_executable(MingGoRTS_IDE_CLI MingGoRTS_IDE/MingGoRTS_IDE_Main.cpp MingGoRTS_IDE/IDECore.cpp MingGoRTS_IDE/AIIntegration.cpp)
target_include_directories(MingGoRTS_IDE_CLI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# MingGoRTS IDE（GUI 版本）
add_executable(MingGoRTS_IDE_GUI 
    MingGoRTS_IDE/GUI/Main.cpp
    MingGoRTS_IDE/GUI/IDEGUI.cpp
    MingGoRTS_IDE/GUI/IDEGUI.h
    MingGoRTS_IDE/GUI/I18N.cpp
    MingGoRTS_IDE/GUI/I18N.h
    MingGoRTS_IDE/IDECore.cpp
    MingGoRTS_IDE/AIIntegration.cpp
    Rendering/ImageCodec.cpp
    Logging/Logger.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(MingGoRTS_IDE_GUI PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(MingGoRTS_IDE_GUI glfw ${OPENGL_LIBRARIES}
    IntelligentDevelopmentSystem
    IntelligentSuggestion
    LLMIntegration
    RAGSystem
    AgentChain
    ToolFramework
    AIAgentSystem
    NeuralNetwork
    NaturalLanguageProcessing
    KnowledgeGraph
    SelfReflection
)

# 數學測試程序
add_executable(MathTest Examples/MathTest.cpp)
target_include_directories(MathTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MathTest)

# 物理積分測試
add_executable(PhysicsTest Examples/PhysicsTest.cpp)
target_include_directories(PhysicsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 物理數學測試（headless：Verlet 精度、無散度湍流場、混沌敏感度）
add_executable(PhysicsMathTest Examples/PhysicsMathTest.cpp)
target_include_directories(PhysicsMathTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PhysicsMathTest PotatoEngine)
target_link_libraries(PhysicsTest PotatoEngine)

# 安全模組紅隊測試（對抗性驗證 Security/ 的偵測能力）
add_executable(SecurityRedTeamTest Examples/SecurityRedTeamTest.cpp)
target_include_directories(SecurityRedTeamTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SecurityRedTeamTest PotatoEngine)

# 內容掃毒測試：EICAR/黑名單/啟發式/隔離（headless,純引擎層）
add_executable(ContentScannerTest Examples/ContentScannerTest.cpp)
target_include_directories(ContentScannerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ContentScannerTest PotatoEngine)

# 掃毒整合管線測試：ResourceManager 載入閘門/DownloadGuard 落盤即掃/
# IntegrityManifest 自身完整性（headless,純引擎層）
add_executable(SecurityPipelineTest Examples/SecurityPipelineTest.cpp)
target_include_directories(SecurityPipelineTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SecurityPipelineTest PotatoEngine)

# 稽核帳本測試：hash 鏈 append-only/零容忍調節/L0-L2 分層揭露（headless,純引擎層）
add_executable(AuditLedgerTest Examples/AuditLedgerTest.cpp)
target_include_directories(AuditLedgerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AuditLedgerTest PotatoEngine)

# 資料防護層測試：ProtectedValue 混淆容器 + SignData 簽章（headless）
add_executable(SecurityDataTest Examples/SecurityDataTest.cpp)
target_include_directories(SecurityDataTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SecurityDataTest PotatoEngine)

# 記憶體混淆/資料簽章防竄改測試（headless,純引擎層）
add_executable(AntiTamperTest Examples/AntiTamperTest.cpp)
target_include_directories(AntiTamperTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AntiTamperTest PotatoEngine)

# JobSystem 工作池契約測試（headless：投遞/批次/help-run/巢狀/析構排空）
add_executable(JobSystemTest Examples/JobSystemTest.cpp)
target_include_directories(JobSystemTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(JobSystemTest PotatoEngine)

# Profiler 區段計時契約測試（headless：巢狀/啟停/序列化報表）
add_executable(ProfilerTest Examples/ProfilerTest.cpp)
target_include_directories(ProfilerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ProfilerTest PotatoEngine)

# 簽章存檔測試（headless：往返/竄改偵測/錯金鑰/截短）
add_executable(SignedSaveFileTest Examples/SignedSaveFileTest.cpp)
target_include_directories(SignedSaveFileTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SignedSaveFileTest PotatoEngine)

# CPU 粒子模擬測試（headless:發射/壽命/積分/流場/決定性）
add_executable(ParticleSystemTest Examples/ParticleSystemTest.cpp)
target_include_directories(ParticleSystemTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ParticleSystemTest PotatoEngine)

# UDP + lockstep 同步測試（headless,localhost loopback）
add_executable(NetworkingTest Examples/NetworkingTest.cpp)
target_include_directories(NetworkingTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NetworkingTest PotatoEngine)

# Tween 插值/Ease 曲線測試（headless）
add_executable(TweenTest Examples/TweenTest.cpp)
target_include_directories(TweenTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TweenTest PotatoEngine)

# SpatialHash + A*/FlowField 測試（headless,純 MathUtils）
add_executable(SpatialPathTest Examples/SpatialPathTest.cpp)
target_include_directories(SpatialPathTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SpatialPathTest PotatoEngine)

# FileWatcher 輪詢監視測試（headless,temp dir）
add_executable(FileWatcherTest Examples/FileWatcherTest.cpp)
target_include_directories(FileWatcherTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FileWatcherTest PotatoEngine)

# 紅隊測試用的無害 DLL（模擬被注入的作弊模組）
add_library(FakeCheatModule SHARED Examples/FakeCheatModule.cpp)
set_target_properties(FakeCheatModule PROPERTIES OUTPUT_NAME "FakeCheat" PREFIX "")
add_dependencies(SecurityRedTeamTest FakeCheatModule)

# 子系統測試程序
add_executable(FileSystemTest Examples/FileSystemTest.cpp)
target_include_directories(FileSystemTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FileSystemTest PotatoEngine)

add_executable(LoggerTest Examples/LoggerTest.cpp)
target_include_directories(LoggerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LoggerTest PotatoEngine)

add_executable(MemoryTest Examples/MemoryTest.cpp)
target_include_directories(MemoryTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MemoryTest PotatoEngine)

add_executable(PlatformTest Examples/PlatformTest.cpp)
target_include_directories(PlatformTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PlatformTest PotatoEngine)

add_executable(SceneTest Examples/SceneTest.cpp)
target_include_directories(SceneTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SceneTest PotatoEngine)

add_executable(SerializationTest Examples/SerializationTest.cpp)
target_include_directories(SerializationTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SerializationTest PotatoEngine)

add_executable(RenderPipelineTest Examples/RenderPipelineTest.cpp)
target_include_directories(RenderPipelineTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RenderPipelineTest PotatoEngine)

add_executable(BattleSceneTest Examples/BattleSceneTest.cpp)
target_include_directories(BattleSceneTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleSceneTest Gameplay)

add_executable(SquadFatigueTest Examples/SquadFatigueTest.cpp)
target_include_directories(SquadFatigueTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SquadFatigueTest Gameplay)

add_executable(TerrainBonusTest Examples/TerrainBonusTest.cpp)
target_include_directories(TerrainBonusTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TerrainBonusTest Gameplay)

add_executable(PostBattleTest Examples/PostBattleTest.cpp)
target_include_directories(PostBattleTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PostBattleTest Gameplay)

add_executable(MapGenTest Examples/MapGenTest.cpp)
target_include_directories(MapGenTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MapGenTest Gameplay)

# P-3 擁擠阻塞玩法接線測試（headless：SetJamming→crowdFactor→減速）
add_executable(JammingBattleTest Examples/JammingBattleTest.cpp)
target_include_directories(JammingBattleTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(JammingBattleTest Gameplay)

add_executable(SquadTemplateTest Examples/SquadTemplateTest.cpp)
target_include_directories(SquadTemplateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SquadTemplateTest Gameplay)

add_executable(GeneralGuardTest Examples/GeneralGuardTest.cpp)
target_include_directories(GeneralGuardTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GeneralGuardTest Gameplay)

add_executable(BattlePlanTest Examples/BattlePlanTest.cpp)
target_include_directories(BattlePlanTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattlePlanTest Gameplay)

# quantum-plan-effects：計畫加成依情報確定度縮放（無頭,進 POTATO_TESTS）
add_executable(QuantumPlanTest Examples/QuantumPlanTest.cpp)
target_include_directories(QuantumPlanTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(QuantumPlanTest Gameplay)

# G-9 戰鬥間整補循環測試（headless：傷兵池/招募/遺物/部署/存檔）
add_executable(RefitCampTest Examples/RefitCampTest.cpp)
target_include_directories(RefitCampTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RefitCampTest Gameplay)

# 影音管線 CPU 段無頭測試（GL/ffmpeg 段不進測試）
add_executable(MediaPipeTest Examples/MediaPipeTest.cpp)
target_include_directories(MediaPipeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MediaPipeTest Media)

# 主題 token 品質門檻（headless：WCAG 對比地板/hover 可辨/UTF-8 步進器）
add_executable(UIThemeTest
    Examples/UIThemeTest.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)
target_include_directories(UIThemeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(UIThemeTest PotatoEngine) # SpriteAtlas(F-1)

# F-2 UI scale 真管線無頭測試（絕對重建/鉗制/Px/settings 持久化）
add_executable(UIScaleTest
    Examples/UIScaleTest.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)
target_include_directories(UIScaleTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(UIScaleTest PotatoEngine)

# F-3 HUD 密度三檔無頭測試（門檻矩陣/序數鉗制/settings 持久化）
add_executable(HUDDensityTest
    Examples/HUDDensityTest.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)
target_include_directories(HUDDensityTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(HUDDensityTest PotatoEngine)

# RenderTarget 離屏 FBO 測試（GL-gated：無顯示環境 SKIP）
add_executable(RenderTargetTest Examples/RenderTargetTest.cpp)
target_include_directories(RenderTargetTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RenderTargetTest PotatoEngine glad glfw)

# N-1 史官戰報組裝器無頭測試
add_executable(HistorianReportTest Examples/HistorianReportTest.cpp)
target_include_directories(HistorianReportTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HistorianReportTest Gameplay)

add_executable(RivalDeckTest Examples/RivalDeckTest.cpp)
target_include_directories(RivalDeckTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RivalDeckTest Gameplay)

add_executable(CampaignLedgerTest Examples/CampaignLedgerTest.cpp)
target_include_directories(CampaignLedgerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CampaignLedgerTest Gameplay)

# N-2 敵將判詞（聽聞態）無頭測試
add_executable(GeneralDossierTest Examples/GeneralDossierTest.cpp)
target_include_directories(GeneralDossierTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GeneralDossierTest Gameplay)

# N-5 神話事件具名記錄 + 滲透掛鉤無頭測試
add_executable(MythLogTest Examples/MythLogTest.cpp)
target_include_directories(MythLogTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MythLogTest Gameplay)

# N-6 章節慣例文字層（題詞/判詞/欲知後事/結局四聲部）無頭測試
add_executable(ChapterConventionsTest Examples/ChapterConventionsTest.cpp)
target_include_directories(ChapterConventionsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ChapterConventionsTest Gameplay)

# L-1/L-2 帳本機械化：複式記帳 + 雜湊鏈無頭測試
add_executable(LedgerChainTest Examples/LedgerChainTest.cpp)
target_include_directories(LedgerChainTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerChainTest Gameplay)

# L-3 查帳戰報：HistorianReport 消費 LedgerChain 無頭測試
add_executable(LedgerAuditTest Examples/LedgerAuditTest.cpp)
target_include_directories(LedgerAuditTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerAuditTest Gameplay)

# L-5 回放完整性：battle_replay rootHash 驗證無頭測試
add_executable(ReplayIntegrityTest Examples/ReplayIntegrityTest.cpp)
target_include_directories(ReplayIntegrityTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ReplayIntegrityTest Gameplay)

# L-4 偽帳機制：InjectForgery + 疑帳標記無頭測試
add_executable(LedgerForgeTest Examples/LedgerForgeTest.cpp)
target_include_directories(LedgerForgeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerForgeTest Gameplay)

# L-7 產生軌跡：provenance 欄位 + hash 保護 + 降級相容無頭測試
add_executable(LedgerProvenanceTest Examples/LedgerProvenanceTest.cpp)
target_include_directories(LedgerProvenanceTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerProvenanceTest Gameplay)

# L-6 帳簿登錄編號：rootHash 派生短碼無頭測試
add_executable(LedgerRegistryTest Examples/LedgerRegistryTest.cpp)
target_include_directories(LedgerRegistryTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerRegistryTest Gameplay)

# L-8 查證式史官：seeded 抽驗 + 三態確信結論無頭測試
add_executable(LedgerAssuranceTest Examples/LedgerAssuranceTest.cpp)
target_include_directories(LedgerAssuranceTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LedgerAssuranceTest Gameplay)

# E-7 戰役框架 facade 無頭測試（roundtrip/壞檔拒絕/原子寫）
add_executable(CampaignStateTest Examples/CampaignStateTest.cpp)
target_include_directories(CampaignStateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CampaignStateTest Campaign)

# C-1 章節定義庫無頭測試（schema 驗證/消毒/LoadDir 除重/真實資產）
add_executable(ChapterLibraryTest Examples/ChapterLibraryTest.cpp)
target_include_directories(ChapterLibraryTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ChapterLibraryTest Campaign)

# C-2 戰役治理累加器無頭測試（折半率/動亂級/持久化）
add_executable(GovernanceTest Examples/GovernanceTest.cpp)
target_include_directories(GovernanceTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GovernanceTest Campaign)

# Epic A 戰場治理追蹤無頭測試（佔領/焚村/護輜/墮落 ratchet）
add_executable(GovernanceFieldTest Examples/GovernanceFieldTest.cpp)
target_include_directories(GovernanceFieldTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GovernanceFieldTest Campaign)

# D-1 神話滲透狀態機無頭測試（四階 ratchet/怒神加成/跨章重推導）
add_executable(MythLayerTest Examples/MythLayerTest.cpp)
target_include_directories(MythLayerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MythLayerTest Campaign)

# PPAH 和平/崩壞首達時層：無頭驗證 + Monte-Carlo 模擬工具
add_executable(PeaceHazardTest Examples/PeaceHazardTest.cpp)
target_include_directories(PeaceHazardTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PeaceHazardTest Campaign)

add_executable(PeaceDynamicsSim Examples/PeaceDynamicsSim.cpp)
target_include_directories(PeaceDynamicsSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PeaceDynamicsSim Campaign)

# UGSP 第二層：情報不對稱 + 合作複製方程無頭測試
add_executable(GroundStateTest Examples/GroundStateTest.cpp)
target_include_directories(GroundStateTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GroundStateTest Campaign)

# D-2 神社與精靈實體無頭測試（佔領觸發/選擇入帳/favor 回呼接線）
add_executable(ShrineFieldTest Examples/ShrineFieldTest.cpp)
target_include_directories(ShrineFieldTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ShrineFieldTest Campaign)

# D-3 天命兌換無頭測試（灰顯價目/限次/零副作用拒絕）
add_executable(FateExchangeTest Examples/FateExchangeTest.cpp)
target_include_directories(FateExchangeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FateExchangeTest Campaign)

# D-4 神話入侵無頭測試（達閾觸發/稀有性/雙結局/章節旗標）
add_executable(MythIncursionTest Examples/MythIncursionTest.cpp)
target_include_directories(MythIncursionTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MythIncursionTest Campaign)

# D-5 滲透視效無頭測試（等級→指令包/scrim 過渡/降檔/設定相容）
add_executable(SeepageStageTest Examples/SeepageStageTest.cpp)
target_include_directories(SeepageStageTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(SeepageStageTest Gameplay)

# G-4 沙盤模擬器（FR14 平衡工具）：CLI + 無頭測試共用 BattleSandboxCore
add_executable(BattleSandbox Examples/BattleSandbox.cpp)
target_include_directories(BattleSandbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleSandbox Gameplay)

add_executable(BattleSandboxTest Examples/BattleSandboxTest.cpp)
target_include_directories(BattleSandboxTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleSandboxTest Gameplay)

# C# 前端橋：C ABI DLL——無頭 BattleController 部署/執行/二進制快照
# （csharp/MingGoRTS.Client 經 P/Invoke 消費；MinGW 產物名 libPotatoBridge.dll）
add_library(PotatoBridge SHARED Bridge/PotatoBridge.cpp)
target_include_directories(PotatoBridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PotatoBridge PRIVATE Gameplay)
# MinGW：去 lib 前綴 + 靜態連結執行庫——C# DllImport("PotatoBridge")
# 依模組名解析，libPotatoBridge.dll 不命中；且 dotnet 行程 PATH
# 無 MinGW bin，DLL 須自含（否則 0x8007007E）
if(WIN32 AND NOT MSVC)
    set_target_properties(PotatoBridge PROPERTIES
        PREFIX "" OUTPUT_NAME "PotatoBridge")
    target_link_options(PotatoBridge PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()

# E-1 無戰章節門檻判定無頭測試（定義塊/灰顯報表/三路徑入帳/中計）
add_executable(NoBattleResolverTest Examples/NoBattleResolverTest.cpp)
target_include_directories(NoBattleResolverTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NoBattleResolverTest Campaign)

# E-2 首波優勢兌現無頭測試（敵資源增量/我軍士氣減益夾取/邊界）
add_executable(NoBattleAdvantageTest Examples/NoBattleAdvantageTest.cpp)
target_include_directories(NoBattleAdvantageTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NoBattleAdvantageTest Gameplay)

# E-3 顛覆內應無頭測試（倒戈翻面/排序/情報消費/全倒戈自動勝利）
add_executable(SubversionDefectTest Examples/SubversionDefectTest.cpp)
target_include_directories(SubversionDefectTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SubversionDefectTest Campaign)

# G-3 敵將編輯器 headless 半邊：卡目錄驗證 CLI + 無頭測試
add_executable(CardValidator Examples/CardValidator.cpp)
target_include_directories(CardValidator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CardValidator Gameplay)

add_executable(CardValidatorTest Examples/CardValidatorTest.cpp)
target_include_directories(CardValidatorTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CardValidatorTest Gameplay)

# G-3 敵將編輯器存回半邊：編輯 CLI + 無頭測試
add_executable(CardEditor Examples/CardEditor.cpp)
target_include_directories(CardEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CardEditor Gameplay)

add_executable(CardEditorTest Examples/CardEditorTest.cpp)
target_include_directories(CardEditorTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CardEditorTest Gameplay)

# enemy_deck 消費半邊：章節牌組束 → 卡池攤平 → 敵將手牌
add_executable(EnemyDeckTest Examples/EnemyDeckTest.cpp)
target_include_directories(EnemyDeckTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EnemyDeckTest Gameplay)

# C-5 四手結局生成無頭測試（帳目聚合/無字/對手之筆/證詞）
add_executable(EndingPageTest Examples/EndingPageTest.cpp)
target_include_directories(EndingPageTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(EndingPageTest Campaign)

# C-2 敘事內容包無頭測試（載入/locale 降級/壞檔跳過/戰報消費）
add_executable(NarrativePackTest Examples/NarrativePackTest.cpp)
target_include_directories(NarrativePackTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NarrativePackTest Gameplay)

# G-1 doctrine 卡池無頭測試（potato.doctrine/1 載入/驗證/掛鉤加總）
add_executable(DoctrineLibraryTest Examples/DoctrineLibraryTest.cpp)
target_include_directories(DoctrineLibraryTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DoctrineLibraryTest Gameplay)

# G-2 牌庫掠奪無頭測試（signature 卡解鎖/強化/轉資源/存檔聚合）
add_executable(PlayerDeckTest Examples/PlayerDeckTest.cpp)
target_include_directories(PlayerDeckTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PlayerDeckTest Campaign)

# G-5 教學章節無頭測試（tutor 解析/序列驗證/亂序拒絕/pack 文案）
add_executable(TutorialFlowTest Examples/TutorialFlowTest.cpp)
target_include_directories(TutorialFlowTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TutorialFlowTest Campaign)

# E-4 無戰章節敘事與結算無頭測試（和平語域/章尾鉤/結局軌跡）
add_executable(NoBattleNarrativeTest Examples/NoBattleNarrativeTest.cpp)
target_include_directories(NoBattleNarrativeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NoBattleNarrativeTest Campaign)

# B-1 Potato::Log 輕量日誌無頭測試（stderr 捕獲/級別/檔案 sink）
add_executable(LogTest Examples/LogTest.cpp)
target_include_directories(LogTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LogTest PotatoEngine)

# F-1 sprite atlas 索引/uv/降級無頭測試
add_executable(SpriteAtlasTest Examples/SpriteAtlasTest.cpp)
target_include_directories(SpriteAtlasTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SpriteAtlasTest PotatoEngine)

# 斷橋攻防戰即時渲染 → MP4
add_executable(BattleRenderDemo Examples/BattleRenderDemo.cpp)
target_include_directories(BattleRenderDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleRenderDemo Gameplay Media)

# 戰鬥指揮官 RL 訓練：DQN 學 CP 介入時機（無頭,進 POTATO_TESTS）
add_executable(BattleTrainerDemo Examples/BattleTrainerDemo.cpp)
target_include_directories(BattleTrainerDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleTrainerDemo Gameplay ReinforcementLearning NeuralNetwork)

# RL 指揮官訓練 CLI：--episodes/--eval/--seed/--out 可設定,
# 產出 trained_battle_agent.json 供 DuanqiaoPlayable 的 AI 指揮官載入
add_executable(BattleTrainer Examples/BattleTrainer.cpp)
target_include_directories(BattleTrainer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleTrainer Gameplay ReinforcementLearning NeuralNetwork)

# 指揮官契約/消費端無頭測試（不跑長訓練，驗特徵契約與載入接線）
add_executable(BattleCommanderAITest Examples/BattleCommanderAITest.cpp)
target_include_directories(BattleCommanderAITest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BattleCommanderAITest Gameplay ReinforcementLearning NeuralNetwork)

# 時序訓練資料產線（工具性質：需顯示環境,不進 POTATO_TESTS）
add_executable(VideoDataDemo Examples/VideoDataDemo.cpp)
target_include_directories(VideoDataDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VideoDataDemo Gameplay Media glad glfw)

# 天使軍團程序化影片（工具性質：需 GL + ffmpeg,不進 POTATO_TESTS）
add_executable(AngelLegionVideo Examples/AngelLegionVideo.cpp)
target_include_directories(AngelLegionVideo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AngelLegionVideo PotatoEngine glad glfw Media)

add_executable(ModelLoaderTest Examples/ModelLoaderTest.cpp)
target_include_directories(ModelLoaderTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ModelLoaderTest PotatoEngine)

# glTF/VRM 載入管線測試（assets/avatars/ 缺檔時自動略過真檔案例）
add_executable(VrmLoadTest Examples/VrmLoadTest.cpp)
target_include_directories(VrmLoadTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VrmLoadTest PotatoEngine)

add_executable(InputTest Examples/InputTest.cpp)
target_include_directories(InputTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(InputTest PotatoEngine)

add_executable(AudioTest Examples/AudioTest.cpp)
target_include_directories(AudioTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(AudioTest PotatoEngine)

# F-4 miniaudio 後端降級契約測試（ma_backend_null 全無頭;缺檔/無裝置/冪等)
add_executable(MiniaudioBackendTest Examples/MiniaudioBackendTest.cpp)
target_include_directories(MiniaudioBackendTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MiniaudioBackendTest PotatoEngine)

add_executable(TimeTest Examples/TimeTest.cpp)
target_include_directories(TimeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TimeTest PotatoEngine)

add_executable(ImageCodecTest Examples/ImageCodecTest.cpp)
target_include_directories(ImageCodecTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ImageCodecTest PotatoEngine)

# 核心系統無頭煙霧測試：ECS / EventBus / ResourceCache
add_executable(CoreSystemsTest Examples/CoreSystemsTest.cpp)
target_include_directories(CoreSystemsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(CoreSystemsTest PotatoEngine)

# 深度學習繪圖：headless 測試（SR/降噪/上色/INR/地形/法線/LOD）
add_executable(NeuralGraphicsTest Examples/NeuralGraphicsTest.cpp)
target_include_directories(NeuralGraphicsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NeuralGraphicsTest PotatoEngine NeuralNetwork)

# 深度學習繪圖離線 CLI：訓練產出 .pnn，推論讀寫 PNG
add_executable(NeuralArtTool Examples/NeuralArtTool.cpp)
target_include_directories(NeuralArtTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NeuralArtTool PotatoEngine NeuralNetwork)

# assets/neural/ 預訓 .pnn 權重載入 + 推論示範（CPU/headless）
add_executable(NeuralArtDemo Examples/NeuralArtDemo.cpp)
target_include_directories(NeuralArtDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NeuralArtDemo PotatoEngine NeuralNetwork)

# GL 煙霧測試：需要顯示環境,無顯示時自行 SKIP
add_executable(GLSmokeTest Examples/GLSmokeTest.cpp)
target_include_directories(GLSmokeTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GLSmokeTest PotatoEngine glad glfw)

# 人物立繪產生器：VRM/glTF → 離屏渲染 → PNG（需 GL 環境，屬工具非測試）
add_executable(PortraitRenderer Examples/PortraitRenderer.cpp)
target_include_directories(PortraitRenderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PortraitRenderer PotatoEngine glad glfw)

# 合成資料管線：離屏渲染 -> 標註資料集 -> NeuralNetwork 訓練驗證
# （需 GL 環境；無顯示時自行 SKIP，屬測試）
add_executable(SynthDataDemo Examples/SynthDataDemo.cpp)
target_include_directories(SynthDataDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SynthDataDemo PotatoEngine NeuralNetwork glad glfw)

# 2D 分層立繪產生器（純 CPU 光栅；ImGui 只用 font atlas 點陣化，無 GL）
add_executable(PortraitBaker
    Examples/PortraitBaker.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
)
target_include_directories(PortraitBaker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(PortraitBaker PotatoEngine)

# AI Agent GUI 示例 (暫時禁用)
# add_executable(AIAgentGUIExample Examples/AIAgentGUIExample.cpp)
# target_link_libraries(AIAgentGUIExample PotatoEngine)

# 安裝 (暫時禁用)
# install(TARGETS PotatoEngine SimpleExample PlatformTest
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )

# 安裝頭文件
install(DIRECTORY Core/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Rendering/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Physics/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Audio/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Input/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Resources/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ECS/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY GameObject/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Events/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY FileSystem/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Logging/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY MathUtils/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Memory/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Platform/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Scene/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Serialization/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY Time/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")
install(DIRECTORY AI/ DESTINATION include/PotatoEngine FILES_MATCHING PATTERN "*.h")

# ============================================================================
# CTest - 非互動測試可用 ctest 一鍵執行
# ============================================================================
enable_testing()

set(POTATO_TESTS
    MinimalTest
    MathTest
    PhysicsTest
    PhysicsMathTest
    FileSystemTest
    LoggerTest
    MemoryTest
    PlatformTest
    SceneTest
    SerializationTest
    RenderPipelineTest
    BattleSceneTest
    SquadFatigueTest
    ModelLoaderTest
    VrmLoadTest
    InputTest
    AudioTest
    MiniaudioBackendTest
    TimeTest
    ImageCodecTest
    CoreSystemsTest
    NeuralGraphicsTest
    GLSmokeTest
    SecurityRedTeamTest
    ContentScannerTest
    SecurityPipelineTest
    AuditLedgerTest
    SecurityDataTest
    AntiTamperTest
    JobSystemTest
    ProfilerTest
    SignedSaveFileTest
    NetworkingTest
    TweenTest
    SpatialPathTest
    FileWatcherTest
    ParticleSystemTest
    AITestSuite
    DevAssistantSmoke
    SuggestionRulesTest
    AIAgentSmoke
    DoctrineBattleDemo
    AutoPlannerDemo
    BattleTrainerDemo
    BattleCommanderAITest
    DuanqiaoDemo
    SageCommandTest
    QuantumTest
    QuasiRandomTest
    QuasiModelsTest
    UnsolvedBatch3Test
    QuantumFogBattleTest
    QuantumBench
    TutorialScriptTest
    BattlePickerTest
    PlanningDeckTest
    TerrainBonusTest
    PostBattleTest
    MapGenTest
    JammingBattleTest
    SquadTemplateTest
    GeneralGuardTest
    RefitCampTest
    BattlePlanTest
    QuantumPlanTest
    MediaPipeTest
    UIThemeTest
    UIScaleTest
    HUDDensityTest
    RenderTargetTest
    HistorianReportTest
    RivalDeckTest
    CampaignLedgerTest
    GeneralDossierTest
    CampaignStateTest
    ChapterLibraryTest
    LogTest
    SpriteAtlasTest
    SynthDataDemo
    MythLogTest
    ChapterConventionsTest
    LedgerChainTest
    LedgerAuditTest
    ReplayIntegrityTest
    LedgerForgeTest
    LedgerProvenanceTest
    LedgerRegistryTest
    LedgerAssuranceTest
    GovernanceTest
    GovernanceFieldTest
    MythLayerTest
    PeaceHazardTest
    GroundStateTest
    ShrineFieldTest
    FateExchangeTest
    MythIncursionTest
    SeepageStageTest
    BattleSandboxTest
    NoBattleResolverTest
    NoBattleNarrativeTest
    NoBattleAdvantageTest
    SubversionDefectTest
    CardValidatorTest
    CardEditorTest
    EnemyDeckTest
    EndingPageTest
    NarrativePackTest
    DoctrineLibraryTest
    PlayerDeckTest
    TutorialFlowTest
)
foreach(test_name IN LISTS POTATO_TESTS)
    if(TARGET ${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
        # 測試需要同目錄的 DLL(如 FakeCheat.dll),工作目錄設為 bin
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
endforeach()

# CPack 打包配置
set(CPACK_PACKAGE_NAME "PotatoEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "完全獨立的遊戲引擎")
set(CPACK_PACKAGE_VENDOR "Potato Engine Team")
set(CPACK_GENERATOR "ZIP;TGZ")

include(CPack)

endif() # STANDALONE