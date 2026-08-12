# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A 2D game engine in C++20, split into a reusable **engine** static library and a
thin **game** executable that currently drives it through hand-written test scenes rather
than a real gameplay loop.

## Working backlog

[TODOS.md](TODOS.md) is the backlog — planned work under section headings, plus a "Done"
section recording what landed. Check it when picking up a task and keep it current: add
items as work is defined, edit them when a plan changes, and move them to Done when they
land. Don't let it drift from what the code actually does.

**Keep it short.** An open item is **one line**: a bold title and a clause saying what it
is or why it matters. A done item is **one or two lines** — what landed, and at most the
one decision worth remembering. TODOS.md is an index, not a design document; the reasoning
belongs in this file or in a comment next to the code. If an entry needs a paragraph to
explain itself, that paragraph is in the wrong place.

## Build & Run

```bash
# First-time setup: submodules (glfw, miniaudio, nlohmann_json, freetype,
# msdf-atlas-gen — the last pulls msdfgen recursively)
git submodule update --init --recursive

cmake -S . -B build -G Ninja
cmake --build build

# Working directory MUST be the repo root — assets and the log file are relative
# paths like "game/assets/..." and "game/output/log.txt".
./build/game/game.exe
```

- **New `.cpp` files need a CMake *configure*, not just a build** — sources are
  glob-collected (`file(GLOB_RECURSE ...)`).
- **Building from a plain shell fails** with `cannot open include file: 'cstdint'` — the
  MSVC environment isn't loaded. Run `vcvars64.bat` first (VS 2022 BuildTools) or build
  from VS Code, which is configured for Ninja and copies `compile_commands.json` to the
  repo root for clangd.
- `DEBUG` is defined unconditionally in [CMakeLists.txt](CMakeLists.txt); logging macros
  compile to no-ops without it. The OS is injected as `OS_NAME` (compare against
  `OS_WINDOWS`/`OS_MACOS`/`OS_LINUX`).
- `ENGINE_BUILD_FONT_BAKER` (default `ON`) gates freetype + msdf-atlas-gen. With it `OFF`
  neither is configured or linked, and `Font` still loads any atlas already in
  `.cache/fonts/`.

Two dependencies are **not** submodules and must be placed by hand or configure fails with
a FATAL_ERROR: `lib/glad/` (generate from the GLAD web service) and `lib/stb/`
(`stb_image.h`, `stb_image_write.h`, `stb_perlin.h`).

### Tests

There is **no test framework**. `game/src/tests_scenes/` holds interactive demo scenes —
`ecs_test`, `piano_demo`, `file_management_test`, `batch_renderer_test`, `tilemap_test`,
`text_rendering_test`, `ui_test` — declared in [game/include/test_funcs.hpp](game/include/test_funcs.hpp).
Only one runs per build; switch by editing the call in [game/src/main.cpp](game/src/main.cpp).
When you add a feature, exercise it in the scene that covers its subsystem.

## Architecture

**Layering:** `game` → `engine` → `lib/*`. The game never touches a third-party library
directly. Include everything through the single umbrella header
[engine/include/EngineInclude.hpp](engine/include/EngineInclude.hpp); everything lives in
`namespace Engine`.

**Include aggregators:** every subsystem folder has a `*Include.hpp` re-exporting its
public headers. Adding a type to a subsystem means adding its header there so it flows up
to `EngineInclude.hpp`.

**Global services are Singletons** via the CRTP base [utils/Singleton.hpp](engine/include/utils/Singleton.hpp)
— private constructor plus `friend class Singleton<...>`, reached as `Xxx::get()`.

**Frame loop:** [core/Application.hpp](engine/include/core/Application.hpp) owns the window
loop, `Time::update`, per-window `ViewContext::setActiveWindow` → `Input::update` →
`ViewContext::updateCamera`, `swapBuffers` and `pollEvents`. Scenes supply three optional
`Delegate` phases — `onFrame(dt)`, `onWindowUpdate(windowId, dt)` (move cameras here, before
the view-proj matrix is baked) and `onWindowRender(windowId, dt)` — and call `app.run()`.
A hand-rolled loop is still valid for scenes needing full control.

**Subsystem folders** under `engine/include/`: `audio`, `components`, `context`, `core`
(application/time/input/logging/files/resources/windowing), `ecs`, `graphics` (renderer,
GL wrappers, text, tilemap), `ui`, `utils`. Note the UI lives at `engine/include/ui`, not
under `graphics`.

## What to reach for

**Everything third-party is wrapped. Use the wrapper, never the library.** The same goes
for most of the STL where the engine has its own type.

| Need | Use | Not |
|---|---|---|
| Vectors, matrices, math | `Vec2/3/4`, `Mat4`, `Math::` (`sin`, `clamp`, `lerp`, `perlin2D/3D`, `DEG2RAD`) | `<cmath>`, GLM |
| Randomness | `Random::` (`float01`, `rangeInt`, `pointInCircle`) | `<random>` |
| Handles / ids | `IdType` with `INVALID_ID`/`START_ID`, plus `uint`/`byte` ([utils/TypeAliases.hpp](engine/include/utils/TypeAliases.hpp)) | raw `int`/`unsigned` |
| Runtime per-type ids | `TypeRegistery::get().getTypeId<T>()` | `typeid` |
| Id-keyed storage | `IdIndexedVector<T>` ([utils/IdIndexedVector.hpp](engine/include/utils/IdIndexedVector.hpp)); derive from `IHasId` to auto-assign | a hand-rolled id/slot map |
| Global services | the `Singleton<T>` CRTP | a file-static or a free global |
| Active window, camera, matrices, screen↔world | `ViewContext` | `Renderer`, `WindowManager` |
| Heap-owned resources (textures, tilesets, audio) | `ResourceManager` + `IResource` | bare `new` at a call site |
| Files | `FileManager` typed entries (`TextFile`, `BinaryFile`, `JsonFile`, `ImageFile`) | `<fstream>` |
| Engine-owned assets | `FileManager::engineAsset(...)` | a path relative to the game |
| Entities and game state | the ECS (`Registry`, `EntityHandle`, `View<...>`) | ad-hoc containers of game objects |
| GL objects | `gl_wrappers/` RAII types (`GlBuffer`, `GlShader`, `GlTexture`, `GlFrameBuffer`, `GlVertexArray`, `GlLayout`) | raw `glGen*`/`glBind*` |
| Console output | `LOG_INFO`/`LOG_WARNING`/`LOG_ERROR` (std::format style) | `std::cout`/`std::cerr` |
| UTF-8 iteration | `Utf8::next` | byte-wise loops (Latin-1 glyphs are multi-byte) |

UI nodes are the one deliberate exception to "game state lives in the ECS": they live in
`UIManager`'s own `IdIndexedVector<UINode>`.

**STL is used freely** where there is no engine equivalent — `std::vector`,
`std::unordered_map`, `std::string`, `std::array`, `std::pair`/`tuple`, structured
bindings, `if constexpr`, `<cstdint>`.

**Ownership:** managers use **raw owning pointers with manual `new`/`delete`**. Match that;
don't introduce `unique_ptr`/`shared_ptr` unless extending code that already uses them.

### Third-party libraries and their wrappers

| Library | Reached only through |
|---|---|
| glfw | `WindowManager` / `Window` / `GlfwContext` |
| glad | `gl_wrappers/*`, `GladContext` |
| stb | `ImageFile`, `Math::perlin2D/3D` |
| miniaudio | `AudioManager` |
| nlohmann_json | `JsonFile` |
| freetype, msdf-atlas-gen | `Font` (included only by `FontBaker.cpp`) |

### Subsystem entry points

Reach for these rather than rediscovering them:

- **Logging** — `Logger::get().addSink<FileSink>(path)`, `setUseAsync(true)`;
  `DEFINE_TYPE_FORMATTER` registers a `std::formatter` for a custom type.
- **Input** — `Input::get().update()` per window after `setActiveWindow`; axes via
  `addAxis(name, {pos..., neg...})` then `getAxis(name)`; also `keyPressed`,
  `mouseButtonPressed/Released/Held`, `getMousePos`, `getScrollDelta`.
- **Rendering** — `Renderer` is a batched quad renderer drawing through an offscreen FBO
  then a post-processing pass. Per window: `beginPass` → `setShader`/`clearColor` →
  `addQuad` × N → second `beginPass` with the post shader → `drawToWindow`. Shaders are
  single `.glsl` files holding several stages.
- **Text** — `Font` (async-loading `IResource`, mtsdf or bitmap atlas),
  `TextLayoutCalculator` owns all wrapping and alignment over a `TextBlock`,
  `TextRenderer` only emits. Every style measurement except `size` is in **em**.
- **UI** — immediate mode, rebuilt every frame. [ui/widgets/UIWidgets.hpp](engine/include/ui/widgets/UIWidgets.hpp)
  is the public face: `clear()`, `openContainer`/`closeContainer`, `addTextLeaf`, and a
  widget per free function. Parenting comes from an open stack, never an argument. Styling
  and layout are all-`std::optional` structs generated from one X-macro field list each,
  so adding a field is one line. Theme roles are read inline via `UITheming::colors()` /
  `metrics()` / `textStyle(name)`.
- **Audio** — `AudioManager` with buses, streams and instances.
- **Tilemap** — `TilemapManager`, `TilemapRenderer`, chunked and culled through
  `ViewContext`.

## Conventions

**These are not suggestions — follow them exactly.**

- Members prefixed `m_`; constants `ALL_CAPS`; types `PascalCase`; methods `camelCase`.
- 4-space indent, `{` on same line except function bodies, Allman-style class access
  labels (`  public:` indented). A `.clang-format` (100-column, the settings above) sits
  one directory **above** the repo, so it is not in version control; a PostToolUse hook
  runs `clang-format -i` on every C/C++ file written or edited. Don't hand-align code — it
  will be restyled. Match the surrounding file.
- Class member ordering, top to bottom: private vars, public vars, public functions,
  private functions (see [Renderer.hpp](engine/include/graphics/Renderer.hpp)).
- IDs are `IdType` with the `INVALID_ID` sentinel; prefer `TypeAliases.hpp` over raw ints.
- **Comments — default to none; rename instead of commenting.** No comments on
  declarations in `.hpp` files (members, fields, params, function signatures) — the name
  must carry the meaning. Only exception: a short trailing *format/example* where an
  encoding cannot fit in a name, or a short description on a struct that is genuinely
  ambiguous and cannot be explained by one name, e.g.
  `std::string neighbors;   // 8 chars, e.g. "1x1xxxxx"` (format only, not purpose). In
  `.cpp` bodies, comment only what the code cannot express — a non-obvious step, a tricky
  invariant, a magic number, a bug workaround — one line above the block. Never restate or
  narrate what a line does; comment **why**, not **what**.
