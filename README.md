# MingGoRTS（民國史詩 RTS）

**回合制 RPG × 即時戰略的 hybrid 戰術遊戲，執行於自研 PotatoEngine（C++20）**

玩家在戰鬥中不點選單位——而是**書寫**單位。每場戰鬥前編排 doctrine 卡
（trigger → condition → action）、部署具名小隊，然後看雙方腳本即時執行，
僅以稀少的指揮點（CP）在計畫崩潰時介入數秒。

戰役圍繞兩個哲學軸：

- **至聖者無戰**——不戰而屈人之兵：倒戈優於殲滅、嚇阻優於接戰，
  指定章節可完全不戰而勝。
- **治平者無勝**——軍事勝利不是目的，治理才是：民心／秩序是
  跨戰鬥的持久軸，戰場敗北仍可轉化為治理勝利。

每場戰鬥跑在同一張地圖的兩個層面上：**歷史層**（縱隊、渡口、糧道、
電報線）與**神話層**（土地神、狐仙、戰神附體、怨靈鬼軍）。神話行為
是民心軸的主要放大器——安撫神社直接轉化為下一場戰鬥的治理資本。

---

## 架構分層（規劃）

```
PotatoEngine  ←  Gameplay   ←  Campaign   ←  Game（外殼）
（引擎）          （單場戰鬥）   （跨章節持久）  （呈現）
```

依賴方向單向：引擎不依賴玩法層；玩法層不依賴戰役層；
`Gameplay` 全部 headless 可測（無 OpenGL context 需求）。

> 遊戲層為 greenfield——`Gameplay/`、`Campaign/`、`Game/`、`assets/`
> 尚未建立，由 Epic 1 Story 1.1 起實作。詳見
> `_bmad-output/game-architecture.md`。

### 目錄

```
MingGoRTS/
├── Core/ Rendering/ Physics/ Audio/ Input/   # PotatoEngine 子系統
├── Resources/ ECS/ GameObject/ Events/
├── FileSystem/ Logging/ MathUtils/ Memory/
├── Platform/ Scene/ Serialization/ Time/ Security/
├── AI/                  # AI Agent / 強化學習（DQN）/ 神經網路
├── GUI/                 # ImGui 介面系統
├── MingGoRTS_IDE/       # 整合開發環境（智能建議含專案規範護欄）
├── Examples/            # 可執行 demo／測試（各自獨立 target）
├── external/            # vendored 第三方（勿改既有內容）
├── _bmad-output/        # 規劃產物（GDD/架構/敘事/epics/sprint 狀態）
└── docs/                # 引擎指南、架構、研究文件
```

---

## 建置與測試

### 需求

- CMake ≥ 3.15、C++20 編譯器
- Windows：MSVC（VS Developer Prompt）或 MinGW
- Linux：g++
- OpenGL 3.3+；GLFW 由 FetchContent 自動拉取
- **不需要** vcpkg／Bullet／OpenAL——第三方僅 `external/` vendored

### Windows

```bat
BuildEngine.bat          :: 一鍵建置入口（VS 2022，Release；需 VS Developer Prompt）
```

或手動：

```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

MinGW：

```bash
cmake -B build-mingw -S . -G "MinGW Makefiles"
cmake --build build-mingw
```

### 測試約定

- 目前**無統一測試入口**（無 CTest）——`Examples/` 下的測試程式是
  獨立可執行 target，逐一建置執行
- 遊戲層測試採 `potato_test_<name>` 命名（`POTATO_TESTS` CMake
  option 為規劃項，尚未落地）；玩法測試一律 headless——不得依賴
  GL context

---

## 開發規範（詳見 AGENTS.md）

- 不直接推 `main`/`develop`——一律走 PR，Conventional Commits
- 不修改 `external/` 既有內容；新第三方僅以「新增子目錄＋README」引入
- 不引第三方 JSON 庫；遊戲層 JSON 走自建 `JsonValue`（`Gameplay/Json/`，
  Epic 1.2）；檔案 schema 版本化 `potato.<name>/<ver>`
- 禁用 C 函式 `gets|strcpy|strcat|sprintf|vsprintf|scanf`；
  用 `strncpy`/`snprintf` 等安全替代
- 本地驗證需 MSVC **與** MinGW 雙過；MinGW 專用連結用
  `if(WIN32 AND NOT MSVC)` 守衛
- 存檔寫入一律 tmp＋rename 原子路徑；壞 schema/版本 → 拒絕不動現況
- 模擬層決定性：固定 20 Hz、整數/定點、單一 seeded PRNG；
  `Gameplay/` 無 Rendering/GUI 依賴；tick 路徑無檔案 I/O

## 文件索引

- [GDD](_bmad-output/planning-artifacts/gdds/gdd-MingGoRTS-2026-09-29/gdd.md)——完整遊戲設計
- [技術架構](_bmad-output/game-architecture.md)——分層、橫切面、實作模式
- [敘事設計](_bmad-output/narrative-design.md)——章回結構、四聲部結局、雙層世界
- [Epics & Stories](_bmad-output/planning-artifacts/epics.md)——9 epics / 61 stories
- [Sprint 狀態](_bmad-output/implementation-artifacts/sprint-status.yaml)——實作追蹤
- [實作就緒報告](_bmad-output/planning-artifacts/implementation-readiness-report-2026-09-29.md)
- [docs/](docs/index.md)——引擎指南、架構、研究報告

---

**MingGoRTS — 以筆代兵，以治為勝**

*狀態：規劃完成 · GDD／架構／敘事／epics（61 stories）就緒 ·
實作就緒評估 READY · 待啟動 Epic 1（Battle Core）*
