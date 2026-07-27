# Engine TODOs

Working backlog for the engine, ordered by rough priority. Claude Code agents may
add, edit, reorder, check off, or remove items here as tasks are defined or
finished — keep it current, don't let it drift from reality. When an item is
done, move it to "Done" with a one-line note of what landed (not a changelog,
just enough to know it happened); when a plan changes, edit the item in place
rather than leaving a stale description.

## In progress / next up

- [ ] **Terrain-type tilemap rework** — `Tileset::tilesConnect(a, b)` is
  currently just `a != 0 && a == b` ([Tileset.cpp](engine/src/graphics/tilemap/Tileset.cpp)),
  so a rule tile only autotiles against itself. Add a terrain-type concept
  decoupled from the visual tile id (e.g. `terrainType` on `TileData`), make
  `tilesConnect` compare terrain types, and support transition/blend tiles
  keyed by a `(terrainA, terrainB)` pair for edges between two terrains
  (grass/dirt/water blending, à la Terraria/RimWorld/Unity Rule Tiles). This
  is a data-model change — do it before building real tilemap content on the
  current single-id scheme.

- [ ] **Tilemap vertex/perf pass** — `TileVertex`
  ([TilemapComponent.hpp](engine/include/components/TilemapComponent.hpp))
  is `Vec2 pos, Vec2 uv, Color color, int texIndex`; `Color` is likely 4
  floats, so each vertex is large and full chunk meshes get rebuilt on any
  dirty tile. Pack color to `uint32_t` RGBA8, shrink `texIndex`, consider a
  more compact position/uv encoding. Bundle with other `TilemapRenderer`
  chunk-rebuild optimizations (partial rebuild instead of full
  `CHUNK_SIZE^2` rebake where feasible). Do alongside the terrain-type
  rework since both touch `TileVertex`/`TilemapRenderer`.

- [ ] **Per-sprite material uniforms** — `SpriteComponent::shader` covers
  "draw this sprite with a different shader", but there is still no way to
  feed a custom shader its own uniforms (tint strength, outline width, …).
  Needs a material concept — a shared uniform set stored once and referenced
  by sprites, applied on the shader switch in `Renderer::renderSprites` —
  rather than per-entity uniform blobs.

## Later

- [ ] **Text rendering** — glyph atlas + text batch (likely its own
  `BatchRenderer<TextVertex>` instance, following the `TilemapRenderer`
  pattern), font loading via `ResourceManager`/`FileManager`.
- [ ] **UI system** — layout + widgets, built on top of text rendering and
  the sprite/quad batch. Custom-shader effects (9-slice, blur, etc.) go
  through `SpriteComponent::shader`, plus the material uniforms item above.
- [ ] **Docs pass** — no `docs/` currently exists; CLAUDE.md is the only
  source of truth. Once the above systems stabilize, either expand
  CLAUDE.md's "Other subsystems" section or split into a `docs/` folder per
  subsystem (rendering, tilemap, ECS, audio, input) with best-practices notes
  for both human and agent contributors.

## Done

- [x] **Per-sprite shaders** — `SpriteComponent` gained a `GlShader* shader`
  (`nullptr` = whatever shader is bound on the `Renderer`).
  `Renderer::renderSprites` now sorts by `(layer, shader, texture)`, flushes
  the batch on a shader boundary, and restores the pass shader when done.
  `BatchRenderer::getShader()` added so the pass shader can be read back.

- [x] **Application/loop wrapper** — added `Engine::Application`
  ([Application.hpp](engine/include/core/Application.hpp) +
  [Application.cpp](engine/src/core/Application.cpp)), wired into
  `CoreInclude.hpp`. Holds three `Delegate`s (`onFrame`, `onWindowUpdate`,
  `onWindowRender`) bound via `app.onFrame().bind(&fn)`; `run()` is a plain
  non-template function. Owns the `anyWindowOpen` loop, `Time::update`,
  per-window `ViewContext::setActiveWindow`/`Input::update`, close-window
  bookkeeping (incl. `setCloseOnEscape`), `ViewContext::updateCamera`
  ordering, `swapBuffers`, and `GlfwContext::pollEvents` — render body stays
  fully scene-owned. Unbound phases are skipped, so each is optional.
  Migrated `tilemap_test.cpp` and `batch_renderer_test.cpp`.

- [x] **`Delegate` binds plain callables** — added a
  `bind(C* callable)` overload to
  [Delegate.hpp](engine/include/ecs/signals/Delegate.hpp) that binds a
  lambda or functor through its `operator()`, alongside the existing
  free-function and member-function `bind`s. Constrained with a `requires`
  clause so a signature mismatch errors on the `bind` line naming the
  expected argument list, instead of failing deep inside the stub. Stores
  only a pointer, so the callable must outlive the delegate — taking it by
  pointer is what prevents binding a temporary. Generic (`auto`-parameter)
  lambdas are not supported: their `operator()` is a template with no single
  address to take.
