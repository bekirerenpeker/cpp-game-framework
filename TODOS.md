# Engine TODOs

Working backlog for the engine, ordered by rough priority. Claude Code agents may
add, edit, reorder, check off, or remove items here as tasks are defined or
finished — keep it current, don't let it drift from reality. When an item is
done, move it to "Done" with a one-line note of what landed (not a changelog,
just enough to know it happened); when a plan changes, edit the item in place
rather than leaving a stale description.

## In progress / next up

- [ ] **Engine-handled resizable containers** — `UiState` already reports
  `isHeld` + `dragDelta` + `mouseLocal` + last frame's rect, which is all a scene
  needs to resize a panel itself (see case 0 in `ui_layout_test`). Doing it
  *inside* the engine needs three things that are not free: a second hit pass
  that prefers a container's resize border over its own children (under
  "topmost wins" the border loses the hit to any overlapping child, so the
  container never becomes active), a long-lived `key -> size` map with its own
  frame-age eviction since immediate mode has no destroy event, and a builder
  that overwrites the caller's `LayoutConfig.width/height` with
  `SizeSpec::fixed(...)`, which breaks the rule that `LayoutConfig` is purely
  the caller's data. Only worth it once several call sites want it.

- [ ] **Grid** — a track list where each track carries the same `SizeSpec`, run
  through `LayoutCalculator`'s existing distribution routine, then row-major
  auto-placement into cells with an optional span. That is ~80% of grid's value
  for a fraction of CSS Grid's algorithm; auto-fit/minmax/dense packing are not
  worth it.

- [ ] **Widget theming** — `UIRenderer` draws every paintable `UIContainerStyle`
  field for real now, but there are no presets and no *theme*: a named palette +
  role mapping a style resolves against, so a call site says "surface" or
  "accent" rather than a literal `Color`. Presets (`panel`/`button`/`outline`/
  `divider`) come first since they are what a theme would resolve. Only then is
  restyling a whole UI one edit.

- [ ] **Scroll + clip** — `LayoutConfig::clipX/clipY` and `scrollOffset` are
  honoured by the solver already (a clipped axis reports `minW = 0`, which is
  what lets it shrink below its content), but nothing sets a scissor rect or
  drives the offset from input. Hit testing must be clipped in the same change,
  not before it: `UiSystem`'s cached rects are currently full rects, and
  clipping the hit test while the renderer still draws everything unclipped
  would produce visible elements that cannot be clicked — a worse bug than the
  one it fixes.

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

- [ ] **Text overflow: clip + ellipsis** — `TextOverflow` is declared on
  `TextBlock` and threaded through the setters (it dirties layout), but nothing
  honours it: every value behaves as `Visible`. `Clip` is a filter in
  `TextRenderer::draw`'s run loop against the `boxSize` it already takes, using
  `TextLine::top`/`height`, plus a scissor for the partially visible line.
  `Ellipsis` additionally needs a `maxLines` consumed by `calculate`'s
  `closeLine` and one synthesized trailing run, backtracking glyph-by-glyph until
  the ellipsis fits. `TextLine::firstRun`/`runCount` exist so neither needs a
  second walk of the runs.

- [ ] **One batch for UI rects and UI glyphs** — the UI now submits two batches
  per root (rounded rects through `UIBoxShader`, glyphs through `TextShader`), so a
  panel and its label cost two draw calls. They cannot merge as things stand:
  the rect shader is a rounded-box SDF with its own vertex layout and the glyph
  shader is a distance-field sampler. Merging means one shader branching on a
  per-vertex mode flag and one vertex format wide enough for both — worth it
  only once a real UI shows the draw calls actually matter.

- [ ] **Named style tags** — the parser only knows `/s`. `TextTags::isTagAt`
  is the single extension point; `/b`, `/i` or `/color=red` slot in without
  touching the span-emitting loop or any call site.

- [ ] **Cross-block layout cache** — the per-block half landed with `TextBlock`:
  a block caches its parse, its min/max widths and its solved runs, and
  `calculate` early-outs unless a setter dirtied it or the width actually
  changed, so a steady-state frame re-walks nothing. What is still open is
  sharing across blocks — key a cached span list + glyph positions on
  `hash(text, style, maxWidth)` with frame-age eviction, so N identical labels
  cost one walk rather than N.

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

- [x] **Containers paint through `UIRenderer`, not a debug quad** —
  [UIRenderer.hpp](engine/include/graphics/ui/UIRenderer.hpp) is a Singleton
  owning a `BatchRenderer<UIBoxVertex>` over
  [UIBoxShader.glsl](game/assets/shaders/UIBoxShader.glsl), and `UINode::draw`
  forwards a container to `addContainerQuad(drawPos, size, style)` instead of
  `Renderer::addQuad`. **One quad carries the whole container**: background
  (colour, image, both), border ring, corner radius, dash pattern and shadow all
  come out of a single rounded-box SDF evaluated per fragment, so no style field
  costs extra geometry. Every field is per-vertex, never a uniform — a uniform
  would cost one draw call per style — and every varying except the local
  position is `flat`, since they are per-quad constants.
  Things that had to be right: the border's inner edge is the *same* distance
  field offset by `borderWidth` (offsetting an SDF inserts the correctly shrunken
  corner radii for free, so 4 border quads are never emitted); the quad is padded
  out by `blur + |offset| + AA_PADDING` so the shadow has somewhere to live and
  the box's own antialiased fringe is not cut off by the edge it is smoothing,
  which is why the fragment reads uv off `halfSize` rather than the interpolated
  corners; antialiasing is `fwidth` of the distance itself, so an edge stays one
  screen pixel at any zoom; the shadow is knocked out under the box the way CSS
  does it, or a translucent background shows its own shadow through itself; and
  `shadowOffset.y` is negated on the way in, because the style names it Y-down
  like CSS while every geometric field past that point is the renderer's Y-up
  draw space. Dashes walk **arc length** around the rounded boundary (straight
  runs plus quarter-arcs, first quadrant measured and the other three mirrored
  onto it), with the period divided into a whole number of repeats **CPU-side**
  so the pattern closes on itself instead of leaving a stub at the seam;
  `UIBorderStyle::Dashed`/`Dotted` are just two period-to-width ratios, so the
  shader stays generic.
  A container whose fill, border and shadow are all invisible emits **zero
  geometry**, so layout-only containers cost nothing — but note this also means a
  container with no style at all is now invisible, where the debug quad used to
  paint it white. `ui_test` is now a gallery of all 24 style combinations.
  Not covered, and not a quad's business: `overflow` needs a clip rect from the
  solver, `zIndex` reorders the walk, `cursor` and `transition` are input and
  animation concerns.

- [x] **Text layout folded out of `TextRenderer`** — wrapping, line boxes and
  alignment now live in one stateless `TextLayoutCalculator`
  ([TextLayoutCalculator.hpp](engine/include/graphics/text/TextLayoutCalculator.hpp))
  over a `TextBlock` ([TextLayout.hpp](engine/include/graphics/text/TextLayout.hpp))
  that carries **input and solved output together**. Two entry points replace
  `measure()`: `measureMinMaxWidth` and `calculate(block, width)`. The algorithm
  is the deleted `TextMeasure::wrapSpans` ported verbatim — the invariants that
  had to survive are the CSS line-box rule, the retroactive baseline patch in
  `closeLine`, the last-space backtrack across span boundaries, and a word being
  allowed to straddle a `/s`.
  This closed two open items at once. `TextRenderer::walkSpan`, `measure`,
  `parseSpans`, `firstLineSize`, `pickStyle` and the shared `m_spans` scratch are
  gone; `drawGlyph` became the private `appendGlyph` (its return value was a
  second, kerning-less copy of the advance rule). **World-space text can wrap
  now**, and the line-stepping bug is fixed structurally rather than patched:
  baselines are placed at `lineTop + thatLine'sAscent`, so a tall line can no
  longer ride up into the one above — verified as gaps of 2e-8 and 7e-9 between
  three lines of heights `[0.319, 0.452, 0.213]` (the 0.24 and 0.34 spans driving
  lines 1 and 2), collapsing to `[0.213 x3]` under `fixedLineHeight`.
  The divergence hazard the old code was flagged for is structurally closed:
  `TAB_SPACES`, `FALLBACK_SPACE_ADVANCE` and the whole codepoint→advance rule
  live once in [TextMetrics.hpp](engine/include/graphics/text/TextMetrics.hpp),
  and **both** the measuring and emitting walks call `TextMetrics::step`. That
  also fixed two real divergences: `\t` now resets the kerning pair everywhere
  (`wrapSpans` used not to), and `\r` never does.
  Non-obvious things that had to be right: `TextBlock` is **neither copyable nor
  movable**, because its runs hold `string_view`s into its own `std::string` and a
  short string moves by copying into the destination's SSO buffer, silently
  dangling every view — deleting all four turns `std::vector<TextBlock>` into a
  compile error instead of a heisenbug; `setText` clears runs **eagerly** so no
  window exists where a run points into a freed buffer; **horizontal** alignment
  is baked into run offsets at `calculate` (the width is final there) while
  **vertical** is applied in `TextRenderer::draw` (only the caller knows the box
  height); and the calculator holds **no mutable state**, so unlike the `measure()`
  it replaces it is safe to call mid-draw.
  Verified from `ui_test`'s startup proof in `game/output/log.txt`: wrap at
  1/2/4/12 of max-content gives 1/3/5/14 lines with `bounds.x` always within
  budget; `min = max` with wrapping off; and a two-line block at Left/Center/Right
  yields **different offsets per line** (`0`/`1.106`/`2.213` vs `0`/`0.166`/`0.331`),
  which is what proves alignment is per line and not per block. `UITextLeafData`
  is three one-line forwards onto the calculator.

- [x] **Dividers + fully overridable container styling** — `UiSystem::addDivider(thickness, styles, id)`
  pushes a leafless container with `SizeSpec::grow()` width and `SizeSpec::fixed(thickness)`
  height, so it is a full-width rule in a column and takes the leftover main-axis
  space in a row. `Grow`, deliberately **not** `Percent(100)`: a percent is measured
  against a parent content box that does not exist yet when the parent is `Fit`, so
  the rule would collapse there. It paints as a **background fill**, not a border —
  the shader insets a border from both edges, so a 1-unit rule with a 1-unit border
  draws itself twice; `UiPresets::divider(tint)` is the matching preset.
  `UiStyleOverride` gained `cornerRadius`, so every `UiStyle` field is now
  per-state overridable (it is purely visual, so it does not break the
  style-only rule that keeps an element from resizing out from under the cursor).
  `cornerRadius` itself already worked — the CLAUDE.md note saying it needed shader
  work was stale from before the SDF shader landed. Verified in `ui_layout_test`
  case 15: three rules at 1/3/8 thick, all 344 wide inside a 360 column, the 8-thick
  one rounding into a capsule because the shader clamps the radius to the shorter
  half-extent.

- [x] **UI renders in window space, from a rounded-rect shader** — `UiRenderer`
  owns a `BatchRenderer<UiVertex>` over
  [UIBoxShader.glsl](game/assets/shaders/UIBoxShader.glsl) instead of borrowing the
  sprite batch, and `setViewport(screenTopLeft, pixelsPerUiUnit)` places a root
  in **window pixels**, so the camera no longer moves or scales the UI. Every
  container is one quad whose fragment shader evaluates a rounded-box signed
  distance field, which is where `cornerRadius`, `borderWidth`/`borderColor` and
  antialiased edges all come from — no texture is bound at all, hence
  `BatchRenderer::nextQuad()` (the texture-less overload) and a guard so `flush`
  skips the `uTextures` lookup a texture-less shader never declares, which would
  otherwise warn every frame since missing locations are deliberately not cached.
  A container with no visible background *and* no visible border emits **zero
  geometry**, so layout-only containers — most of them — cost nothing.
  Text joined the same space through `TextRenderer::setViewProjOverride`, which
  flushes on change. The projection stays **Y-up** (`ortho(0, w, 0, h)`) rather
  than the more obvious Y-down: glyph quads are built baseline-up, so a Y-down
  matrix renders every string mirrored. The single Y flip therefore stays in
  `UiRenderer::uiToScreen`, exactly where `uiToWorld` used to hold it, and hit
  testing needs no flip at all now that client pixels and layout units share an
  orientation — which also dropped `UiSystem`'s whole camera dependency
  (`getMouseWorldPos`, the world-origin probe, the no-camera phantom-hover
  guard).
  The debug-box pass is gone: `UiStyle` is the only thing drawn.
  `UiPresets::panel/button/outline` give starting points. `ui_layout_test` styles
  its demo boxes rather than relying on debug outlines, and WASD/QE now pan and
  zoom the UI viewport instead of the camera.

- [x] **UI interaction + hover/press styling** — every builder now returns a
  `UiState` ([UiInteraction.hpp](engine/include/graphics/ui/UiInteraction.hpp))
  read from the **previous** frame's solved rects, so
  `if (ui.addButton("Save", {}, styles).isClicked)` works inline. Identity is a
  64-bit FNV-1a chain, `key = hash(parentKey, id)`, with the id being an explicit
  string when given and the sibling ordinal within the parent otherwise; the root
  seeds from the window id plus a per-frame root ordinal. `addButton` uses its
  label as the id, so two same-labelled buttons under one parent share state —
  pass an explicit id there. A dynamic list needs explicit ids for the same
  reason; ordinals only hold while the tree shape does.
  `UiSystem` keeps two rect buffers swapped at the frame boundary, and `draw()`
  **appends** to the write buffer rather than clearing it, because one frame can
  hold several `begin`/`draw` cycles. Rects stay in layout units with a parallel
  per-root `{windowId, screenTopLeft, pixelsPerUiUnit}`, so the mouse is converted
  once per root instead of every rect being converted to world space.
  Three things had to be right or this silently misbehaves: the boundary keys on
  **`(Time::getFrameCount(), activeWindowId)`** — the buffer swap on frame, the
  mouse sample / hover resolve / press machine on window — because `Time::update`
  runs once for all windows while `Input`'s cursor and button edges are
  per-window; mouse **deltas come from screen pixels**, not world, or panning the
  camera folds into the drag; and hit resolution walks the cached rects
  **backwards** because that is exactly the reverse of `UiRenderer`'s paint order
  (a z-index or floating-last pass there would have to be mirrored here).
  `isHovered` is ancestor-inclusive (a button hovers when the cursor is on its
  label) and `isHoveredDirect` is the exact hit; text is never hit-testable so a
  label cannot steal its button's hit. `m_activeKey` survives the cursor leaving
  the element, which is what makes dragging work, and is cleared on release even
  when the element was not rebuilt that frame.
  Styling resolves `normal -> hovered -> pressed` inside the builder, from
  `UiStyleOverride`'s `std::optional` fields, and is **style-only** on purpose —
  a size or padding override would grow the element out from under the cursor and
  flicker between states at 60Hz. Label colour comes from the resolved
  `contentColor` and has to be baked into the `TextStyle` *before* `TextLeaf::set`,
  since `UiRenderer` reads a text element's style off its leaf, not off
  `UiElement::textStyle`. `Time` gained a frame counter for the boundary; the
  `UiStyle` -> draw mapping landed with it, since hover styling was invisible
  before.
  `begin()` returns a `UiState` too, which is what makes a **root** resizable:
  the root is the one element whose size is a plain argument rather than
  something the solver derives, so dragging its edge is just editing the `Vec2`
  passed in next frame — and shrinking its width re-wraps every text block
  inside it. Both roots in `ui_layout_test` do this from a 14 unit right/bottom
  band, which stays inside the root's 16 unit padding so no child ever steals
  the hit. Verified in `ui_layout_test` case 0: hover/press styling, inline
  click counting, panel corner drag, and both roots resizing.

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
  the single Y flip lives in `UiRenderer::uiToScreen`. Verified numerically in
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
