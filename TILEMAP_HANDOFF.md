# Tilemap system — handoff

_Written for the next agent/developer picking up the tilemap work. Snapshot of the
system as of this session. Everything lives under the tilemap feature except the
cross-cutting GL-wrapper change (§6) and the formatting hook (§9)._

---

## 1. TL;DR

The tilemap stores tiles in a sparse map of **32×32 chunks**. This session wired the
data model to the GPU: it added the missing `tileId → UV` lookup, linked a tilemap to a
`Tileset`, and implemented **`TilemapManager::render()`**, which **builds/uploads a
per-chunk mesh (VAO+VBO) for every dirty chunk** — but **does not draw anything yet**.
Actual drawing is intentionally deferred to a future dedicated batch renderer (§7).

If you're here to make tiles appear on screen, you're implementing §7.

---

## 2. What this session changed

**Closed the holes that blocked meshing:**
- `Tileset` tiles now get a real **id** on creation and there's an O(1) `id → UV` lookup.
  Previously `TileDefinition.id` was never assigned and tiles were keyed only by name, so
  there was no way to turn a `TileData.textureId` into a UV rect.
- `TilemapComponent` now references a `Tileset*` (was an uninitialized raw `GlTexture*`).
- Fixed a `getChunkKey` mask typo (`0xFFFFFFFFFFF` → `0xFFFFFFFF`).
- Established the convention **`textureId == 0` means empty** (skipped when meshing).

**New rendering-prep pass:**
- `TilemapManager::render()` + `setTileset()` + a debug `totalIndexCount()`.
- `TilemapChunk` GL objects are now **lazily created** inside `render()` (see §5), so pure
  tile mutation (`setAt`) needs no live GL context.

**Cross-cutting:** GL wrapper base class made **move-only / copy-safe** (§6).

**Tooling:** a `clang-format` auto-format hook replaced an earlier one-off skill (§9).

---

## 3. Files

| File | Role |
|---|---|
| [engine/include/components/TilemapComponent.hpp](engine/include/components/TilemapComponent.hpp) | `TileData`, `TileVertex`, `TilemapChunk`, `TilemapComponent` |
| [engine/include/graphics/tilemap/Tileset.hpp](engine/include/graphics/tilemap/Tileset.hpp) / [.cpp](engine/src/graphics/tilemap/Tileset.cpp) | Tileset: slices a texture into tiles, id/UV lookup |
| [engine/include/graphics/tilemap/TilemapManager.hpp](engine/include/graphics/tilemap/TilemapManager.hpp) / [.cpp](engine/src/graphics/tilemap/TilemapManager.cpp) | Singleton: mutation (`setAt`/`getAt`/`clear`), invalidation, `render()` |
| [game/src/tests_scenes/tilemap_test.cpp](game/src/tests_scenes/tilemap_test.cpp) | Manual test scene (§8) |

Everything is in `namespace Engine`. Reach it via the umbrella `EngineInclude.hpp` (the
tilemap headers flow up through `GraphicsInclude.hpp`), but include
`graphics/tilemap/Tileset.hpp` explicitly when you need the full `Tileset` type — the
component only forward-declares it.

---

## 4. Data model & coordinate math

- **`TileData`** = `{ uint16_t textureId; uint16_t flags; }`. `textureId` indexes into the
  tilemap's `Tileset` (id 0 = empty). `flags` is currently unused (room for rotation/
  animation/rule-tile state later).
- **`TilemapChunk`** = a fixed `32×32` (`CHUNK_SIZE`) array of `TileData`, its grid position
  `(chunkX, chunkY)`, plus its GPU handles (`vbo`, `vao`, `indexCount`) and an `isDirty`
  flag. Chunks live in `TilemapComponent::m_chunks`, an
  `unordered_map<uint64_t key, TilemapChunk>` keyed by `getChunkKey(cx, cy)` (two int32
  chunk coords packed into a u64).
- **Coordinate spaces:** global tile `(x, y)` → chunk `(cx, cy)` via `gridToChunkCoord`
  (floor division that handles negatives) → local `(lx, ly)` in `[0, 32)`. World position of
  a tile corner is `chunkX*32 + lx` (same for y); tiles are 1 world-unit squares.
- **Chunks are created lazily** on first `setAt` into them, and **never removed** (no GC of
  emptied chunks yet — see §7 gaps).

`TilemapComponent`'s members are private with `friend class TilemapManager` — all access
goes through the manager. It has no copy (holds move-only `unique_ptr` chunk GL objects),
which is fine because it's used as an ECS component (constructed in place), but note
`Registry::clone` would not compile for an entity holding a `TilemapComponent`.

---

## 5. The render/build pass (what `render()` actually does)

`TilemapManager::render(TilemapComponent&)`:
1. Warns and returns if no tileset is assigned.
2. Lazily builds a **shared static index buffer** (`m_indexBuffer`, the `0,1,2, 0,2,3`
   pattern) sized for the max `32*32` quads — reused by every chunk's VAO.
3. For each chunk where `isDirty`, calls `buildChunk`.

`buildChunk(chunk, tileset)`:
- Walks the 32×32 tiles, **skips `textureId == 0`**, resolves UVs via
  `tileset.getTile(textureId)`, and appends 4 `TileVertex` per non-empty tile. Tiles are
  **compacted** (only non-empty ones emitted), so the shared sequential index buffer stays
  valid; `indexCount = nonEmptyTiles * 6`.
- **Lazily creates** `chunk.vao`/`chunk.vbo` on first build and records the layout once:
  binds vao → binds vbo → `setLayout({Float2, Float2, Float4})` → binds the shared index
  buffer (its element-buffer binding is captured in VAO state). Then uploads the vertex data
  and clears `isDirty`.

**`TileVertex`** = `{ Vec2 pos; Vec2 uv; Color color; }` (32 bytes). No per-vertex texture
index (a tilemap binds one tileset texture). `color` is written as white today — it exists so
tint/lighting can be added without changing the vertex format or the eventual shader.

After `render()`, each dirty chunk holds a ready-to-draw static mesh: bind `chunk.vao`, bind
the tileset texture, `glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_INT, 0)`.

**Dirty tracking:** `setAt` marks a chunk dirty only when a tile's value actually changes,
and also marks the neighboring chunk dirty when the edited tile is on a chunk edge (via
`invalidateChunk`) — this is groundwork for rule/auto-tiles whose appearance depends on
neighbors. `buildChunk` does **not** yet read neighbor data; edge invalidation is currently a
no-op in terms of visual output.

---

## 6. GL wrapper move-safety (cross-cutting — read if you touch `gl_wrappers/`)

[IGlObject](engine/include/graphics/gl_wrappers/IGlObject.hpp) previously had an implicit copy
ctor, so copying any `GlBuffer`/`GlTexture`/`GlVertexArray`/`GlShader`/`GlFrameBuffer` would
double-`glDelete` its GL name. It is now **move-only**: copy is deleted; move is a
`std::swap` of the GL id (the moved-from object deletes whatever id it ends up holding via its
own derived destructor). The four value-safe wrappers (`GlBuffer`, `GlVertexArray`,
`GlTexture`, `GlShader`) `=default` their move ops.

**`GlFrameBuffer` is deliberately non-movable** (no move ops added): it owns extra resources
(`m_colorAttachment`, `m_depthAttachment`, a `GlTexture*`) that a defaulted swap-move would
double-free. It's only ever heap-allocated and held by pointer, so this is safe. If you ever
need to move one, write a proper hand-rolled move, don't `=default` it.

Practical impact: you can hold these wrappers by value in containers now, but a class holding
one by value becomes non-copyable. Nothing in the codebase copied them (verified:
`ResourceManager` stores `IResource*` heap pointers; `Renderer`/`Tileset` hold them as
never-copied members).

---

## 7. NOT done yet — the drawing / batch renderer (your likely next task)

`render()` only prepares meshes. There is **no draw path and no tilemap shader yet**. The
plan is a `TilemapRenderer` **separate from the main `Renderer`** (the main one is a general
quad batcher; the tilemap draws pre-built static per-chunk meshes, a different pattern).

Suggested shape:
- A `graphics/tilemap/TilemapRenderer` (singleton, like `Renderer`) owning a tilemap shader
  (single sampler2D + `uMVP`). Add a `game/assets/shaders/TilemapShader.glsl` matching the
  `{pos(2), uv(2), color(4)}` layout. See existing shaders in `game/assets/shaders/` and the
  `#shader vertex` / `#shader frag` split parsed by [GlShader](engine/src/graphics/gl_wrappers/GlShader.cpp).
- Per window/frame: call `TilemapManager::render(tilemap)` (rebuilds dirty chunks), then for
  each chunk bind the tileset texture (`tilemap`'s `Tileset::getTexture()`), set `uMVP`
  (reuse the camera view-proj like `Renderer::setViewProjMat` does), bind `chunk.vao`, and
  `glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_INT, 0)`. Skip chunks with
  `indexCount == 0`.
- The tilemap will need a world transform + which tileset/entity it belongs to. Decide how a
  tilemap entity is represented (likely `TransformComponent` + `TilemapComponent`, iterated
  with a `View`) — not yet defined.

**Watch out for:**
- **UV vertical flip.** `buildChunk` uses the main renderer's corner→UV convention
  (bottom-left = `uvMin`). Depending on how the tileset PNG loads (stb, top-left origin), tiles
  may render vertically flipped. Fix in the shader or by flipping V in `buildChunk` once you
  can see output. `TilesetFloorB.png` tiles are **16×16 px**.
- **Culling:** none yet. Fine for small maps; add chunk frustum culling before large worlds.
- **Chunk lifecycle:** chunks are never freed, and a chunk that becomes all-empty still keeps
  its (empty) mesh. Add GC if memory matters.
- **`m_tilesById` is per-`Tileset`**; the tilemap trusts that its `textureId`s came from the
  assigned tileset. `getTile` returns `nullptr` for unknown ids and `buildChunk` skips them.

---

## 8. Testing

There is no test framework; `game/src/tests_scenes/*.cpp` are manual demo `main`s selected by
editing the call in [game/src/main.cpp](game/src/main.cpp). This session added
`tilemap_test()` ([tilemap_test.cpp](game/src/tests_scenes/tilemap_test.cpp), declared in
[test_funcs.hpp](game/include/test_funcs.hpp)).

It creates a window (for the GL context), builds a `Tileset` from
`game/assets/images/TilesetFloorB.png` via `fromCellSize("tile", 16, 16)`, sets 5 tiles
spanning **3 chunks** — `(0,0)`, `(1,0)`, `(-1,-1)` — calls `render()`, and asserts
`totalIndexCount() == 30` (5 tiles × 6) with a clean `glGetError()`; a second `render()` must
rebuild nothing. Point `main.cpp` at `tilemap_test()`, then build and run **from the repo
root** (assets use relative paths). A new `.cpp` requires re-running CMake configure
(sources are globbed). Note: `fromCellSize` does not skip blank cells in the atlas yet — known,
ignored for now.

_Build/run is the human's job in this workflow — don't assume you should run it._

---

## 9. Formatting hook (good to know)

A `PostToolUse` hook ([.claude/settings.json](.claude/settings.json) →
[.claude/hooks/format-edited.py](.claude/hooks/format-edited.py)) runs `clang-format -i` on
C/C++ files you edit (skipping `lib/` and `build/`). The project's `.clang-format` lives one
directory **above** the repo root and is auto-discovered. So don't hand-format to match style —
the hook does it. (It only activates after Claude Code reloads settings, i.e. a restart or
opening `/hooks` once.)
