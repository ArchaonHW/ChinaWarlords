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

## 架構分層

```
PotatoEngine  ←  Gameplay   ←  Campaign   ←  Game（外殼）
（引擎）          （單場戰鬥）   （跨章節持久）  （呈現）
```

依賴方向單向：引擎不依賴玩法層；玩法層不依賴戰役層；
`Gameplay` 全部 headless 可測（無 OpenGL context 需求），
由 `scripts/CheckGameplayDeps.ps1` 在 CTest 中強制檢查禁入標頭。

四層皆已落地：`Gameplay/`（Sim/Doctrine/Fog/Json/…）、
`Campaign/`（Ledger/Myth/Narrative/Save/…）、`Game/`（Hud/Render/Shell）、
`assets/`（bencao、doctrine、encounters、narrative、worlds 等 JSON 內容）。
`Game/Render` 使用 indexed-color `SpriteAtlas` 與 headless RGBA8 `Framebuffer`；
tile blit 先裁到可見畫面範圍，動畫則由明確整數 tick 推進，不改變確定性戰鬥模擬。

### 目錄

```
MingGoRTS/
├── Core/ Rendering/ Physics/ Audio/ Input/   # PotatoEngine 子系統
├── Resources/ ECS/ GameObject/ Events/
├── FileSystem/ Logging/ MathUtils/ Memory/
├── Platform/ Scene/ Serialization/ Time/ Security/
├── AI/ Networking/ Media/ Quantum/ Simulation/
├── GUI/                 # ImGui 介面系統
├── Gameplay/            # 遊戲層：headless 模擬（禁 Rendering/GUI 依賴）
├── Campaign/            # 遊戲層：跨章節持久狀態
├── Game/                # 遊戲層：呈現殼（Render headless、Shell 為 GL presenter）
├── assets/              # 版本化 JSON 遊戲內容（potato.<name>/<ver>）
├── MingGoRTS_IDE/       # 整合開發環境
├── Examples/            # 可執行 demo／測試（potato_test_*、工具 CLI）
├── Tools/               # MintRelayer、PotatoEngine 上游同步腳本
├── java/                # agent-service / api-gateway / discovery-server
├── external/            # vendored 第三方（勿改既有內容）
├── scripts/             # CheckGameplayDeps.ps1 等檢查腳本
├── .github/ .githooks/  # CI workflows、pre-commit/commit-msg hooks
└── docs/                # 引擎指南、架構、研究文件
```

---

## 建置與測試

### 需求

- CMake ≥ 3.15、C++20 編譯器
- Windows：MSVC（VS 2022 或 Build Tools）
- Linux：g++（CI 於 ubuntu-latest 建置通過）
- OpenGL 3.3+；**glad 為預生成產物**（`external/glad_gen`），
  glfw 優先使用 `external/glfw` vendored 副本，不存在時才走 FetchContent
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

Headless 建置（無 glfw/glad 依賴，適合 MinGW、CI、非 ASCII 路徑環境）：

```bash
cmake -B build -S . -DPOTATO_BUILD_GUI=OFF
cmake --build build --config Release
```

> 注意：`PotatoEngine` 靜態庫目前僅 MSVC 可編（`Security/` 使用 MSVC 專用
> 寫法）；`PotatoGameplay` 刻意不連結引擎靜態庫，headless 遊戲層可在
> MinGW 下建置。MinGW 專用連結以 `if(WIN32 AND NOT MSVC)` 守衛。

### Linux（Ubuntu 24.04 驗證）

```bash
sudo apt-get install -y cmake g++ libglfw3-dev libgl1-mesa-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

中文字型自動偵測（`Platform/SystemFonts.h`）：Windows 標楷體／微軟正黑體 →
Linux Noto CJK（`fonts-noto-cjk`）、文鼎、文泉驛 → macOS PingFang；
可用 `POTATO_CJK_FONT`（及 `POTATO_CJK_FONT_BOLD`）指定字型檔。
IDE 工作區：`--workspace <dir>` → `MINGGORTS_WORKSPACE` → 自執行檔／目前目錄向上尋找 repo 根目錄。

### 測試

```bash
cd build
ctest -C Release --output-on-failure
```

- CTest 共 44 個測試（Linux）及 45 個測試
  （Windows，另含 `gameplay_dep_guard`）：`potato_test_*` 單元測試、
  record/verify CLI e2e、引擎測試（RenderingServer、LLMToolBridge、Zeta 等）。
  測試數以 `ctest -N` 顯示為準；Windows CTest 會為每個測試隔離 TEMP/TMP，
  並以程序限定的 PowerShell execution-policy override 執行依賴守衛。
- CI（`pre-commit-checks.yml`，ubuntu-24.04）每次 push／PR 皆建置並跑 ctest
- `-DPOTATO_TESTS=OFF` 可略過全部測試 target 加速純庫建置
- 玩法測試一律 headless——不得依賴 GL context

---

## 開發規範（詳見 AGENTS.md）

- 不直接推 `main`/`develop`——一律走 PR（branch protection 已啟用，
  需通過 required checks），Conventional Commits（`commit-msg` hook +
  CI 雙重檢查）
- 不修改 `external/` 既有內容；新第三方僅以「新增子目錄＋README」引入
- 不引第三方 JSON 庫；遊戲層 JSON 走自建 `JsonValue`（`Gameplay/Json/`）；
  檔案 schema 版本化 `potato.<name>/<ver>`
- 禁用 C 函式 `gets|strcpy|strcat|sprintf|vsprintf|scanf`；
  用 `strncpy`/`snprintf` 等安全替代（pre-commit hook 與 CI 皆掃描）
- 存檔寫入一律 tmp＋rename 原子路徑；壞 schema/版本 → 拒絕不動現況
- 模擬層決定性：固定 20 Hz、整數/定點、單一 seeded PRNG；
  `Gameplay/` 無 Rendering/GUI 依賴；tick 路徑無檔案 I/O
- 引擎子系統改動會觸發 Upstream Mirror 檢查——`Tools/sync-potatoengine-upstream.sh`
  同步至 `ArchaonHW/PotatoEngine` 鏡像，漂移即 CI 失敗

## 文件索引

- [docs/](docs/index.md)——引擎指南、架構、研究報告
- [技術基線](docs/architecture/TECHNICAL_BASELINE.md)——目前已驗證的技術與依賴邊界；舊版引擎概覽中的方向性方案不代表已採用

以下規劃產物為**本地開發文件**（`_bmad-output/` 刻意不進版控，
clone 後不會出現；僅供持有該目錄的開發環境參考）：

- GDD：`_bmad-output/planning-artifacts/gdds/gdd-MingGoRTS-2026-09-29/gdd.md`
- 技術架構：`_bmad-output/game-architecture.md`
- 敘事設計：`_bmad-output/narrative-design.md`
- Epics & Stories：`_bmad-output/planning-artifacts/epics.md`
- Sprint 狀態：`_bmad-output/implementation-artifacts/sprint-status.yaml`

---

**MingGoRTS — 以筆代兵，以治為勝**

*狀態：實作進行中 · 四層架構（Engine/Gameplay/Campaign/Game）已落地 ·
44 項 CTest 已註冊（含 headless 玩法測試）· CI（syntax/security/build/ctest/mirror）運作中*
