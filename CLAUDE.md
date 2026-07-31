# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A 2D Terraria-style game written in C++20, built on a custom engine. The repo is split into a reusable **engine** static library and a thin **game** executable that currently drives it through hand-written test scenes rather than a real gameplay loop.

## Working backlog

[TODOS.md](TODOS.md) at the repo root tracks planned engine work. Check it when picking a next task, and keep it current: add items when new work is defined, edit items when a plan changes, check off / move to "Done" with a one-line note when something lands. Don't let it drift from what's actually true in the code.

## Build & Run

The project uses CMake. VS Code is configured to use the **Ninja** generator and copies `compile_commands.json` to the repo root for clangd.

```bash
# First-time setup: init the git submodules (glfw, miniaudio, nlohmann_json,
# freetype, msdf-atlas-gen — the last one pulls msdfgen recursively)
git submodule update --init --recursive
```

`freetype` + `msdf-atlas-gen` are only needed to **bake** a font atlas. They are
gated behind the `ENGINE_BUILD_FONT_BAKER` CMake option (default `ON`); with it
`OFF` neither library is configured or linked, and `Font` still loads any atlas
already present in `.cache/fonts/`.

Two dependencies are **NOT** submodules and must be placed manually or the CMake configure step fails with a FATAL_ERROR:
- `lib/glad/` — generate from the GLAD web service (OpenGL loader), place its `include/` and `src/` folders here.
- `lib/stb/` — download `stb_image.h`, `stb_image_write.h`, `stb_perlin.h` from the stb repo; `stb_impl.cpp` provides the single-translation-unit implementation.

```bash
# Configure + build (from repo root)
cmake -S . -B build -G Ninja
cmake --build build

# Run — the working directory MUST be the repo root, because assets and the
# log file are referenced by relative paths like "game/assets/..." and
# "game/output/log.txt". VS Code sets cmake.debugConfig cwd to the workspace root.
./build/game/game.exe
```

There is **no test framework**. The "tests" in `game/src/tests_scenes/` are interactive demo `main`-like functions (`ecs_test`, `piano_demo`, `file_management_test`, `batch_renderer_test`, `tilemap_test`, `ui_test` — the font/text/layout demo, which also logs a numeric layout proof at startup — and `ui_layout_test`, currently an empty stub awaiting the UI rewrite) declared in `game/include/test_funcs.hpp`. To switch which scene runs, edit the call in [game/src/main.cpp](game/src/main.cpp). Only one runs per build.

`DEBUG` is defined unconditionally in the root [CMakeLists.txt](CMakeLists.txt); logging macros compile to no-ops when it is absent. OS is injected as the `OS_NAME` macro (compared against `OS_WINDOWS`/`OS_MACOS`/`OS_LINUX`).

## Architecture

### Layering
`game` (executable) → depends on → `engine` (static lib) → depends on → `lib/*` (glfw, glad, stb, miniaudio, nlohmann_json). The game never touches third-party libraries directly. Include everything engine-side via the single umbrella header [engine/include/EngineInclude.hpp](engine/include/EngineInclude.hpp); everything lives in `namespace Engine`.

### Include-aggregator convention
Every subsystem folder has a `*Include.hpp` (e.g. `CoreInclude.hpp`, `GraphicsInclude.hpp`, `ComponentsInclude.hpp`) that re-exports the folder's public headers. When adding a new type to a subsystem, add its header to the corresponding `*Include.hpp` so it flows up to `EngineInclude.hpp`. Sources are glob-collected by CMake (`file(GLOB_RECURSE ... src/**.cpp)`), so new `.cpp` files are picked up on the next configure but **new files require re-running CMake configure**, not just build.

### Global managers are Singletons
Core services are accessed as `Xxx::get()` via the CRTP base [utils/Singleton.hpp](engine/include/utils/Singleton.hpp): `WindowManager`, `ViewContext`, `Renderer`, `Input`, `Time`, `Logger`, `ResourceManager`, `AudioManager`, `TilemapManager`, `TypeRegistery`. Constructors are private with `friend class Singleton<...>`.

### ViewContext — the active window + camera
[core/window_management/ViewContext.hpp](engine/include/core/window_management/ViewContext.hpp) owns "which window am I working on and what camera is looking at it": the active window id, the active camera entity, its view-proj matrix and visible `WorldBounds`, plus `screenToWorld`/`worldToScreen`/`getMouseWorldPos`. `Renderer`, `TilemapRenderer` and `Input` all read from it rather than holding or re-receiving that state. Per window each frame, `setActiveWindow(id)` comes first — `Input::update()` picks its window from here — then `updateCamera(registry)` once the camera has been moved. Cull with `isVisible(pos, halfExtents)`; the bounds are already widened for a rotated camera.

### ECS (custom, EnTT-like)
Located in `engine/include/ecs/`. Not EnTT — a hand-rolled equivalent.
- `Registry` owns entities and one `SparseSet<T>` component pool per type, keyed by a runtime type id from `TypeRegistery`. It also holds singleton-style "contexts" (`setContext`/`getContext`).
- `EntityHandle` is the ergonomic wrapper: `registry.create()` returns a handle; use `.emplace<T>()`, `.get<T>()`, `.tryGet<T>()`, `.contains<T>()`, `.remove<T>()`. Multi-get returns a `std::tie` tuple.
- `View<Components...>` iterates entities having all listed components, yielding `[entity, comp&, ...]` structured bindings.
- Entities use a generation/free-list scheme; `destroyDeferred` + `flush` allow deferred destruction. Component pools expose `onCreate`/`onDestroy`/`onSet` signals (see `ecs/signals/`).

### Rendering (batch renderer + multi-window)
[graphics/Renderer.hpp](engine/include/graphics/Renderer.hpp) is a batched quad renderer (single Singleton). Typical per-frame flow, driven per-window:
1. `WindowManager::createWindow(...)` returns an `IdType`; cameras are entities with `TransformComponent` + `CameraComponent` (the camera's `windowId` binds it to a window).
2. Per window each frame: `ViewContext::setActiveWindow(id)` → `Input::update()` → `ViewContext::updateCamera(registry)` → `beginPass()` → `setShader()` / `clearColor()` → `addQuad(...)` many times → a second `beginPass()` with a post-processing shader → `drawToWindow()` → `window->swapBuffers()`. The Renderer binds the GL context lazily, syncing to `ViewContext`'s active window inside `beginPass`/`beginScene`/`flush`.
3. `GlfwContext::pollEvents()` once after all windows; close windows collected during iteration afterward (don't mutate the window list mid-loop).

Rendering goes through an offscreen FBO then a post-processing pass to the window (see `beginPass`/`drawToWindow`/`drawToBuffer` and the `gl_wrappers/` RAII wrappers: `GlBuffer`, `GlShader`, `GlTexture`, `GlFrameBuffer`, `GlVertexArray`, `GlLayout`). Shaders are single `.glsl` files containing multiple stages (see `game/assets/shaders/`).

### Application — the frame/window loop
[core/Application.hpp](engine/include/core/Application.hpp) owns steps 2–3 above so scenes don't hand-roll them: the `anyWindowOpen` loop, `Time::update`, per-window `setActiveWindow`/`Input::update`, escape/close bookkeeping, `ViewContext::updateCamera`, `swapBuffers` and `pollEvents`. Scene code supplies three `Delegate` phases — `onFrame(dt)` (once per frame), `onWindowUpdate(windowId, dt)` (per window, before the view-proj matrix is baked, so move cameras here), `onWindowRender(windowId, dt)` (per window, matrix ready) — bound with `app.onFrame().bind(&fn)` and run with `app.run()`. Each phase is optional; unbound ones are skipped. The render body (passes, shaders, draw calls) stays entirely in the callback. `tilemap_test`/`batch_renderer_test` show the shape; the manual loop is still valid for scenes needing full control.

### Other subsystems
- **Logging** ([core/logging](engine/include/core/logging)): use `LOG_INFO/LOG_WARNING/LOG_ERROR(fmt, ...)` (std::format-style). Sinks are pluggable (`ConsoleSink`, `FileSink`); add with `Logger::get().addSink<FileSink>(path)`. `setUseAsync(true)` enables async logging. Register a `std::formatter` for a custom type via the `DEFINE_TYPE_FORMATTER` macro. Never use `std::cout`/`std::cerr` directly — always go through the logging macros.
- **Input** ([core/input](engine/include/core/input)): `Input::get().update()` per window, after `ViewContext::setActiveWindow(id)`; axis-based via `addAxis("Name", {posKeys..., negKeys...})` then `getAxis("Name")`; also `keyPressed(KeyCode::...)`.
- **File management** ([core/file_management](engine/include/core/file_management)): typed file entries (`TextFile`, `BinaryFile`, `JsonFile`, `ImageFile`) under an `IFileEntry`/`Folder` tree via `FileManager`.
- **Audio** ([audio](engine/include/audio)): miniaudio-backed `AudioManager` with buses, streams, and instances (`piano_demo` scene exercises it).
- **Fonts** ([graphics/text](engine/include/graphics/text)): `Font` is an `IResource` built from a `.ttf`/`.otf`. Pick `FontAtlasType::Mtsdf` (scales/rotates cleanly) or `FontAtlasType::Bitmap` (crisp at its native `emPixelSize`) via `FontBakeSettings`; the type selects the texture filtering internally. Glyph data comes back as `Glyph`/`FontMetrics` structs in **em units** — multiply by a pixel size. `getUnitRange()` gives the per-vertex factor an MSDF shader needs (`VEC2_ZERO` for a bitmap atlas). Bakes are cached in `.cache/fonts/` as a `.png` + `.json` pair and rebuilt only when the font file's bytes actually change; `FontBaker` is the only code that touches msdf-atlas-gen.
- **Text rendering** ([graphics/text](engine/include/graphics/text)): `TextRenderer` is a Singleton owning its own `BatchRenderer<TextVertex>` (init it with `TextShader.glsl`). It only *emits* — all layout lives in `TextLayoutCalculator` (below). Two `draw`s: `draw(block, origin, boxSize)` is the real one (walks a solved `TextBlock`'s runs; `boxSize` exists solely to resolve `TextAlignV`, and a zero box means Top), and `draw(font, text, origin, style, spanStyles)` is a convenience wrapper that builds a throwaway `TextBlock` each call — own a `TextBlock` in a frame loop instead. `drawSpan` (baseline pen in → pen out) stays the public chaining primitive for mixing fonts on one line. `origin` is **top-left**; `drawSpan` is baseline-relative. `flush()` is explicit — call it once after all text so everything shares one draw call. One `TextStyle` covers both atlas types and every effect: colour, `letterSpacing`/`lineSpacing`, `italicSkew`, `underline`, `strikethrough`, plus the distance-field ones — `outlineWidth`/`outlineColor`, `boldness` (±weight), `softness` (blur), and `shadowColor`/`shadowWidth`/`shadowSoftness`/`shadowOffset` (a zero offset makes it a centred glow). A `Bitmap` font silently ignores the distance-field ones but keeps colour, spacing, skew and the decorations, since those are geometry. **Every measurement except `size` is in em**, so a style holds its proportions under camera zoom — do not put these in pixels, or effects drift with zoom and a wide glow floods the glyph quad. Outline/glow width is capped by the baked field: see `Font::getMaxEffectEm()`, and raise `FontBakeSettings::distanceRangePixels` (default 4) for wider effects. Positional styling: `"a /sb/s c"` plus a `{styleB}` list; `//s` escapes a literal `/s`, a short style list falls back to the default, and note `"m/s"` reads as a tag. Style-varying data is **per-vertex, never a uniform**, so a uniform would cost one draw call per style. Text draws in world space by default; `setViewProjOverride` swaps in a screen-space matrix (it flushes on change, so submit all world text, then all screen text). Iterate strings with `Utf8::next` — Latin-1 glyphs are multi-byte.
- **Text layout** ([graphics/text/TextLayoutCalculator.hpp](engine/include/graphics/text/TextLayoutCalculator.hpp)): the single owner of wrapping, line boxes and alignment, serving both world-space text and the UI. A `TextBlock` ([TextLayout.hpp](engine/include/graphics/text/TextLayout.hpp)) holds **input and computed output together** — font, text, style, span styles, `TextAlignH`/`TextAlignV`, `TextOverflow`, wrap and `fixedLineHeight` flags, plus the solved `TextRun`s, `TextLine`s and `bounds`. Two entry points: `measureMinMaxWidth(block)` → `{min-content = longest word, max-content = longest line}`, and `calculate(block, availableWidth)` → height, caching everything on the block. A `TextRun` is a per-line, per-span slice whose `offset` already holds x and the **baseline** y, so rendering never re-walks the string. **A line's height/ascent/descent are the max over every style touching it** (CSS line-box rule) and baselines are placed at `lineTop + thatLine'sAscent`, never stepped baseline-to-baseline; `fixedLineHeight` opts out to the default style. Wrapping backtracks to the last space even several spans back, and a word may straddle a `/s` boundary. `availableWidth <= 0` means unbounded, which is how non-wrapping text reuses one code path. Non-obvious invariants: **`TextBlock` is neither copyable nor movable** — its runs hold `string_view`s into its own `std::string`, and a short string moves by copying into the destination's inline storage, silently dangling every view; `setText` clears runs/spans **eagerly** for the same reason. Three dirty flags (spans/widths/layout) with per-setter equality guards make re-setting identical data every frame free, and `setAlignV` dirties nothing because vertical alignment is applied at draw. The calculator itself is **stateless**, so unlike the `measure()` it replaced it is safe to call mid-draw. **Horizontal alignment is baked into run offsets** (the width is final at `calculate` time); **vertical is applied in `TextRenderer::draw`** (it needs a box height only the caller has). `TAB_SPACES`/`FALLBACK_SPACE_ADVANCE` and the whole codepoint→advance rule live once in [TextMetrics.hpp](engine/include/graphics/text/TextMetrics.hpp) — both the measuring walk and the emitting walk call `TextMetrics::step`, so a measured width cannot drift from a drawn one. `TextOverflow` is declared but **not honoured yet**; everything behaves as `Visible`. Exercise with `ui_test`, which logs a numeric proof at startup.
- **UI** ([graphics/ui](engine/include/graphics/ui)): **being rewritten by hand — treat this folder as in flux.** The previous immediate-mode system (`UiSystem`, `UiRenderer`, `UiElement`, `LayoutCalculator`, `ILeafMeasurer`, `TextMeasure`, `UiPresets`, `UiState`) was deleted in `a886a32`; recover it with `git show a886a32^:<path>` rather than assuming any of it still exists. What is here now: `UIManager` (a Singleton owning an `IdIndexedVector<UINode>`), `UINode` (parent / firstChild / lastChild / nextSibling links, a `UINodeStyle`, and an optional `IUILeafData*`), `UINodeStyle` (all-`std::optional` fields merged by `combine`, where a set field on `other` wins), and `leaf_types/` (`IUILeafData` → `UITextLeafData`). A node with no `leafData` is a container; leaves route `measureWidths`/`measureHeight`/`draw` through the leaf, so callers use `UINode` uniformly. There is no layout solver or renderer yet — `UILayoutCalculator` is an empty stub, and `ui_layout_test` is stubbed out until they exist.
- **Math** ([utils/math](engine/include/utils/math)): custom `Vec2/3/4`, `Mat4`, `Random`, `MathFuncs` — do not pull in GLM.

## What to reach for
Prefer the engine's own systems over the raw STL/third-party equivalent. The game never includes third-party headers directly — the engine wraps each one.

Use the engine's, not the raw version:
- **Math / vectors / matrices** → `Vec2/3/4`, `Mat4`, and the `Math::` free functions (`sin`, `clamp`, `lerp`, `floor`, `perlin2D/3D`, `DEG2RAD`, …). Do **not** include `<cmath>` or GLM. Randomness → `Random::` (`float01`, `rangeInt`, `pointInCircle`, …), not `<random>`.
- **IDs / handles** → `IdType` with `INVALID_ID`/`START_ID` (from [utils/TypeAliases.hpp](engine/include/utils/TypeAliases.hpp)), plus the `uint`/`byte` aliases — never a raw `int`/`unsigned` for a handle. Runtime per-type ids → `TypeRegistery::get().getTypeId<T>()`.
- **Stable-id → value storage** → `IdIndexedVector<T>` ([utils/IdIndexedVector.hpp](engine/include/utils/IdIndexedVector.hpp)): id-keyed, O(1) `get`, swap-remove; derive `T` from `IHasId` to auto-assign ids on `add`. Reach for this before hand-rolling an id/slot map.
- **Global services** → the `Singleton<T>` CRTP (private ctor + `friend class Singleton<...>`).
- **Active window / camera / matrices / screen↔world** → `ViewContext`, never the `Renderer`. It is the single source for the current window id, view-proj matrix, visible bounds and mouse world position.
- **Heap-owned resources** (textures, tilesets, audio, …) → `ResourceManager` + `IResource` (`addResource<T>(...)` → `IdType`, `getResource<T>(id)`).
- **Files** → `FileManager` typed entries (`TextFile`/`BinaryFile`/`JsonFile`/`ImageFile`), not raw `<fstream>`.
- **Entities / game state** → the ECS (`Registry`, `EntityHandle`, `View<...>`), not ad-hoc containers of game objects. UI nodes are the exception: they live in `UIManager`'s own `IdIndexedVector<UINode>`, not in the registry.
- **GL objects** → the `gl_wrappers/` RAII types, never raw `glGen*`/`glBind*`.
- **Console output** → the logging macros, never `std::cout`/`std::cerr`.

**STL is used freely** where there's no engine equivalent: `std::vector`, `std::unordered_map`, `std::string`, `std::array`, `std::pair`/`tuple`, structured bindings, `if constexpr`, fixed-width ints (`<cstdint>`). `std::format`-style formatting comes in through the logging macros.

**Ownership**: managers use **raw owning pointers with manual `new`/`delete`** (e.g. `ResourceManager`, per-context FBOs/VAOs) — smart pointers are not the norm. Match that; don't introduce `unique_ptr`/`shared_ptr` unless extending code that already uses them.

**Third-party libraries**, each reached only through its wrapper: glfw → `WindowManager`/`GlfwContext`; glad → `gl_wrappers/*`/`GladContext`; stb → `ImageFile` + `Math::perlin2D/3D`; miniaudio → `AudioManager`; nlohmann_json → `JsonFile`; freetype/msdf-atlas-gen → `Font` (only ever included by `FontBaker.cpp`).

## Conventions
- Members prefixed `m_`; constants `ALL_CAPS`; types `PascalCase`; methods `camelCase`.
- 4-space indent, `{` on same line except function bodies, Allman-style class access labels (`  public:` indented). No `.clang-format` is committed — match the surrounding file.
- Class member ordering, top to bottom: private vars, public vars, public functions, private functions (see [Renderer.hpp](engine/include/graphics/Renderer.hpp)).
- IDs are `IdType` with `INVALID_ID` sentinel; prefer engine `TypeAliases.hpp` over raw ints.
- **Comments — default to none; rename instead of commenting.** No comments on declarations in `.hpp` files (members, fields, params, function signatures) — the name must carry the meaning. Only exception: a short trailing *format/example* when an encoding can't fit in a name or a short description on a struct that is a bit ambiguis and cant be explaind in one name, e.g. `std::string neighbors;   // 8 chars, e.g. "1x1xxxxx"` (format only, not purpose). In `.cpp` bodies, comment only what the code can't express — a non-obvious step, tricky invariant, magic number, or bug workaround — one line above the block. Never restate or narrate what a line does; comment *why*, not *what*.
