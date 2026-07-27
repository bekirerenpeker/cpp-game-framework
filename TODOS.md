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

- [ ] **Word wrapping + overflow** — `TextRenderer::measure` and the
  `walkSpan` advance arithmetic are already the right hooks; add a max-width to
  `TextStyle` (or a draw-in-rect entry point), break on word boundaries, and
  support clip/ellipsis overflow. Wrapping across a span boundary is the fiddly
  part: a `/s` span can start mid-word, so the break search has to run over the
  span list rather than one span's text.

- [ ] **Screen-space text pass** — `TextRenderer` currently always takes its
  matrix from `ViewContext`, so text lives in world space. UI wants a pixel
  ortho matrix (origin top-left). Add a space selector; switching spaces has to
  flush, so submit all world text then all UI text.

- [ ] **Plain quads in the text batch** — the shader's `unitRange == 0` branch
  and `TextVertex` already accommodate a non-distance-field quad. Adding a
  `drawRect` to `TextRenderer` would let a panel and its label share one draw
  call, which is the whole reason the batch is shaped this way. Probably the
  point at which `TextRenderer` becomes the UI renderer.

- [ ] **Named style tags** — the parser only knows `/s`. `TextTags::isTagAt`
  is the single extension point; `/b`, `/i` or `/color=red` slot in without
  touching the span-emitting loop or any call site.

- [ ] **Layout cache** — immediate mode re-walks every string every frame
  (~293 quads/frame in `ui_test` is fine, a full UI will not be). Key a cached
  span list + glyph positions on `hash(text, style, maxWidth)` with frame-age
  eviction.

- [ ] **Pack `TextVertex`** — 7 attributes and 68 bytes per vertex, with two
  full `Color`s. Pack both to RGBA8 and consider halving `unitRange`/`params`;
  same change as the `TileVertex` item above, so do them together.

## Later

- [ ] **UI system** — layout + widgets, built on top of text rendering and
  the sprite/quad batch. Custom-shader effects (9-slice, blur, etc.) go
  through `SpriteComponent::shader`, plus the material uniforms item above.
- [ ] **Docs pass** — no `docs/` currently exists; CLAUDE.md is the only
  source of truth. Once the above systems stabilize, either expand
  CLAUDE.md's "Other subsystems" section or split into a `docs/` folder per
  subsystem (rendering, tilemap, ECS, audio, input) with best-practices notes
  for both human and agent contributors.

## Done

- [x] **Text renderer** — `TextRenderer`
  ([TextRenderer.hpp](engine/include/graphics/text/TextRenderer.hpp)), a
  singleton owning a `BatchRenderer<TextVertex>` + `TextShader.glsl`, layered as
  four functions: `draw` (parses tags, delegates all drawing), `drawSpan` (the
  chaining primitive — takes a baseline pen, returns where the next glyph lands,
  handles `\n`/`\t`/kerning), `drawGlyph` (one quad, also usable directly), and
  `measure` (same walk, no geometry). `draw` takes a **top-left** origin;
  `drawSpan` works in baseline coords so pen hand-off between spans is exact.
  Styling is positional: `/s` opens and closes a span, `//s` is a literal `/s`,
  and an optional `TextStyle` list is matched to the spans in order — a short
  list silently falls back to the default, an unclosed tag styles to end of
  string (warned once, not per frame). One `TextStyle`
  ([TextStyle.hpp](engine/include/graphics/text/TextStyle.hpp)) for every layer
  and both atlas types; `drawGlyph` dispatches to `appendGlyphMtsdf`/
  `appendGlyphBitmap`, and the bitmap path zeroes `unitRange` which is exactly
  what makes `outlineWidth`/`boldness` no-ops in the shader. Everything a style
  varies is per-vertex, so **verified 293 quads in a single flush per frame**
  across 9 blocks, two font atlases and four styles. Added `Utf8`
  ([Utf8.hpp](engine/include/utils/Utf8.hpp)) because Latin-1 codepoints are
  multi-byte in UTF-8 and a byte-wise walk would never find the baked glyphs.
  Known and accepted: `"10 m/s"` opens a span — write `"10 m//s"`.

- [x] **Text effects** — `TextStyle` gained `outlineWidth`/`outlineColor`,
  `boldness`, `softness`, `shadowColor`/`shadowWidth`/`shadowSoftness`/
  `shadowOffset` (zero offset = centred glow), `italicSkew`, `underline` and
  `strikethrough`. Skew is a shear about the baseline and the decorations are
  solid quads (signalled by a negative `unitRange`, which also gives the shader
  the plain-fill branch a future `drawRect` needs), so all three work on bitmap
  fonts too. **All widths are em, not pixels** — they are converted to normalized
  field units on the CPU and folded into the field value before the screen-pixel
  conversion, which is what keeps them proportional under zoom; putting them in
  pixels made outline/boldness drift and made a wide glow flood the quad.
  `layerAlpha` in the shader uses a `smoothstep` bounded by `Font::MAX_FIELD_OFFSET`
  with a one-screen-pixel softness floor, so a transition provably reaches zero
  before the glyph quad's edge instead of leaving a faint tint. Effect width is
  therefore capped by the baked distance range (`Font::getMaxEffectEm()`); bake
  with a larger `distanceRangePixels` for wider outlines and glows.

- [ ] **Multi-line line height across spans is solved, wrapping is not** — a
  `\n` now steps by the tallest style that touched the line (`TextPen::maxLineStep`)
  and `draw`'s top-left origin clears the tallest style on the first line, so a
  big span no longer overlaps the next line. Word wrapping still has to respect
  the same per-line max.

- [x] **Font resource + msdf-atlas-gen baker** — `Font`
  ([Font.hpp](engine/include/graphics/text/Font.hpp)) is an `IResource` that
  turns a `.ttf`/`.otf` into a GPU atlas + glyph metrics. `FontAtlasType` is
  `Mtsdf` or `Bitmap` (both baked from the same font file; the type picks
  `Linear` vs `Point` filtering internally so a caller can't get it wrong) and
  `FontCharset` is `Ascii` or `AsciiLatin1`. Exposes `getGlyph`/`getKerning`
  and em-unit `FontMetrics` plus `getLineHeight(pixelSize)`-style helpers.
  `getUnitRange()` returns `distanceRange / atlasSize` for Mtsdf and
  `VEC2_ZERO` for Bitmap — the value a future MSDF shader wants per-vertex.
  `FontBaker` (behind the `ENGINE_BUILD_FONT_BAKER` CMake option, the only
  file touching msdf-atlas-gen) and `FontCache` both produce the same
  `FontData`, so the disk cache is the real interface: `.cache/fonts/<stem>-<hash>.png`
  + `.json`, read back with `ImageFile`/`JsonFile` and needing neither
  freetype nor msdfgen. Cache key hashes the bake settings + `BAKER_VERSION`
  but **not** the font contents, so a rebake overwrites its own entry instead
  of orphaning it; staleness checks size+mtime first and only rehashes
  contents on a mismatch (a git checkout keeps its cache). Writes go to
  `.tmp.png`/`.tmp.json` then rename, so a crash mid-bake can't leave a
  truncated file that reads as a valid hit. `evictStale` drops sibling entries
  baked from different bytes or an older `BAKER_VERSION` while leaving
  legitimate settings variants alone. `lib/` gained `freetype` and
  `msdf-atlas-gen` submodules. Verify with `ui_test`.

- [x] **Separate alpha blend func** — `GladContext::applyContextOptions` now
  uses `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
  Everything renders into a transparent-cleared FBO that is then composited to
  the window, and the old plain `glBlendFunc` multiplied source alpha in twice.
  Invisible on opaque sprites, but it eats antialiased edges — would have shown
  up as thin, dark-fringed text.

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
