# MingGoRTS 技術基線

本文件記錄目前由原始碼與 CMake targets 證實的技術，並將已實作與規劃中能力分開。若本文與舊版 PotatoEngine 概覽或架構草案衝突，以程式碼、`CMakeLists.txt`、此基線及 `_bmad-output/` 的當前規劃為準。

## 已實作

| 領域 | 目前技術與邊界 |
| --- | --- |
| 語言與建置 | C++20、CMake、MSVC / GCC；CTest 管理測試。 |
| 引擎渲染 | `PotatoRendering` 靜態庫；OpenGL 3.3、GLFW、預生成 GLAD 與 Dear ImGui。`RenderingServer` 提供 GL 與 Dummy rasterizer 路徑；headless 測試不需要建立 GL context。 |
| 遊戲分層 | `PotatoGameplay` → `PotatoCampaign` → `PotatoGame`。Gameplay 不連結引擎渲染庫，保持 headless；呈現與遊戲外殼在上層。 |
| 戰鬥模擬 | 固定 20 Hz、整數／定點運算、固定種子的 PRNG 與明確排序；模擬 tick 不執行檔案 I/O。LOD importance/capacity policy 已在 Gameplay 實作，尚不改變戰鬥 tick 或 replay 格式。 |
| 內容與存檔 | 遊戲層使用自建 `Gameplay/Json/JsonValue`；內容與存檔使用版本化 `potato.<name>/<version>` schema，寫檔採暫存檔再 rename。 |
| AI 與工具 | 引擎有獨立 AI/工具 targets，包括 `ToolFramework`、`AgentChain`、LLM 整合與 `RenderAgentTools`。它們是引擎能力，並不代表戰鬥模擬依賴 LLM 或非決定性 AI。 |
| 測試 | CTest 包含 headless Gameplay/Campaign 測試、引擎測試及 CLI record/verify 驗收；CI 另執行語法、安全與依賴方向檢查。 |

## 目前不應宣稱為已採用

- Vulkan、DirectX、PBR、光線追蹤、DLSS、Bullet 與 OpenAL 不屬於目前已驗證的 MingGoRTS 執行技術基線。
- StonePotatoes 的 Vulkan 編輯器與其 C++ SDK 不會因為同一開發者維護而自動成為 MingGoRTS 依賴；跨 repo 共用前需先定義 ABI/API、版本與安裝後 consumer 驗收。
- Campaign world-agent LOD identity/persistence 屬於後續故事與 PR 工作；合併前不能描述為 main 已具備。

## 技術演進原則

1. 保持 `Engine ← Gameplay ← Campaign ← Game` 單向依賴；新遊戲規則先進 headless Gameplay。
2. 新增模擬功能須維持 20 Hz 確定性與 replay 相容性，並以固定種子測試證明。
3. 先以 CMake library/test target 落地一個垂直切片，再更新此基線與 README；避免僅在舊架構草案中標記功能完成。
4. Renderer 或編輯器跨專案重用時，先做獨立安裝 consumer 驗收，不直接把 repo 內部路徑當公開 API。

## 驗證來源

- 根目錄 `CMakeLists.txt` 的實際 targets 與連結關係。
- `Rendering/RenderingServer.h`、`Gameplay/` 與 `Campaign/` 的實際實作。
- `docs/architecture/SIMULATION_LOD.md` 及 `_bmad-output/implementation-artifacts/sprint-status.yaml` 的目前規劃／審查狀態。
