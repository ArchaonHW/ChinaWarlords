<!-- bmad:context -->
<!-- Verified 2026-09-29 (unversioned: git unavailable in this environment). Managed by bmad-project-context; edits inside this block are replaced on refresh. Keep anything you want preserved outside the markers. -->

## MingGoRTS / PotatoEngine

C++20 game engine (PotatoEngine) + greenfield game layer (MingGoRTS). Planning artifacts live in `_bmad-output/` (GDD, `game-architecture.md`, `narrative-design.md`, `planning-artifacts/epics.md`, `implementation-artifacts/sprint-status.yaml`); these are authoritative over README status claims.

## Policy

- Never push to `main`/`develop`; PRs only, Conventional Commits.
- Never modify `external/` — vendored dependencies.
- No third-party JSON libraries; use the game-layer `JsonValue` (to be built in `Gameplay/Json/`).
- No unsafe C functions: `gets`, `strcpy`, `strcat`, `sprintf`, `vsprintf`, `scanf`.
- All game content files are versioned JSON: `potato.<name>/<ver>` header, reject bad schema without mutating state.
- Saves write via temp-file + rename (atomic).

## Where things are

- Engine subsystems: top-level dirs (`Core/`, `ECS/`, `Events/`, `Rendering/`, `Serialization/`, `GUI/`, ...).
- Game layer (planned, may not exist yet): `Gameplay/` sim → `Campaign/` → `Game/` shell → `assets/`.
- Executable examples/tests: `Examples/` — each is its own `add_executable` target.
- Story files and sprint tracking: `_bmad-output/implementation-artifacts/`.

## Running and verifying

- Build (Windows/MSVC): `BuildEngine.bat`, or `cmake -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release`.
- MinGW builds: guard toolchain specifics with `WIN32 AND NOT MSVC`; verify both MSVC and MinGW before calling work done.
- No unified test command — there is no CTest; tests are standalone executables under `Examples/`. Game-layer tests use the `potato_test_<name>` convention (POTATO_TESTS option is planned, not yet in CMake).
- Environment: `git`, `python`, `uv` are NOT on PATH here — don't script around them.

## Conventions that differ from defaults

- `Gameplay/` must compile headless: no Rendering/GUI/OpenGL includes; dependency direction is Engine ← Gameplay ← Campaign ← Game, single direction only.
- Simulation is deterministic: fixed 20 Hz tick, integer/fixed-point only (no floats in sim), one seeded PRNG stream, canonical eval order (squad index → slot index).
- Truth boundary: nothing outside `Gameplay/Sim/` reads true enemy state — presentation and doctrine conditions read certainty only.
- Two event regimes: typed `SimEvent` list inside the sim (recorded for replay); engine `EventBus` only at layer boundaries — subscribers never write `BattleState`.
- `PotatoEngine` facade subsystem accessors are commented out — bind engine leaf modules directly; do not "fix" the facade as a side task.
- Error handling: no exceptions in the tick path; `Result<T>` at I/O/content boundaries; registries immutable during battle; zero file I/O in the tick path.

## Known pitfalls

- README describes aspirational state (completed epics, existing `Gameplay/`, old planning paths) that does not match the filesystem — trust `_bmad-output/` documents and the actual tree, not README status lines.
- PowerShell here-strings treat backticks as escapes — prefer `[IO.File]::WriteAllText` with single-quoted here-strings when writing content containing backticks.
- The repo path contains CJK (`F:\民國史詩`) — MinGW make and FetchContent subbuilds fail on it ("Illegal byte sequence"). Build via an ASCII junction, e.g. `F:\dev\MingGoRTS -> F:\民國史詩\HWC\MingGoRTS` (older sessions may reference `C:\MingGoRTS`, a legacy alias of the same junction — prefer F: to keep workspace off the system drive).
- `PotatoEngine` static lib does not compile under MinGW (`Security/SecuritySystem.cpp` uses MSVC-isms) — `PotatoGameplay` deliberately does not link it yet. For headless builds use `-DPOTATO_BUILD_GUI=OFF` (skips glfw/glad FetchContent, which also needs git/network).

<!-- /bmad:context -->

## Local environment (D:\HWC\MingGoRTS, set up 2026-10-12)

- Toolchain installed via winget: CMake 4.4.4, Git 2.55.0, VS 2022 Build Tools (MSVC 19.44 + Windows SDK). Note: freshly spawned shells may need a PATH refresh (`[Environment]::GetEnvironmentVariable("Path","Machine")`) to see `cmake`/`git`.
- Configure+build works with the default `POTATO_BUILD_GUI=ON` — FetchContent clones glfw 3.3.8 / glad v2.0.4 fine (ASCII path, git+network present).
- Tests DO run under CTest: `ctest -C Release` in `build/` — 14/14 pass (potato_test_* + potato_record_emit/potato_verify_cli + gameplay_dep_guard). `gameplay_dep_guard` requires `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser` (now set).
- All targets build clean under MSVC as of 2026-10-12 (legacy AI/Security/IDE modules were repaired: undeclared members, stale duplicate tail in AIAgentSystem.cpp, missing class declarations, unsafe-C calls replaced with `_s` variants).
- VS installer requested a reboot to finish; builds work without it, but reboot once when convenient.

## Engine notes (synced from upstream PotatoEngine checkout, 2026-10-12)

- `Rendering/` is now compiled as the `PotatoRendering` static lib (all `Rendering/*.cpp` + Logging/Scene/Platform-GLFW/Input sources; links `NeuralNetwork glad glfw imgui`). It contains the Godot-style server layer: `Core/RID.h` (RID handles, generational validation) → `Rendering/RenderingServer.h` + `RenderingServerDefault` (`CreateHeadless()` Dummy rasterizer / `CreateGL()` GL backend) → `RendererCanvasCull`/`RendererSceneCull` → `Rasterizer{Canvas,Scene}` (`GL` or `Dummy`). `AI/RenderAgentTools` registers ~25 `render_*` tools on ToolFramework so agents can draw via the headless server.
- `POTATO_BUILD_GUI` semantics changed: vendored native libs (external/glfw, glad_gen, imgui) now build UNCONDITIONALLY (no network/context needed); the flag only gates windowed executables and the glfw FetchContent fallback. Headless builds still work — `-DPOTATO_BUILD_GUI=OFF` verified.
- `AI/AgentChain` gained real dispatch: Tool nodes via `SetToolRegistry`, condition/loop/parallel subroutine nodes (`{var}` placeholders), `AgentGraph` topological exec, `intermediateResults` on ChainResult.
- `MathUtils/RiemannZeta.h` (`Potato::RZ`) is the shared ζ/L numerical core for `RiemannExplorerTest` (rewritten) and `RiemannExplorer2Test`; `AI/ZetaAgentTools` exposes it to agents via ToolFramework (`ZetaAgentToolsTest`). All engine tests are ctest-registered under `POTATO_TESTS`.
- Known environmental issue: `gameplay_dep_guard` CTest fails "Process not started" spawning powershell.exe despite RemoteSigned being set — script itself passes when run directly; suspected pending VS-installer reboot. Re-check after reboot.
