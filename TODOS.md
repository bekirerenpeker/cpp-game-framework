# Engine TODOs

Working backlog for the engine, ordered by rough priority. Claude Code agents may
add, edit, reorder, check off, or remove items here as tasks are defined or
finished — keep it current, don't let it drift from reality. When an item is
done, move it to "Done" with a one-line note of what landed (not a changelog,
just enough to know it happened); when a plan changes, edit the item in place
rather than leaving a stale description.

## In progress / next up

- [ ] **UI interaction (one-frame-late hit testing)** — the layout solver lands
  boxes but nothing is clickable. Keep a persistent `id -> rect` map (hash the
  builder's call-site string, store in an `IdIndexedVector`) holding *last*
  frame's rect plus hover/press/focus, so `addButton` can return an
  `Interaction{hovered, pressed, released}` read from the previous frame and
  `if (ui.addButton(...))` works inline. Layout is stable frame to frame so the
  one-frame lag is invisible; a widget's first frame simply has no rect. Do not
  route this through `Delegate` — a return value has no lifetime problem and
  needs no per-widget object.

- [ ] **Hover/press styling** — split in two. Style-only changes (colour, border,
  alpha, and a *draw-time* scale about the element's centre) cost nothing and
  cannot oscillate; make them the default. Layout-affecting changes (padding,
  size, font size) are legal — hover state is known before the tree is built, so
  they just emit a bigger box — but they can feed back: the element grows, the
  growth moves it out from under the cursor, it un-hovers, shrinks, re-hovers,
  flickering at 60Hz. Hit-test against the *un-hovered* rect and animate the
  transition if this is ever opted into.

- [ ] **Grid** — a track list where each track carries the same `SizeSpec`, run
  through `LayoutCalculator`'s existing distribution routine, then row-major
  auto-placement into cells with an optional span. That is ~80% of grid's value
  for a fraction of CSS Grid's algorithm; auto-fit/minmax/dense packing are not
  worth it.

- [ ] **Widget rendering + theming** — `UiRenderer` draws text properly but its
  boxes are still depth-tinted debug outlines through the sprite batch. What is
  missing is a `UiStyle` -> draw mapping (background, border, radius) and a theme
  those styles resolve against instead of literal colours at call sites. Moving
  the rects into `TextRenderer`'s batch (see "Plain quads in the text batch"
  below) would then let a panel and its label share one draw call, which is the
  whole reason that batch has a solid-fill branch.

- [ ] **Scroll + clip** — `LayoutConfig::clipX/clipY` and `scrollOffset` are
  honoured by the solver already (a clipped axis reports `minW = 0`, which is
  what lets it shrink below its content), but nothing sets a scissor rect or
  drives the offset from input.

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

- [ ] **`TextRenderer` line stepping ignores the *next* line's ascent** —
  `walkSpan` steps `pen.pos.y -= pen.maxLineStep` where `maxLineStep` is the max
  over the line being *left*. That is right when the oversized span is on the
  current line, but a line whose content is much taller than the previous line's
  max rides up into it: 22pt line 1 gives a ~29 step, and a 46pt span opening
  line 2 has a ~35 ascender, so it overlaps line 1 by ~6 units. The fix is the
  one `TextMeasure::wrapSpans` already uses — accumulate a line *top*, and place
  each baseline at `lineTop + thatLine'sMaxAscent` instead of stepping
  baseline-to-baseline. Do this when folding the two walks together (item
  below), since that is where the two implementations converge anyway.

- [ ] **Fold `TextMeasure` and `TextRenderer::walkSpan` together** — wrapping now
  works end to end through the UI path (`TextMeasure::wrapSpans` -> `TextRun`s ->
  `TextRenderer::drawSpan`), but `TextRenderer::draw` on its own still only
  breaks on an explicit `\n`, so world-space text outside the UI cannot wrap.
  `TAB_SPACES` and `FALLBACK_SPACE_ADVANCE` are declared in both files and must
  not diverge or measured width stops matching drawn width; both belong on
  `TextStyle.hpp`. Ellipsis/clip overflow is still open.

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

- [ ] **Docs pass** — no `docs/` currently exists; CLAUDE.md is the only
  source of truth. Once the above systems stabilize, either expand
  CLAUDE.md's "Other subsystems" section or split into a `docs/` folder per
  subsystem (rendering, tilemap, ECS, audio, input) with best-practices notes
  for both human and agent contributors.

## Done

- [x] **UI renderer + styled/wrapped text** — `UiRenderer`
  ([UiRenderer.hpp](engine/include/graphics/ui/UiRenderer.hpp)) is the Singleton
  `UiSystem::draw()` hands the solved tree to; it replaced the throwaway
  `UiDebugDrawer` and owns the UI->world transform, the 1x1 white texture, the
  debug boxes and the text pass. `TextMeasure` went span-aware: `measureSpanWidths`
  and `wrapSpans` walk a `TextTags::parse` span list, so a UI text element takes
  `/s` tags plus a style list exactly like `TextRenderer::draw` does. `wrapSpans`
  emits `TextRun`s carrying x **and the baseline y**, so `UiRenderer` renders by
  calling `TextRenderer::drawSpan` per run with no second walk of the string.
  **Line height is the max over every style on that line** (the CSS line-box
  rule) — verified as steps `[29.3, 61.2, 29.3, 29.3]` for a block whose second
  line holds a 46pt span among 22pt body text, and a 13pt span correctly failing
  to shrink its line. `addText(..., fixedLineHeight = true)` opts out and steps
  uniformly (same block: `[29.3 x4]`, 117.0 tall instead of 149.0) for the case
  where one oversized word should not push its line apart. Wrapping backtracks
  to the last space even when it is several spans back, and a word may straddle
  a span boundary. Only `TextStyle::size` is scaled UI->world; every other
  measurement is em and scales itself. Verified identical layout under both the
  MTSDF and Bitmap atlas (Tab in `ui_layout_test`), which is the check that the
  pipeline is font-agnostic.
  Note: `TextRenderer`'s own `TextPen::maxLineStep` does take the max across
  every span touching a line, but it steps by the max of the line it is
  *leaving* — see the separate item below for the case that still breaks.

- [x] **UI layout solver** — `LayoutCalculator`
  ([LayoutCalculator.hpp](engine/include/graphics/ui/layout/LayoutCalculator.hpp)) plus an
  immediate-mode `UiSystem` singleton
  ([UiSystem.hpp](engine/include/graphics/ui/UiSystem.hpp)) under the new
  `graphics/ui/`. Five sequential passes over a **flat preorder array** — so
  `for (i = n; i-- > 0;)` is the bottom-up pass and `for (i = 0; ...)` is
  top-down, with no recursion anywhere: intrinsic widths -> final widths ->
  intrinsic heights -> final heights -> position/align. The axes are two
  separate cycles rather than one, because a text leaf's height is a *function
  of* its final width; width never reads height, so nothing loops and no
  iteration-to-convergence is needed. Distribution is one routine shared by
  passes 2 and 4, parameterized by axis: surplus levels the smallest growers up,
  a deficit levels the largest children down toward their `contentMin`.
  `LayoutCalculator` holds its **own** node struct carrying only layout data and a
  `sourceIndex` back to `UiElement`, so styling never enters the solver and the
  returned array is pure geometry. Leaves plug in through `ILeafMeasurer`
  (`measureWidths` / `measureHeight(contentWidth, lines)`) — `TextLeaf` and
  `ImageLeaf` ship; the leaf objects live in reusable pools on `UiSystem`
  because `LayoutInput::measurer` is a bare pointer held from `addText` all the
  way through pass 3. Layout space is **Y-down, top-left origin, unitless**;
  the single Y flip lives in `UiRenderer::uiToWorld`. Verified numerically in
  `ui_layout_test` across all 10 cases (75 nodes): Fit row = 222 exactly, two
  growers 234/234, `sizing.max` capping at 120 with the leftover going to
  `alignMain`, a floating child leaving its parent at 222 (identical to the same
  row without one), an image at 388x194 from a 2:1 aspect, and text shrunk to
  120 wrapping to 7 lines.
  Gotchas that are handled and must stay handled: `Grow` seeds at **max**-content
  (seeding at min lets two growers in a `Fit` parent split evenly, wrapping the
  long one while the short one keeps slack); floating children are excluded from
  the sum, the max **and** the `n-1` gap fence-post; a clipped axis reports
  `minW = 0` or a scroll container could never shrink; `NO_NODE` is `(uint)-1`
  and deliberately **not** `INVALID_ID`, because index 0 is the root.
  Deviation from plan worth knowing: `Percent` aggregates upward like `Fit`
  rather than contributing 0, which is what CSS does for percentages in
  intrinsic sizing and removes a silent collapse-to-zero inside a `Fit` parent.

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
