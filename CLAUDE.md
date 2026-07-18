# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A 2D Terraria-style game written in C++20, built on a custom engine. The repo is split into a reusable **engine** static library and a thin **game** executable that currently drives it through hand-written test scenes rather than a real gameplay loop.

## Build & Run

The project uses CMake. VS Code is configured to use the **Ninja** generator and copies `compile_commands.json` to the repo root for clangd.

```bash
# First-time setup: init the git submodules (glfw, miniaudio, nlohmann_json)
git submodule update --init --recursive
```

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

There is **no test framework**. The "tests" in `game/src/tests_scenes/` are interactive demo `main`-like functions (`ecs_test`, `piano_demo`, `file_management_test`, `batch_renderer_test`) declared in `game/include/test_funcs.hpp`. To switch which scene runs, edit the call in [game/src/main.cpp](game/src/main.cpp). Only one runs per build.

`DEBUG` is defined unconditionally in the root [CMakeLists.txt](CMakeLists.txt); logging macros compile to no-ops when it is absent. OS is injected as the `OS_NAME` macro (compared against `OS_WINDOWS`/`OS_MACOS`/`OS_LINUX`).

## Architecture

### Layering
`game` (executable) → depends on → `engine` (static lib) → depends on → `lib/*` (glfw, glad, stb, miniaudio, nlohmann_json). The game never touches third-party libraries directly. Include everything engine-side via the single umbrella header [engine/include/EngineInclude.hpp](engine/include/EngineInclude.hpp); everything lives in `namespace Engine`.

### Include-aggregator convention
Every subsystem folder has a `*Include.hpp` (e.g. `CoreInclude.hpp`, `GraphicsInclude.hpp`, `ComponentsInclude.hpp`) that re-exports the folder's public headers. When adding a new type to a subsystem, add its header to the corresponding `*Include.hpp` so it flows up to `EngineInclude.hpp`. Sources are glob-collected by CMake (`file(GLOB_RECURSE ... src/**.cpp)`), so new `.cpp` files are picked up on the next configure but **new files require re-running CMake configure**, not just build.

### Global managers are Singletons
Core services are accessed as `Xxx::get()` via the CRTP base [utils/Singleton.hpp](engine/include/utils/Singleton.hpp): `WindowManager`, `Renderer`, `Input`, `Time`, `Logger`, `ResourceManager`, `AudioManager`, `TilemapManager`, `TypeRegistery`. Constructors are private with `friend class Singleton<...>`.

### ECS (custom, EnTT-like)
Located in `engine/include/ecs/`. Not EnTT — a hand-rolled equivalent.
- `Registry` owns entities and one `SparseSet<T>` component pool per type, keyed by a runtime type id from `TypeRegistery`. It also holds singleton-style "contexts" (`setContext`/`getContext`).
- `EntityHandle` is the ergonomic wrapper: `registry.create()` returns a handle; use `.emplace<T>()`, `.get<T>()`, `.tryGet<T>()`, `.contains<T>()`, `.remove<T>()`. Multi-get returns a `std::tie` tuple.
- `View<Components...>` iterates entities having all listed components, yielding `[entity, comp&, ...]` structured bindings.
- Entities use a generation/free-list scheme; `destroyDeferred` + `flush` allow deferred destruction. Component pools expose `onCreate`/`onDestroy`/`onSet` signals (see `ecs/signals/`).

### Rendering (batch renderer + multi-window)
[graphics/Renderer.hpp](engine/include/graphics/Renderer.hpp) is a batched quad renderer (single Singleton). Typical per-frame flow, driven per-window:
1. `WindowManager::createWindow(...)` returns an `IdType`; cameras are entities with `TransformComponent` + `CameraComponent` (the camera's `windowId` binds it to a window).
2. Per window each frame: `setRenderWindowId(id)` → `setViewProjMat(registry)` → `beginPass()` → `setShader()` / `clearColor()` → `addQuad(...)` many times → a second `beginPass()` with a post-processing shader → `drawToWindow()` → `window->swapBuffers()`.
3. `GlfwContext::pollEvents()` once after all windows; close windows collected during iteration afterward (don't mutate the window list mid-loop).

Rendering goes through an offscreen FBO then a post-processing pass to the window (see `beginPass`/`drawToWindow`/`drawToBuffer` and the `gl_wrappers/` RAII wrappers: `GlBuffer`, `GlShader`, `GlTexture`, `GlFrameBuffer`, `GlVertexArray`, `GlLayout`). Shaders are single `.glsl` files containing multiple stages (see `game/assets/shaders/`).

### Other subsystems
- **Logging** ([core/logging](engine/include/core/logging)): use `LOG_INFO/LOG_WARNING/LOG_ERROR(fmt, ...)` (std::format-style). Sinks are pluggable (`ConsoleSink`, `FileSink`); add with `Logger::get().addSink<FileSink>(path)`. `setUseAsync(true)` enables async logging. Register a `std::formatter` for a custom type via the `DEFINE_TYPE_FORMATTER` macro. Never use `std::cout`/`std::cerr` directly — always go through the logging macros.
- **Input** ([core/input](engine/include/core/input)): `Input::get().update(windowId)` per window; axis-based via `addAxis("Name", {posKeys..., negKeys...})` then `getAxis("Name")`; also `keyPressed(KeyCode::...)`.
- **File management** ([core/file_management](engine/include/core/file_management)): typed file entries (`TextFile`, `BinaryFile`, `JsonFile`, `ImageFile`) under an `IFileEntry`/`Folder` tree via `FileManager`.
- **Audio** ([audio](engine/include/audio)): miniaudio-backed `AudioManager` with buses, streams, and instances (`piano_demo` scene exercises it).
- **Math** ([utils/math](engine/include/utils/math)): custom `Vec2/3/4`, `Mat4`, `Random`, `MathFuncs` — do not pull in GLM.

## Conventions
- Members prefixed `m_`; constants `ALL_CAPS`; types `PascalCase`; methods `camelCase`.
- 4-space indent, `{` on same line except function bodies, Allman-style class access labels (`  public:` indented). No `.clang-format` is committed — match the surrounding file.
- Class member ordering, top to bottom: private vars, public vars, public functions, private functions (see [Renderer.hpp](engine/include/graphics/Renderer.hpp)).
- IDs are `IdType` with `INVALID_ID` sentinel; prefer engine `TypeAliases.hpp` over raw ints.
- **Comments — strict rules, follow exactly:**
  - **Never** put a comment on a variable/member declaration or a function/method signature in `.hpp` files. Not even a short one. The name must carry the meaning — if it doesn't, rename it, don't comment it.
  - This applies everywhere a declaration could get a trailing or leading comment: class members, struct fields, function parameters, function declarations in headers. None of these get comments, ever, no exceptions.
  - Inside `.cpp` function bodies, comments are allowed only to explain something the code genuinely can't express: a non-obvious algorithm step, a tricky invariant, a magic number, or a workaround for a specific bug. One line, placed above the block it explains.
  - Do not comment obvious lines, restate what a line does, or narrate the code ("// loop over tiles", "// increment counter"). If you're unsure whether a comment is needed, leave it out.
  - Default assumption: **no comment**. Only add one when its absence would leave a future reader genuinely confused about *why*, not *what*.
