# 遊戲主畫面 UI/UX 設計規劃

> 狀態:規劃稿(2026-10-12)。範圍:`potato_game` shell 的完整畫面流程——
> 標題 → 指揮官創建 → 大地圖 → 戰鬥(規劃/執行/戰報) → 結算/結局。
> 本文同時是 Epic 13(Shell UX)的工項拆解依據。

---

## 0. 現況盤點

`potato_game` 目前是開發者級 ImGui:進程直接落在「指揮官創建 → 大地圖
+ Orders 浮動窗」(`Game/Shell/main.cpp`),無標題畫面、無讀檔、無戰鬥
畫面串接。已具備的地基:

| 已有 | 位置 | 對應畫面 |
|---|---|---|
| 指揮官創建 view-model | `Game/Ui/CreationForm.h` | S1 |
| 行軍令 view-model | `Game/Ui/OrderMenu.h` | S2 |
| 大地圖光柵 | `Game/Render/OverworldView.h` | S2 |
| 戰場光柵(含 fog cloud) | `Game/Render/BattleView.h` | S3 |
| HUD widget model + hit-test | `Game/Hud/HudView.h` | S3 |
| 戰報史官體渲染 | `Campaign/Narrative/BattleReport.h` | S3c |
| 結局冊頁(信史/疑史/偽史/絕筆) | `Campaign/Narrative/EndingPage.h` | S4 |
| 存檔槽系統(tmp→rename) | `Campaign/Save/SaveSystem.h` | S0/覆層 |
| 章節/遭遇結算儀式 | `Campaign/Aftermath/Aftermath.h` / `Campaign/World/EncounterSettle.h` | S3d→S2 |

**三個必須先補的洞**:

1. **ImGui 未載入 CJK 字體** — 目前所有中文(地名、武將名、戰報全文)
   顯示成方框。
2. **Framebuffer 無文字繪製** — 文字全走 ImGui;若要像素級一致的主
   畫面需加 bitmap-font 渲染器,或接受「光柵底圖 + ImGui 文字層」混合。
3. **無畫面狀態機** — `main.cpp` 用 `commanderPicked` bool 切兩個畫面,
   需正規化為 screen flow。

## 1. 設計原則

- **書寫而非點選**:UI 重心是閱讀與編排(部署、卡槽、箭頭),不是微
  操;介入動詞刻意稀少且昂貴。
- **冊頁隱喻**:視覺語言走 1920s 公文/軍用地圖/電報紙——紙底色、鉛
  印字、紅色批註。戰報直接是一頁史官體文字,不是數字表格。
- **確信度即畫面**:truth boundary 延伸到 UI——敵軍永遠渲染為 fog
  cloud(alpha ∝ certainty),大地圖只渲染 hearsay sighting,UI 層拿
  不到真值。
- **Headless-first 沿用既有模式**:每個畫面 = 純函數
  `BuildXLayout(Input) → widget 模型(hit rect + enabled)` +
  `RenderX() → Framebuffer`;ImGui/GL 只在 `Game/Shell` 當輸入皮。這讓
  每個畫面都能進 `potato_test_*`。
- **密度三檔**沿用 `HudDensity::{Minimal,Standard,Full}` 作為全域
  UI 縮放設定。
- **狀態機照 `BattleBeat` 慣例**:`enum class ScreenId` + 明確
  transition table,非法跳轉拒絕且無副作用。

## 2. 畫面流程

```
                ┌──────────────────────────────────────────┐
                ▼                                          │
   S0 標題 ──新戰役──▶ S1 指揮官創建 ──▶ S2 大地圖 ◀──┐     │
      ▲  │繼續戰役(讀檔槽)                  │  ▲          │     │
      │  └──────────────(有存檔)──────────▶ │  │結算返回   │     │
      │                                     ▼  │          │     │
      │                                  S3 戰鬥          │     │
      │                                   ├ a 規劃 Planning      │
      │                                   ├ b 執行 Execution     │
      │                                   └ c 戰報 Aftermath     │
      │                                     │  │          │     │
      │            最終章節結算              ▼  ▼          │     │
      └────────────── S4 結局冊頁 ◀── S3d 結算摘要 ───────┘     │
                                                                 │
   全域覆層:Esc → 暫停/設定選單(任何畫面,Execution 中暫停只停呈現)─┘
```

邏輯解析度沿用現行 **960×600**,window 等比縮放(GlShell 已是
nearest-blit)。

### S0 標題畫面

- 中央:「ChinaWarlords／民國史詩（暫定）」標題 + 副標;背景用 `RenderOverworld` 渲染最後存檔
  (或 demo world)作為環境底圖——進遊戲第一眼就是這個世界。
- 選單:**繼續戰役**(列出 `SaveSystem::ListSlots()`,顯示指揮官名/天
  數,空槽 disabled)、**新戰役**、**設定**、**離開**。
- 無存檔時「繼續」整項 disabled(沿用 `HudWidget.enabled` 慣例:變暗
  不消失)。

### S1 指揮官創建(現有,UX 升級)

- 左欄:roster 卡片列(名 + prior 標籤 + 一個 sprite tile);右欄:自訂
  表單(姓名/籍貫/prior 三選/牌組種子收進「進階」摺疊)。
- 底部:**起始 doctrine 牌組預覽**——玩家選的是會打出什麼卡的人,不
  是只有名字。
- 驗證錯誤行內顯示:`BuildCharacter` 的 `Result.reason` 已指名失敗欄
  位,直接 echo 到該欄位下方。

### S2 大地圖主畫面

- 全屏地圖(framebuffer),無常駐窗格;chrome 收到邊緣:
  - **頂欄**:`Day N` · 現駐節點名 · 糧秣結餘 · 民心/秩序量條 · 右端
    選單鈕。
  - **右抽屜**(Tab 開闔,分頁):`行軍令`(`MarchOptions` 列表,含天
    數/耗糧,不可負擔的 disabled)、`遭遇`(`PendingEncounters`)、
    `情報`(IntelLedger)、`名冊`(Roster)、`帳冊`(Ledger)、`本草`
    (BencaoCodex,已解鎖條目)。
  - **點節點下軍令**:點相鄰節點 → 浮出確認卡(天數/耗糧) → 確認才
    `IssueMarch`。行軍中頂欄顯示 `→ 目的地 (eta day N)`。
- 快捷鍵:`Tab` 抽屜、`Space` 推進一日、`Esc` 覆層。

### S3 戰鬥(三節拍,HUD gating 照 `BattleBeat`)

**a. Planning** — 編排畫面:

- 左 dock:我方小隊列(`SquadHeader` cells),每隊展開 doctrine 卡槽
  (`CardTile`)+ 冷卻條。
- 中央地圖:部署 ghost 預覽、`DrawArrow` 路線編排(連點相鄰 region 成
  鏈)、convoy 路線工具、已播種的滲透等級標記。
- 敵情欄:只顯示 fog 視角的 believed clouds;`SetCloudIntel` 供
  briefing 注入戰前情報。
- 頂部節拍帶 Planning→Execution;「開戰」按鈕需二次確認
  (`RequestBeat` 不可逆)。

**b. Execution** — 觀戰 + 稀少介入:

- 左上:CP pips(滿格 `CP_CAP=5`)+ regen 細條;probe 預算
  pips(3/場)。
- 右下動詞盤:`redirect(1) override(2) retreat(3) probe(0)
  entangle(2) replan(N) execute(0)` + 神話動詞
  `myth_pacify/possession/ghost_army`——神話動詞掛**天命**籌碼標而非
  CP(由 campaign ledger 支付)。
- 操作流程:點己方小隊 → 點動詞 → 點目標 region/小隊;
  `Retreat`/`Execute` 走兩段確認(execute 產生 atrocity deed,是帳面
  代價)。
- tick 時鐘顯示;暫停只停呈現層 tick pump,不進 sim。

**c. Aftermath** — 戰報獨佔畫面:`RenderBattleReport` 整頁史官體(含
省略告白行 `本報告省略 N 項`、書吏按 colophon);底部 `結算 →`。

**d. 結算摘要**(S3c→S2 之間的一條 strip):裁決(ledger 決定,非場面
結果)、roster 應用/埋葬/晉升、deeds 入帳數——敗戰同樣推進
(converted defeat,無 game over)。

### S4 結局冊頁

最終章節結算後:`RenderEndingPage` 四聲部冊頁(信史/疑史/偽史/絕筆),
讀完回標題。

### 全域覆層

`Esc` 於任何畫面浮出:存檔(僅 S2/S3a 可用——tick 路徑零 I/O,
Execution 中禁止存檔)、讀檔、設定(音量/解析度/HudDensity)、回標
題、離開。

## 3. 工項拆解(Epic 13 — Shell UX)

依賴序排列;每項給出驗收標準(acceptance = 對應 `potato_test_*` 或手動
驗證點)。

| # | Story | 內容 | 驗收 |
|---|---|---|---|
| 13.1 | **CJK 字體管線** | GlShell 載入 CJK 字型進 ImGui atlas(`AddFontFromFileTTF` + `GetGlyphRangesChineseSimplifiedStandard`/自訂範圍),fallback 處理缺字 | `potato_game` 內中文不再顯示方框 |
| 13.2 | **ScreenFlow 狀態機** | `Game/Ui/ScreenFlow.h`:`enum class ScreenId` + `CanTransition` + 各畫面 Input struct;純 headless | `potato_test_screenflow`:全合法/非法轉移表 |
| 13.3 | **S0 標題畫面** | `BuildTitleLayout`(選單 widget + hit rect)+ `TitleScreen` view-model(接 `SaveSystem::ListSlots`,空檔 disabled)+ shell 渲染 | `potato_test_title`;手動:背景底圖渲染 |
| 13.4 | **S2 大地圖正規化** | 頂欄狀態條 widget(day/節點/糧秣/民心秩序)+ 右抽屜分頁 model(六分頁的 view-model 接既有 ledger/intel/roster/codex API)+ 點節點兩段式軍令 | `potato_test_overworld` 擴充;`MarchOptions` 沿用不改 |
| 13.5 | **S1 創建畫面正規化** | roster 卡片列 + 欄位錯誤行內 echo + 起始牌組預覽(接 `DoctrineLibrary`)| `potato_test_creation` |
| 13.6 | **S3 戰鬥串接** | shell 接上 `BattleController`:encounter 觸發 → `BattleView`+`HudView` 渲染 → Planning 輸入(DeploySquad/SetSheet/DrawArrow)→ `RequestBeat` → Execution tick pump(20 Hz 呈現節拍,呼 `Tick`)→ Aftermath | 手動端到端;`potato_test_battle` 驗 layout gating |
| 13.7 | **S3c 戰報 + 結算頁** | 報告頁呈現(ImGui 文字層,CJK)+ 結算摘要 strip + `SettleEncounter`/`ResolveAftermath` 接線 | 一場戰鬥打完回大地圖;敗戰不當機重觸發 |
| 13.8 | **Esc 覆層 + 存讀檔** | 全域 pause overlay view-model;存檔僅 S2/S3a 允許;讀檔走 `SaveSystem::Load` 重建 `WorldState` | 存→關→讀→狀態一致;tmp 檔原子性沿用 |
| 13.9 | **S4 結局冊頁** | `RenderEndingPage` 呈現 + 回標題轉場 | 最終章節 → 冊頁 → S0 |

**之後可加(非本期)**:像素級 widget 渲染器(bitmap font 進
framebuffer,讓 chrome 也 deterministic)、戰鬥覆盤 replay viewer、鍵盤/
手把全操作支援。

## 4. 決策點(實作前需拍板)

1. **文字層路線**:短期「framebuffer 底圖 + ImGui 文字層」(快,13.1
   即可解)vs. 長期 bitmap font 進 framebuffer(全幀 deterministic,
   HUD hash 可釘)。建議先前者、後者列入 backlog。
2. **輸入層**:widget hit-test(`HudLayout::Hit` 模式,可測)為主;
   S1 文字輸入保留 `ImGui::InputText`(自造文字框成本高)。
3. **Epic 編號**:code comments 目前引用到 Epic 12(Story 12.9 shell、
   12.5 encounter settle);本批記為 **Epic 13**。`_bmad-output` 規劃文
   件已不存在於 repo,epic/story 追蹤改落在本文檔。

---

**🥔 ChinaWarlords（民國史詩，中文名暫定）文檔**
