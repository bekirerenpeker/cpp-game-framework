# Engine TODOs

Working backlog for the engine, ordered by rough priority. Claude Code agents may
add, edit, reorder, check off, or remove items here as tasks are defined or
finished — keep it current, don't let it drift from reality. When an item is
done, move it to "Done" with a one-line note of what landed (not a changelog,
just enough to know it happened); when a plan changes, edit the item in place
rather than leaving a stale description.

## In progress / next up

- [ ] **Widget layer over the primitives** — the target shape for the UI: the
  primitives (`UIManager`'s container/leaf tree, `UILayoutCalculator`,
  `UIRenderer`, `UIContainerStyle`) are treated as **finished and closed**, and
  everything else — button, slider, image, colour picker, window, checkbox,
  dropdown — is a *composition* of them plus default styles. Each widget builds a
  small subtree, applies its own normal/hover/pressed styling, and hands back a
  state the caller reacts to. This is how Clay, Dear ImGui and egui are all built,
  and it is sustainable, with three rules that keep it that way:
  **(a) widgets live outside `UIManager`** — free functions, not methods, or
  `UIManager` becomes a god class coupled to every widget that will ever exist.
  The caller should still only ever see **one** name: a `UI` namespace holding
  both the widgets *and* thin forwarders for the bare primitives
  (`UI::openContainer`/`closeContainer`/`addTextLeaf` → `UIManager`), so dropping
  from a button down to a hand-built container is not a context switch to another
  class. `UIManager` stays the tree/state owner and simply stops being the public
  face. **(b) a widget may only
  touch `UIManager`'s public API.** That is the load-bearing discipline: if a
  widget cannot be written without reaching into privates, the *primitive* layer
  is missing something, and the fix is to add the primitive, never to friend the
  widget. **(c) one config struct per widget** (`UIButtonConfig`, …), the same
  reasoning as `UITextConfig` — a new knob must not cost a signature change, or
  every widget grows a twelve-argument overload set.
  Hit ordering and mouse capture — the two that genuinely had to land before any
  drag widget — are done, and `ui_test`'s slider is the proof that a widget is now
  just composition plus caller-owned state. The remaining items below are
  conveniences, not blockers.

- [ ] **Retained per-widget state, keyed by `persistentKey`** — *lower priority
  than first assessed.* Under the "caller owns the value" model most of this does
  not need to exist: a window's position and size can be `Vec2&` parameters
  exactly like a slider's `float&`, and so can a drag origin. What genuinely
  cannot be caller-owned is the global bookkeeping — which node holds capture
  (done) and which holds keyboard focus — because no single call site knows about
  the others. A `persistentKey -> state` map with **frame-age eviction** (immediate
  mode has no destroy event) stays worth it as a *convenience* once many widgets
  want scratch space, and is the natural home for an ImGui-style ini dump of
  window layout later.
  The old resizable-window wrinkle — that a window has to override the caller's
  `UILayoutConfig.width/height`, breaking "layout config is purely the caller's
  data" — turns out to **dissolve** under this model rather than needing solving:
  `ui_test`'s window feeds its own `Vec2` straight in as `UISizeSpec::fixed(...)`,
  so nothing is overridden and the config stays the caller's. It only comes back
  if the engine ever owns window geometry.

- [ ] **`UINodeState` outputs for dragging** — what sliders and windows need on
  top of today's `isHovered`/`isPressed`/`isReleased`/`isHeld` +
  `relativeMousePos`/`localMousePos`/`isActive`/`isHoveredDirectly`:
  `dragDelta` (movement since last frame),
  `pressOrigin` (mouse position *and* the node rect at press, so a drag computes
  from the grab point instead of accumulating per-frame error), and `grabOffset`
  (where inside the handle it was grabbed, or the handle snaps its centre to the
  cursor on the first frame). Worth adding alongside: `scrollDelta` (wheel over
  this node — the scroll item below needs it), `isDoubleClicked` (title-bar
  maximise, reset-to-default), and `isFocused` once text input exists.
  None of these are blocking: the slider in `ui_test` works on `isActive` plus
  `relativeMousePos` alone, since that is *already* the normalized value along the
  axis. They are what stops each widget re-deriving the same drag math.
  Known wart to document rather than fix: hit testing reads last frame's rect, so
  a widget that moves *because* of the drag trails by a frame. Dear ImGui has the
  same property; it only bites if drag math is written against the current rect
  instead of `pressOrigin`.

- [ ] **Event consumption** — press bubbles along with hover, so clicking a child
  also fires every container above it. Harmless while ancestors are inert panels;
  wrong the first time a button sits inside a clickable row, which is a shape the
  widget layer will produce almost immediately. Needs a way for the hit node to
  stop propagation — either a flag a widget sets, or splitting "the node that
  owns the click" from "the nodes that merely contain the cursor". Related and
  cheap: an `ignoreInput` flag on `UILayoutConfig` (CSS `pointer-events: none`)
  so decorative children opt out of hit testing entirely — the slider handle
  currently captures instead of its own track, which works but only because the
  widget checks both.

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
  **Prerequisite for the widget layer**, not a follow-up to it: every widget
  function bakes in default colours, and if those are literals rather than theme
  roles, restyling means editing engine source once and then every widget again.

- [ ] **A screen-space root that fills the window** — `resolveRootSize` still
  sizes a root from its own `UISizeSpec` with nothing passed in, so `Grow` and
  `Percent` fall back to max-content the way `Fit` does. In screen space the
  natural want is a root that *is* the window, which needs an available size
  threaded into the root solve. That collides with the documented invariant that
  a root is deliberately not clamped to its content floor, so it is a design call
  rather than a patch — decide before building a real HUD on fixed-size roots.

- [ ] **UI scale / DPI** — screen space is hard-wired to one unit per window
  pixel. A `pixelsPerUiUnit` on `UIRenderer` would fold into the ortho and into
  `getMouseUiPos` with no other call site touched, and is what a settings-menu
  "UI scale" slider or a HiDPI display would need.

- [ ] **Scroll** — clipping landed (see Done); what is left is the *scroll* half.
  `UILayoutConfig::scrollOffset` is honoured by the solver but nothing drives it from
  input, so there is no wheel handling, no drag-to-scroll and no scrollbar. Needs
  `scrollDelta` on `UINodeState` (see the drag-outputs item) plus somewhere to keep
  the offset per container — which is the first thing that genuinely wants the
  retained-state store, since a scroll position is engine bookkeeping rather than a
  value the caller naturally owns.

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

- [x] **One UI font instead of one per leaf** — `UIManager::setFont(const Font*)` holds
  the face every text leaf uses; `UITextConfig::font` stays as a per-leaf override for
  mixing faces, and is null in almost every call. With nothing set, `getFont()` bakes a
  system font once (Arial / Liberation / DejaVu, by OS) through `ResourceManager` and
  logs it, so a scene that forgets the call still draws text. Mtsdf for the fallback:
  bitmap is sharper only at the size it was baked for, which a default cannot know.
  `ui_test` toggles atlas type with TAB and shows both at 11px side by side —
  worth revisiting the default if a real HUD ends up drawing one fixed text size.

- [x] **Per-state styles** — `openContainer` takes a `UIContainerStyleSpec`: every
  `UIContainerStyle` field plus `onHover`/`onHeld`/`onPressed`/`onReleased`, each a
  whole style. `UIManager` merges base → hover → held → pressed/released (lowest
  priority first, so a press keeps the hover's look) and stores the **one** resolved
  style on the node, which is unchanged. Landed in the primitive layer rather than the
  planned widget namespace: `if (state.isHovered) node->style...` still works and is
  still the right tool for a per-frame value, but a constant reactive look wanted to be
  declared where the container is.
  Two things worth remembering: `onHeld` is `isActive` (capture), not `isHeld`, so a
  grip dragged off itself stays lit; and the spec repeats `UIContainerStyle`'s fields
  through the `UI_CONTAINER_STYLE_FIELDS` X-macro instead of inheriting, because a
  designated initializer cannot name a base's member and `{.backgroundColor = ...}` is
  the whole point. `UILayoutConfig` deliberately has no equivalent — its fields are not
  optional (an override could only replace, not merge) and hover-driven geometry reads
  last frame's hit test, so it can oscillate.

- [x] **Overflow clipping** — `UIContainerStyle::overflow` is honoured: `Hidden` or
  `Scroll` bounds everything inside the node to its rect. The clip is resolved
  **once, in the solver**, not walked per draw: `computeClipRects` runs strictly
  top-down (free, since the tree is built preorder, so a parent's rect is final
  before its children are reached) and intersects each node's inherited box with its
  own. Every node then carries the finished rect into the vertex data, and both
  fragment shaders `discard` outside it.
  The non-obvious part is that each node stores **two** rects: `clipRect` bounds the
  node's *own* drawing, `childClipRect` is what it hands down. They differ by the
  node's own overflow, and keeping them apart is what stops a clipping container from
  cutting off its own shadow — a shadow lives outside the rect by definition, so
  intersecting a node with itself would erase it.
  Text needed the same treatment, since a clipped container full of glyphs is the
  whole point: `TextVertex` gained a clip rect and `TextRenderer::setClipRect` /
  `clearClipRect` bound whatever is emitted next. It deliberately does **not** flush
  on change the way the view-proj override does — the rect is per-vertex, so text
  under different clips still shares one draw call. World-space text carries a rect
  far enough out that the test never rejects, so it costs one compare and no branch.
  Glyphs are cut **mid-letter** rather than dropped whole, which is what makes a
  half-scrolled line look right.
  Hit testing was clipped in the same change rather than left for later: a node
  clipped away visually but still clickable is a worse bug than the overdraw it
  replaced, and it is the exact failure the old combined scroll+clip item warned
  about.
  Known gap, deliberately not unified: `UILayoutConfig::clipX/clipY` (sizing — lets a
  box shrink below its content) and `style.overflow` (painting) are separate flags a
  caller must set together, and `overflow` clips both axes rather than honouring the
  per-axis distinction.

- [x] **Mouse capture, and paint layers for cross-batch ordering** — two things
  the slider in `ui_test` needed.
  **Capture**: `UIManager` keeps one `m_activeKey`. A press claims the topmost
  node and holds it until release *wherever the cursor goes*, and while it is held
  nothing else can be hovered — which is the point: dragging a slider past a
  button must not light the button, and the slider's own caller has no way to tell
  that button to stand down. `UINodeState::isActive` is what a drag reads instead
  of `isHeld`; `isHeld` rides on hover and so dies the moment the cursor leaves the
  track, which is most of a real gesture. Two details that matter: capture
  survives one frame past button-up so the node that was *pressed* is the one that
  sees `isReleased` (dropping it on button-up hands the release to whatever the
  cursor drifted onto), and the captured node still only counts as *hovered* while
  the cursor is genuinely inside it, so press-drag-away-release correctly does not
  fire. A captured node that stops being declared drops the capture rather than
  deadlocking input on a key nothing will match.
  **Paint layers**: paint order is the preorder walk — children above ancestors,
  later siblings above earlier — and the tree already encodes it correctly. But
  boxes and glyphs are separate batches flushed once each, so *every* glyph landed
  above *every* box regardless of the tree, which is why a floating overlay's
  background sat under the labels it was covering. `UILayoutNode::paintLayer`
  implements the previously-declared-and-ignored `zIndex` as a **layer offset from
  the parent** (not CSS's sibling-relative, global-stacking meaning), so a node and
  its subtree rise together and a child can never fall behind its own container.
  `draw()` then emits a layer at a time and flushes both batches per layer: order
  is preserved at two draw calls per layer rather than per node. Deliberately
  *not* tied to `isFloating` — that is positioning, not stacking, and conflating
  them is a known CSS confusion rather than a model worth copying.

- [x] **Hit resolution: topmost wins, hover bubbles to ancestors** —
  `computeState` used to test every container against the mouse *independently*,
  so every container under the cursor reported its own hover and a click landed
  on all of them at once. `UIManager::resolveHover()` now runs once per frame
  from `clear()` — the last moment before the tree is rebuilt, which pairs this
  frame's mouse with last frame's geometry and settles the answer before any
  `openContainer` asks for it — and finds the **last** hit node in paint order.
  Last is topmost because children paint after parents and later roots after
  earlier ones, so hit order mirrors paint order by construction. Honouring
  `zIndex` means reordering *both* together, not just this pass.
  `isHovered` then **bubbles**: the hit node and every ancestor, DOM-style, so a
  panel stays lit while the cursor is on its own children. Stopping at the hit
  node alone reads as the panel flickering off whenever the cursor crosses its
  contents. `isHoveredDirectly` keeps the single topmost node available for a
  widget that needs to know the cursor is on *it*. The press/release/held flags
  ride on the bubbled hover, and that does **not** reintroduce the original bug:
  two overlapping containers still cannot both react, because only the topmost
  node's ancestors are in the chain at all.
  Two things it needed underneath: `UILayoutNode::acceptsInput`, copied off the
  `UINode` at build time because the snapshot deliberately drops the pointer —
  and it excludes leaves, since a leaf hands back no state and hitting one would
  silently swallow the hover its container should have had. And the snapshot's
  index links are now **rebased** onto the concatenated vector; they are relative
  to each root's own solve, so an un-rebased parent walk lands in whichever root
  happens to sit at that offset.
  Not done, and the natural next step if it is ever wanted: **consumption**, a
  child stopping propagation so an ancestor does not also see the click.

- [x] **UI space switch: screen by default, world on request** — `UIRenderer`
  owns a `UISpace` (`Screen`/`World`) set by `setSpace`, and three things read
  it: the view-proj matrix, `getRootOrigin()` (where a root's top-left lands)
  and `getMouseUiPos()`. One owner, three readers, so they cannot disagree —
  which is what makes the switch safe for rendering, hit testing and layout in
  one move. Layout itself never learned about spaces: `calculate` gained a
  `rootTopLeft` and `computeDrawPositions` bakes it into `drawPos`, so the *one*
  value the renderer draws from and the mouse tests against is already in the
  active space. Screen space is Y-**up** (`ortho(0,w,0,h)`) because glyph quads
  are built baseline-up and a Y-down matrix mirrors every string; the flip is one
  translation in `getRootOrigin`. `UIManager::draw()` became self-contained —
  pushes the matrix into `TextRenderer`, walks, then flushes boxes before glyphs
  — since that ordering is not something a caller should have to know.
  The bug worth remembering: `setSpace` originally flushed unconditionally, and
  because it is naturally called during setup that ran `ensureReady` and built
  the window's VAO **before the render context was bound**. Every later draw then
  had a correct matrix and correct vertices and drew nothing — a forced opaque
  fragment output proved the fragments never ran. It now flushes only when the
  batch is non-empty. Verified in both spaces, including hover.

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
