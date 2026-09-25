# Deferred Work

Findings deferred from `spec-ide-dev-assistant` review (iteration 1). All items below are in files outside that spec's scope — they belong to concurrent work in the worktree (rendering, physics, input, platform, audio, security, serialization changes) or need harnesses that do not exist yet. Triaged from blind-hunter, edge-case-hunter, and verification-gap review layers.

## Bugs / correctness (other modules)

- [x] `Rendering/ModelLoader.cpp` — face index resolution now bounds-checks all three index fields (`f 9999/1/1` skipped), `-INT_MIN` handled via `long long` widening before negation; verified by `ModelLoaderTest`
- [x] `Rendering/ModelLoader.cpp` — quad/ngon faces fan-triangulated; `tangent`/`bitangent` computed per-vertex in `OptimizeMeshes` (degenerate UV determinant skipped)
- [x] `Platform/PlatformSystem.cpp` — `Wait`/`WaitFor` call `lock.release()` after wait so `unique_lock` dtor no longer steals the mutex; PlatformTest exercises Wait/Notify handoff + WaitFor timeout
- [x] `Platform/GLFWSharedContext.cpp` — `installed` flag + user-pointer validation in `GetOrCreateGLFWContext` detects recycled-address stale entries; new `DestroyGLFWWindow()` is the only sanctioned destroy path (used by `GLFWWindow::Shutdown` and `OpenGLRenderer::ShutdownGLFW`); `Main.cpp` documented as not using engine dispatch
- [x] `Input/InputManager.cpp` — `GLFWKeyToKeyCode` completed (punctuation, World1/2, F13–F25, full numpad); `KeyCodeToGLFWKey` maps World1/World2; `keyStates`/`mouseButtonStates` integrated as headless fallback when no window handle; `InputTest` covers OnKeyEvent + 26-pair bidirectional mapping
- [x] `Physics/PhysicsSystem.cpp` — kinematic semantics unified: `IsKinematic()` returns `kinematic || bodyType == Kinematic`; `ApplyForce`/`ApplyTorque`/`ApplyImpulse`/`IntegrateVelocity`/separation all respect it; `bodyB->collisionCallback` fired with flipped `otherBodyID`
- [x] `Audio/AudioSystem.cpp` — `LoadWAV` validates RIFF/WAVE/fmt magic, caps payload at 512MB, checks read result; `AudioTest` covers tiny/non-RIFF/non-WAVE/valid cases
- [x] `Serialization/Serialization.cpp` — `GetSaveSlots` only lists stems that pass sanitization unchanged (round-trip guaranteed); `is_regular_file(entryEc)` non-throwing; `SanitizeSlotName` shared with `GetSaveSlotPath`
- [x] `Rendering/Camera.cpp` — `SetViewport` guards `width > 0 && height > 0`; covered by SceneTest
- [x] `Examples/SecurityRedTeamTest.cpp` — restore-path `VirtualProtect` result checked before write (no AV); `spawnTool(pi2)` failure now `Fail`s instead of silently skipping; `Sleep(100)` race replaced with 2s polling loop on `CheckThreadContexts`

## Missing verification (other modules)

- [x] Input: `InputTest` — headless `GLFWInputManager`, `OnKeyEvent(ESCAPE)` → `KeyCode::Escape`, 26-pair bidirectional map, unknown-key safety
- [x] OBJ parser: `ModelLoaderTest` — all four face formats, negative indices, OOB + INT_MIN, quad triangulation, tangent/bitangent direction
- [x] `PhysicsTest` — added: both collision callbacks, destroy-inside-callback, kinematic-bool force immunity, zero-mass pair, coincident overlap, restitution velocity assertion, non-sphere `BoundingRadius` broadphase
- [x] `SceneTest` — rotated-parent case (90° Y) asserts child world position via `TransformPoint`; Camera viewport guard checks added
- [x] `LoadWAV` hardening — `AudioTest` malformed-file cases
- `CheckExternalHandles` system-dir auto-trust direction unpinned (needs real system-dir process holding a handle — not hermetic).
- [x] GL-context-dependent changes — `GLSmokeTest` covers `VertexArray` lazy create (construct + empty `Draw` before context), `Mesh::Draw` non-indexed/indexed/instanced paths, `ShouldClose` null guard; self-SKIPs on headless. (`AdvancedShader` reload no longer exists — stale item.)
- [x] `TimeManager` paused-FPS — `TimeTest` pins `UpdateFPS(rawDeltaTime)`: paused → deltaTime/totalTime frozen but real FPS still reported. (`DashboardPanel` no longer exists — stale item.)
- [x] IDE dev-assistant write-back seam — `WriteGenerationResult` extracted to `GuiTextUtils.h` (UI-free); `PollDevelopmentResult` uses it; `DevAssistantSmoke` covers success/failure write-back paths.

## Cleanup / leftovers

- [x] `CMakeLists.txt` — commented-out `# add_executable(PlatformTest …)` removed (real target kept above)
- [x] `Resources/ResourceManager.cpp` — stale comment updated to `ResourceCacheHandle 實現`
- [x] `Mesh::Draw`/`DrawInstanced` — early-return when both buffers empty; no longer binds/creates VAO pointlessly
- [ ] `_bmad-output/brainstorming-session-2026-09-16.md` — skipped: file has unstaged user edits (still in use?)
- [x] Untracked root `nul` artifact — already absent (verified via `\\?\` path)

- source_spec: `_bmad-output/implementation-artifacts/spec-intelligent-suggestions.md`
  summary: Run intelligent-suggestion analysis off the render thread / cache compiled regexes — GenerateSuggestions constructs std::regex objects per call and scans the whole 8KB buffer synchronously each quiet-period.
  evidence: Blind Hunter review — std::regex built per call (e.g. magicNumberRegex in GetBestPracticeRecommendations); bounded but can hitch a frame on each analysis pass.
  resolution: 已全修（2026-09-17）：regex 改 function-local static const 快取；分析搬離 render thread——debounce 滿 500ms 後 `std::async` 背景跑 GenerateSuggestions，render thread 只輪詢 future；過期結果以 inflight-hash 丟棄、LearnFromFeedback 與 worker 以 `g_SuggestionMutex` 互斥、Shutdown 有界等待 2s 後才 reset。
- source_spec: `_bmad-output/implementation-artifacts/spec-intelligent-suggestions.md`
  summary: Replace file-scope raw-pointer AI globals (g_SuggestionSystem et al.) with owned instances guarded against double-Init; Initialize() return value currently ignored.
  evidence: Blind Hunter review — second IDEGUI instance overwrites/leaks the global; Shutdown timeout early-return leaves the pointer dangling.
  resolution: 已修（2026-09-17）：六個 AI 全域改為 `std::unique_ptr`；ctor 加 `if (!g_DevSystem)` 防重複初始化；Shutdown 逾時分支保留「不釋放以避免 UAF」語義（unique_ptr 存活即洩漏）。
- source_spec: `_bmad-output/implementation-artifacts/spec-render-training-data.md`
  summary: `NeuralLayer::Backward` calls `activationDerivative(lastOutput[i])` on the post-activation value — sigmoid derivative computes σ(y)·(1−σ(y)) instead of y·(1−y) (ReLU coincidentally correct). Gradients are distorted though sign is preserved; any fix changes training dynamics globally, so evaluate against AITestSuite before changing.
  evidence: Edge Case Hunter review of SynthDataDemo — AI/NeuralNetwork.cpp ~line 141.
  resolution: 已修（2026-09-17）：Forward 改存 `lastPreActivation`（z），Backward 導數吃 z——sigmoid/tanh 梯度正確。驗證：AITestSuite 7/7 + headless AND 訓練 loss 0.25→0.0096 收斂。
- source_spec: `_bmad-output/implementation-artifacts/spec-render-training-data.md`
  summary: `NeuralLayer` seeds its mt19937 from `std::random_device` with no seed API — in-tree NN init is non-reproducible. Consider a `NeuralNetwork`/`NeuralLayer` seed parameter (or Build(seed)) so callers don't need the SetWeights/SetBiases override workaround used in SynthDataDemo.
  evidence: Blind Hunter + Edge Case Hunter reviews.
  resolution: 已修（2026-09-17）：`NeuralLayer::SetSeed`（重 seed + 重建權重）、`NeuralNetwork::Build(unsigned seed)` 重載、Train shuffle 改用成員 rng——同 seed 建構權重完全一致（headless 驗證）。
- source_spec: `_bmad-output/implementation-artifacts/spec-game-backend-services.md`
  summary: Add DELETE endpoints (or a documented retention policy) for /api/replays and /api/rosters — currently the only way to remove a bad upload is deleting the H2 files.
  evidence: Blind-hunter review — services accept unauthenticated writes forever with no removal path; deferred because DELETE adds public API surface the spec's intent did not request.
  resolution: 已修（2026-09-23）：`DELETE /api/replays/{id}`、`DELETE /api/rosters/{id}`——存在回 204、未知回 404 JSON error。各補兩格 MockMvc 測試（verify deleteById）；replay 16/16、roster 21/21。
- source_spec: `_bmad-output/implementation-artifacts/spec-game-backend-services.md`
  summary: Extract a services-common Maven module for the duplicated parse/json/badRequest/notFound helpers in RosterController and ReplayController.
  evidence: Blind-hunter review — two controllers already diverge in style (StringBuilder concat vs record serialization); a shared module is a structural refactor beyond this change's scope.
  resolution: 已修（2026-09-24）：新增 `services/services-common` 模組，`JsonSupport` 集中 parseQuietly×2/toJson/error；兩 controller 私有 helper 改薄委派。parent pom 掛模組、雙 service pom 加依賴；JsonSupportTest 7/7,controller 測試全綠。
- source_spec: `_bmad-output/implementation-artifacts/spec-game-backend-services.md`
  summary: Manage H2 schema with Flyway instead of ddl-auto=update, and wire Maven into CI when Java services are expected to build in CI.
  evidence: Blind-hunter + verification-gap reviews — schema drift is unmanaged and Java tests never run in CI; both were explicitly out of this spec's boundaries.
  resolution: 已修（2026-09-25）：flyway-core 進雙 service pom（H2 支援內建 core）；`db/migration/V1__init.sql` 對齊 entity 映射；`ddl-auto: validate` + `baseline-on-migrate`（舊 H2 檔無痛交接）。CI `services-test` job：setup-java 21 + `mvn -B test`。SchemaMigrationTest 雙模組 5/5。
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: DetectGovernanceEvents Atrocity branch unreachable — rout skips damage so members never drop after routing, and the killing blow clears IsRouting before detection runs; hardcodes `team != 0` as enemy.
  evidence: Blind-hunter + edge-case layers verified in BattleController.cpp ~L275-295/780; belongs to parallel C-2 governance work.
  resolution: 已修（2026-09-25）：ResolveCombat 改為「潰逃目標僅被明確 Engage 點名時吃單向傷害」（鄰接不波及——憐憫仍為預設，追殺是可歸責指令）；DetectGovernanceEvents 改 map-first 排序，全滅清旗標後仍記暴行一次；Intervene(Engage) 放行潰逃目標。team 常數仍是 0/1 約定（獨立 deferred 項未動）。GovernanceTest [5-7] 覆蓋追殺/全滅/鄰接三路徑。spec-atrocity-detection-fix.md。
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: Unrest events appended to chronicler after its display window closes — strings silently swallowed.
  evidence: DuanqiaoPlayable.cpp ~L1733-1736; parallel governance UI work.
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: Chapter shell regressions — replaying a taken chapter rewinds campaign progress, pendingChapter bypasses AdvanceChapter (ledger diverges), ChapterDef::map never loaded (hardcoded duanqiao.json).
  evidence: DuanqiaoPlayable.cpp ~L543-560/L1705-1729; parallel B-5/C-1 chapter shell work.
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: Village/convoy occupation scan ungated by battle phase (fires during Deployment/post-battle); interactable radius<=0 can never fire; interactable feed has no headless test coverage.
  evidence: DuanqiaoPlayable.cpp ~L1043-1070; verification-gap layer disposition=defer — extracting a Gameplay seam exceeds this spec.
  resolution: 已修（2026-09-25）：`GovernanceField::Update` 開頭自守 `battle.GetPhase() != Execution → return`——佔領/焚村/護輜三偵測全閘在執行階段（部署期佔村不記帳、運輸隊不走）；`DetectOccupation` 對 `radius<=0` 明確跳過視為停用點。GovernanceFieldTest [7] 驗部署期靜默+進執行後正常觸發、[8] 驗 radius 0/負值不觸發；既有五段補 `BeginExecution` 對齊新契約。
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: Orphaned install(DIRECTORY GUI/) rule ships AgentGUI.h with no compiled symbols; build-video/check_all.sh still lists GUI/AgentGUI.cpp.
  evidence: CMakeLists.txt ~L889; parallel engine/game split cleanup.
  resolution: 已修（拆倉 Phase 1，82f5a1d）：AgentGUI 遷入 MingGoRTS_IDE/，孤兒 install 規則與 check_all.sh 殘留皆已清。
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: GovernanceTest hygiene — leftover [dbg] printf, weak `emits>0` assertion counts deploy emits not governance, `evs.at` throws instead of FAIL.
  evidence: Examples/GovernanceTest.cpp §1; parallel test file.
  resolution: 已修（2026-09-23）：emit 斷言改計「治理：」前綴字串（排除部署 emit）；EvCount 缺鍵回 0 取代 at() 拋例外；補 unordered_map/cmath 明確 include。
- source_spec: `_bmad-output/implementation-artifacts/spec-quantum-plan-effects.md`
  summary: Plan-bonus HUD hides at exactly ×1.00 — the moment intel resolves against the tip, the label vanishes instead of showing a "落空" state.
  evidence: DuanqiaoPlayable.cpp ~L1436-1439; UX polish under parallel-session churn.
- source_spec: `_bmad-output/implementation-artifacts/spec-d1-myth-layer-seepage.md`
  summary: `MythLayer::ToJson`/`DeriveFrom` iterate `std::unordered_map` — save bytes and MythLog event order are nondeterministic run-to-run. Consider `std::map` or sorted emission if golden-file diffing or replay determinism matters.
  evidence: Edge-case-hunter review — `Campaign/MythLayer.cpp` ToJson regions/favor loops + DeriveFrom per-region dispatch order.
  resolution: 已修（2026-09-22）：`DeriveFrom`/`ToJson` regions 陣列改排序鍵迭代；`WriteJsonValue` 物件鍵一律排序輸出（favor 等 object 段同時治好）。MythLayerTest [11] 驗同狀態異插入序位元組一致 + 事件序確定。
- source_spec: `_bmad-output/implementation-artifacts/spec-d1-myth-layer-seepage.md`
  summary: `JsonValue::AsInt` casts `double`→`int` via `static_cast` — out-of-range input (`"lv":1e20`) is UB before any clamp runs. Pre-existing JsonParser weakness, newly exposed by MythLayer load paths; fix belongs in Serialization.
  evidence: Edge-case-hunter review — `Serialization/JsonParser.h:73`; affects every `AsInt` consumer repo-wide.
  resolution: 已修（2026-09-22）：`AsInt` 先 `isnan` 回 def，再夾取到 `numeric_limits<int>` 極值後轉型——UB 消除。SerializationTest 加 1e20/−1e20/NaN/截斷四格。
- source_spec: `_bmad-output/implementation-artifacts/spec-d1-myth-layer-seepage.md`
  summary: `WriteJson` emits floats via `%g` (6 significant digits) — pressures >~1e6 roundtrip lossily. Shared JsonWriter limitation; current MythLayer constants stay exact, revisit if magnitudes grow.
  evidence: Acceptance auditor + edge-case hunter — `Campaign/JsonWriter.h:49`.
  resolution: 已修（2026-09-22）：改「最短 roundtrip」寫法——6→17 位逐步升精度直到 `strtod` 讀回原值；0.6 仍寫 0.6、大數值不再截斷。MythLayerTest [12] 驗 1234567.8901234567 無損往返。
- source_spec: `_bmad-output/implementation-artifacts/spec-d1-myth-layer-seepage.md`
  summary: No production wiring `MythLayer`→`MythLog`: `SetEventCallback` exists and tests exercise it, but `CampaignState` does not register a callback — transitions are recorded in `transitions` only. End-to-end narrative hookup belongs with D-2+ epic work.
  evidence: Acceptance auditor — `Campaign/CampaignState.cpp` owns no MythLog callback registration; AGENTS.md:47 stale claim that CampaignState aggregates MythLog.
  resolution: 已修（2026-09-25，D-6 `6e132a1`）：DuanqiaoPlayable 殼層持有 MythLog，`Myths()` 事件回呼內同步 `Record`（與音景共用單槽）；結算頁「神蹟錄」段消費 `TestimonyLines()`。AGENTS.md 錯誤描述已勘誤（277aa6b）。MythLog 仍 session 級——持久化屬 C-5 範疇。
- source_spec: `_bmad-output/implementation-artifacts/spec-d1-myth-layer-seepage.md`
  summary: `myth_layer` section has no `potato.myth_layer/1` schema tag — bare object consistent with untagged `governance` section, but no version hook for future migration (NFR5).
  resolution: 已修（2026-09-23）：`ToJson` 寫出 `potato.myth_layer/1`；`FromJson` 軟驗證——缺 tag 舊檔容忍、錯 tag 拒絕且不動狀態。MythLayerTest [13] 七格覆蓋。
  evidence: Acceptance auditor — spec Code Map names the format; only root carries `potato.campaign/1`.
- source_spec: `_bmad-output/implementation-artifacts/spec-d2-shrine-spirits.md`
  summary: `ShrineField`/`GovernanceField`/`BattleController` all hardcode `team == 0` as player — a battle where the player faction isn't team 0 silently never triggers field interactions. Needs a shared player-team parameter across the three sites, not a per-class fix.
  evidence: Blind-hunter review — `Gameplay/ShrineField.cpp` occupation predicate mirrors `GovernanceField.cpp:53`; convention-wide limitation, out of D-2 scope.
- source_spec: `_bmad-output/implementation-artifacts/spec-d2-shrine-spirits.md`
  summary: `ShrineField::Bind` re-bind resets settled state → same node can pay favor twice. Documented in header (favor is persistent, unlike governance counts); enforce one-shot only if hot-reload becomes real.
  evidence: Edge-case-hunter review — consistent with `GovernanceField::Bind` semantics; flagged because favor is a resource.
- source_spec: `_bmad-output/implementation-artifacts/spec-d3-fate-conversion.md`
  summary: `FateExchange::battleUses` is runtime-only (spec Never: no schema change) — mid-battle save/reload refills the per-battle quota (save-scum). Persist per-battle exchange count if mid-battle saves become real.
  evidence: Edge-case-hunter + blind-hunter review — spec explicitly froze `potato.campaign/1`; logged so the save-integration story sees it.
- source_spec: `_bmad-output/implementation-artifacts/spec-d4-myth-incursion.md`
  summary: `MythIncursion` 持有非擁有 `Squad*`/`QuantumFog*`、吃 `BattleController&` — 生命期契約全憑呼叫端紀律：Update 只能在 Execution 拍以「已乘 timeScale 的 dt」驅動、不可在 squad 迭代回呼內呼叫（CreateSquad 會 rehash 迭代器）、物件不得比 battle 長命。已寫進檔頭契約；若未來加 BattlePhase getter 可改為 field 內自守。
  evidence: Three-layer review — `Gameplay/MythIncursion.h` 契約註記；專案無 phase 查詢 API（BattleController 無 GetPhase）。
  resolution: 已修（2026-09-23，`9debcf5` + D-6 `6e132a1`）：新增 `Disarm()` 清 ghost/fog/ghostEid；Arm 驗證非法 kind/NaN 座標、re-Arm 先 Reveal、Trigger latch 先行防 Emit 重入；playable 戰後 scope 結束前呼叫 Disarm（契約落地）。GetPhase 自守強化仍未做——半開放。
- source_spec: `_bmad-output/implementation-artifacts/spec-d4-myth-incursion.md`
  summary: FoxRumor 假雲觀測方硬編 team 0 — 與 ShrineField/GovernanceField 的 team-0 慣例同一洞：玩家非 team 0 的戰場情報計費會指錯邊。需跨三處的共享玩家 team 參數，非單類修復。
  evidence: Edge-case-hunter review — `QuantumFog::AddEntityCloud` team 參數是觀測方語義；同 D-2 deferred 條目。
- source_spec: `_bmad-output/implementation-artifacts/spec-d5-seepage-visual.md`
  summary: MythLayer 區域鍵目前用章節 `def.map` 去副檔名推導（"duanqiao"）——與 D-2 shrine region 同命名空間但無正式 `region` 欄位。若多張地圖共用檔名或章節需要跨圖滲透，ChapterDef 需補顯式 region 欄。
  evidence: D-5 spec 區域鍵慣例註記 + 三路審查——慣例可行但非 schema 級契約。
- source_spec: `_bmad-output/implementation-artifacts/spec-d5-seepage-visual.md`
  summary: `AudioCues.h` 音檔路徑為 cwd 相對（assets/audio/ 目前不存在）——miniaudio 降級契約下不會崩，但 dist 包/不同工作目錄啟動時 cue 靜默失效。屬 F-4 資產管線範疇。
  evidence: D-5 review 附帶發現——F-4 音景床 decode 也受此影響；D-5 只負責送 audioLevel。
  resolution: 已修（2026-09-23）：`AudioCues::Defaults` 加 assetsRoot 參數（預設 "assets" 保舊行為）；DuanqiaoPlayable 改傳 `DemoAssets::Root()`——以 exe 定位資產根，cwd 無關。
- source_spec: `_bmad-output/implementation-artifacts/spec-d5-seepage-visual.md`
  summary: DuanqiaoPlayable 事件文字匹配（"doctrine"/"潰逃" 子字串）可能過寬觸發 cue——非 rout 訊息含「潰逃」字樣會放錯音效。屬 F-4 事件→cue 文案契約範疇。
  evidence: D-5 review 附帶發現——F-4 的 PlayCueForEvent 文字表。
