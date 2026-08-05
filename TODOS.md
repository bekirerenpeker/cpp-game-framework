# Engine TODOs

Working backlog. Claude Code agents may add, edit, reorder, check off or remove
items — keep it current. Done items get **one or two lines**, just enough to know
they happened; the reasoning lives in CLAUDE.md, not here.

## UI & text — next up

Roughly in order: each item is mostly unblocked by the ones above it. Not a
schedule, just the sequence that avoids rework. Deliberately unnumbered — items
land out of order often enough that renumbering the rest was pure churn, so
cross-references go by name.

- [ ] **`UINodeState` drag outputs** — `dragDelta`, `pressOrigin` (mouse *and*
  node rect at press, so a drag computes from the grab point instead of
  accumulating error), `grabOffset`, `scrollDelta`, `isDoubleClicked`. Each is a
  few lines in `resolveInput`; together they stop every widget re-deriving the
  same drag math — the window and the sliders each hand-roll it today.

- [ ] **Event consumption** — press bubbles with hover, so clicking a child
  also fires every container above it. Fine while ancestors are inert panels,
  wrong the first time a button sits in a clickable row. Needs the hit node to be
  able to stop propagation — split "the node that owns the click" from "the nodes
  containing the cursor". `ignoreInput` (the cheap half) already landed.

- [ ] **Keyboard focus + text input** — the biggest one, and the gate on
  `textField`, numeric entry and editable colour values. Needs a focused key in
  the state store, event consumption, `Input` character events, then a caret,
  selection, and clipboard. Do it as one widget first, generalise after.

- [ ] **`contextMenu` and a menu bar** — the overlay machinery is done and
  `dropdown` proves it; both are compositions on top, and the state store now
  gives them somewhere to keep their open flag.

- [ ] **A screen-space root that fills the window** — `resolveRootSize` sizes a
  root from its own spec with nothing passed in, so `Grow`/`Percent` fall back to
  max-content. A HUD wants a root that *is* the window, which needs an available
  size threaded into the root solve — and collides with the invariant that a root
  is deliberately not clamped to its content floor. A design call, not a patch.

- [ ] **`cursor` and `transition`** — both declared on `UIContainerStyle` and
  both dead. `cursor` is a glfw cursor set from the hovered node, once per frame.
  `transition` is a per-key animated value in the state store plus a rule for what
  a style field interpolates as.

- [ ] **Grid** — a track list where each track carries a `UISizeSpec`, run
  through the solver's existing distribution routine, then row-major placement
  with an optional span. ~80% of grid's value; skip auto-fit/minmax/dense.

- [ ] **More widgets** — cheap compositions now theming has landed: `image`,
  `progressBar`, `tabs` (`toolbarMenu` is most of it), `treeView`, `groupBox`,
  `dragFloat` (slider without a track). Each is a config struct plus a subtree.

- [ ] **Rename `UIWidgets` to `UI`** — the namespace already forwards the bare
  primitives alongside the widgets, which was the point; the name still says
  "widgets". A rename, nothing more, but do it before call sites multiply.

- [ ] **One batch for UI rects and UI glyphs** — a panel and its label cost
  two draw calls, since the rect shader is a rounded-box SDF and the glyph shader
  is a field sampler. Merging means one shader branching on a per-vertex mode and
  one vertex format wide enough for both. Only worth it when a real UI shows the
  draw calls matter.

- [ ] **Text perf** — three separate small ones: pack `TextVertex` (7
  attributes, 68 bytes, two full `Color`s → RGBA8); a cross-block layout cache
  keyed on `hash(text, style, maxWidth)` so N identical labels cost one walk; and
  named style tags (`/b`, `/i`, `/color=red`) through `TextTags::isTagAt`, the
  single extension point.

- [ ] **Known warts, documented not fixed** — hit testing reads last frame's rect,
  so a widget that moves *because* of a drag trails a frame (Dear ImGui does the
  same). Dashed borders use the mean of the four corner radii for the arc-length
  walk, exact only when the corners match. There is no `minHeight`/`maxHeight`,
  which the single-number height pass depends on. Both scrollbars span their whole
  edge, so with two up they cross in the corner. A floating child of a scroll
  container does not scroll with it — right for a popup, arguable otherwise.

## Other systems

- [ ] **Terrain-type tilemap rework** — `Tileset::tilesConnect` is `a != 0 && a == b`,
  so a rule tile only autotiles against itself. Add a `terrainType` on `TileData`
  decoupled from the visual tile id, compare that, and support transition tiles
  keyed by a `(terrainA, terrainB)` pair. A data-model change — do it before
  building real tilemap content.

- [ ] **Tilemap vertex/perf pass** — pack `TileVertex`'s `Color` to RGBA8, shrink
  `texIndex`, and rebuild chunks partially instead of a full `CHUNK_SIZE^2`
  rebake. Do it alongside the terrain rework; both touch `TileVertex`.

- [ ] **Per-sprite material uniforms** — `SpriteComponent::shader` picks a shader
  but cannot feed it uniforms. Needs a material concept: a shared uniform set
  stored once and referenced by sprites, applied on the shader switch.

- [ ] **Docs pass** — CLAUDE.md is the only source of truth and is getting long.
  Once the UI stabilises, split into `docs/` per subsystem.

## Done

### UI

- [x] **`TextOverflow` clip + ellipsis** — `Ellipsis` is a post-pass in
  `TextLayoutCalculator`, run before alignment (cutting a line changes the slack it
  aligns against) and after the walk, so the wrap has already settled. It rebuilds
  the run vector forward rather than editing in place, since dropping runs from the
  middle would shift every later line's `firstRun`. The marker takes the style of
  the run the cut lands in, and is `"..."` not `U+2026`, which is outside Latin-1
  and so outside what gets baked. `Clip` has no layout half: both values bound the
  glyphs to the box in `TextRenderer::draw`, **intersected** with the caller's clip
  rather than replacing it, and restored after so it does not leak into the next
  block on the batch. `addTextLeaf` sets `clipX` from a non-Visible overflow, the
  leaf-side twin of overflow implying the zero floor — without it the leaf keeps
  its full width and there is nothing to cut. Alignment now uses the width the
  block was *given*, not just its own longest line, or right-aligned non-wrapping
  text had no slack to move in.

- [x] **Richer input return values** — every bound-value widget returns a
  `UIInputState`: its `UINodeState` plus `isChanged` / `isEditing` / `isReleased`,
  so an expensive update hangs off release instead of running every frame of a
  drag. The value still travels through the caller's reference; the struct only
  says *when* to act. A widget with no drag to it (checkbox, radio, dropdown,
  toolbar) completes on the click, so it fires `isChanged` and `isReleased`
  together and never `isEditing` — otherwise a caller waiting on release would
  wait forever. `dragInputState` holds the one bit that cannot be derived from the
  frame (was a gesture live last frame) in the state store. Sliders measure change
  *after* snapping, so clamping a caller's out-of-range value is not reported as an
  edit, and the picker measures on hsv rather than the colour, which it rebuilds
  every frame and so drifts a bit or two when untouched.

- [x] **Scroll** — `overflow = UIOverflow::Scroll` is the entire interface: it clips,
  gives both axes a zero content floor, drives the offset and draws the bars. The
  placeholder scaffolding it replaced was wrong in three ways and all three
  changed: `scrollOffset` left `UILayoutConfig` (retained state a caller cannot
  set, read from the store by a `resolveScroll` pass between the sizes and the
  positions, so it clamps against *this* frame's content); `overflow` now implies
  the zero floor instead of pairing with `clipX`/`clipY`, which stay only for a
  leaf that has no style to put an overflow on; and the vertical pass now shrinks,
  without which a `Grow` scroll container grew past its parent instead of becoming
  a viewport. `contentSize` is measured from the children's **solved** sizes, not
  from intrinsics: a scrollable child asked for its content, which it then clips,
  so a scroller inside a scroller gave its parent a range that revealed nothing.
  Bars are **not nodes** — `scrollbarOf` derives them from a solved
  `UILayoutNode`, so no key, no tree slot, no frame of lag; they draw after the
  layer's glyph flush and carry their own capture in `UIManager`. Wheel routes
  outward along the hover chain and leaves an axis it cannot move, so a list hands
  its parent the rest at the end. `Input::getScrollDelta` is one `Vec2`: the
  trackpad's own x, or shift+wheel remapped onto it.

- [x] **Retained state store** — `UIStateStore`, keyed by `UINode::persistentKey`
  (now also on `UINodeState`, so a widget can address its own entry). A slim
  `UINodeSystemState` for what the UI's systems own — `scroll`, `contentSize`,
  unwired until scroll lands — plus a flat `string -> float` bag for widgets,
  hashed to one slot per `(node, field)`. Frame-age eviction, swept in
  `beginFrame` so no `float&` can be live when an erase happens; the window is
  deliberately long, since a hidden tab's node is untouched but its scroll
  position still matters. The five ad-hoc maps folded in, which also killed their
  keying bugs: two same-named windows shared geometry, and a dropdown keyed on
  `&selected` broke on a reallocating vector or a stack local.

- [x] **Theming** — `UIThemeManager` (its own Singleton, outside `styling/`) holds
  21 colour roles, a metric ramp and string-keyed text styles. Colours are a spec
  of optionals: set `background`/`accent`/`foreground` and `resolve()` derives the other
  18 by lighten/darken/mix, so a retheme is three lines; `onAccent` picks white or
  near-black off the accent's Rec. 709 luminance. Widgets read roles inline
  (`UITheming::colors().accent`) — the theme holds no per-widget presets, and
  nothing in `UIManager`, the solver or the renderer reads it except the scale.
  Extra roles go in a `custom` string map; extra text styles just take a new name.

- [x] **UI scale (and DPI with it)** — `UIThemeMetrics::scale` multiplies every
  pixel-valued layout and style field once, in `UIManager::addNode`. Scaling the
  *inputs* rather than the projection is what keeps the solve's output in real
  window pixels, so hit testing, clip rects and `getMouseUiPos` need no changes.
  Percent/Grow are ratios and text `size` scales while its em-based effects do
  not. The one trap: `UINodeState` hands back solved geometry in real pixels, so
  anything feeding it back into a config goes through `UIWidgets::unscale` — the
  window drag, the tooltip anchor and both popup offsets do.

- [x] **Widget layer over the primitives** — `UIWidgets` is the single public face:
  thin forwarders for the primitives plus every widget as a free function with one
  config struct each. `UIManager` stays the tree owner and no widget touches its
  privates. Shipped: `button`, `text`, `dividers`, `toolbarMenu`, `sliderFloat`/
  `sliderInt`, `checkBox`, `radioGroup`, `tooltip`, `colorPicker`,
  `colorPickerPopup`, `dropdown`, `dragHandle`, `openSection`/`closeSection`,
  `openWindow`/`closeWindow`. The demo window's Theme tab edits a live theme and
  switches between seven built-in presets.

- [x] **Window widget** — movable, resizable from an image drag handle, and
  collapsible. Geometry lives in the state store, not on the caller. Collapse
  works by `UIManager::removeChildren` at `closeWindow`, since immediate mode
  gives a widget no way to stop its caller declaring content.

- [x] **Overlay machinery + the widgets on it** — `ignoreClip` escapes every
  clipping ancestor for a whole subtree, `ignoreInput` makes a subtree transparent
  to hit testing, `zIndex` lifts a paint layer, and hit resolution orders by the
  same three keys the painter uses (`rootOrder`, `paintLayer`, preorder).
  `tooltip`, `colorPickerPopup` and `dropdown` are built on it.

- [x] **Optional style/layout structs, one field list each** — `UIContainerStyle`,
  `UITextStyle` and `UILayoutConfig` are all `std::optional` fields generated from
  an X-macro, with `combine` (input side wins) and `fillDefaults` (plain unthemed
  fallbacks) for free. Filling happens once per node in `UIManager`, so everything
  downstream dereferences and no default is decided twice.

- [x] **Per-state styles** — `openContainer` takes a `UIContainerStyleSpec`: every
  style field plus `onHover`/`onHeld`/`onPressed`/`onReleased`, merged lowest
  priority first into the one style stored on the node. `onHeld` reads `isActive`
  (capture), not `isHeld`, so a grip dragged off itself stays lit.

- [x] **Mouse capture + paint layers** — one `m_activeKey` claims the topmost node
  on press and holds it until release wherever the cursor goes, suppressing hover
  elsewhere; it survives one frame past button-up so the pressed node sees the
  release. `paintLayer` implements `zIndex` as an offset from the parent, and
  `draw()` flushes boxes and glyphs **per layer**, or every glyph would paint over
  every box.

- [x] **Hit resolution: topmost wins, hover bubbles** — resolved once per frame in
  `clear()`, pairing this frame's mouse with last frame's geometry. The last hit
  in paint order is the topmost; `isHovered` then bubbles to every ancestor, with
  `isHoveredDirectly` for the single node. Leaves are excluded, or a label would
  swallow its button's hit.

- [x] **Overflow clipping** — resolved once in the solver, top-down, into two rects
  per node: `clipRect` bounds the node's own drawing, `childClipRect` what it hands
  down. They differ by the node's own overflow, which is what stops a clipping
  container erasing its own shadow. Both shaders `discard` outside; glyphs cut
  mid-letter. Hit testing reads the same rect.

- [x] **Containers paint through `UIRenderer`** — one quad carries a whole
  container: background, image, border ring, per-corner radius, dash pattern and
  shadow all out of a single rounded-box SDF. Every field is per-vertex, never a
  uniform. A container with nothing visible emits zero geometry — so an unstyled
  container is invisible, not a white debug box.

- [x] **UI space switch** — `UIRenderer::setSpace` picks `Screen` (one unit per
  window pixel, camera-independent) or `World`, and the matrix, root origin and
  mouse position all read the one `m_space` so they cannot disagree. Screen space
  is Y-up, because glyph quads are built baseline-up and a Y-down matrix mirrors
  every string.

- [x] **Per-corner border radius + image rotation** — `UICorners` on
  `borderRadius`, with the SDF picking the radius per fragment;
  `imageRotation` rotates the uv lookup, reusing a spare vertex slot rather than
  growing `UIBoxVertex`.

- [x] **Engine-owned assets** — `FileManager::engineAsset()` resolves from
  `engine/assets` regardless of the game's cwd, so the engine stays
  redistributable. The quad/text/tilemap/uibox/colour-picker shaders auto-load
  through `ResourceManager` in their own subsystem's `init`, with no user-facing
  load call and a pointer override for anyone who wants their own.

- [x] **Window projection on `ViewContext`** — view and proj are stored separately
  with `getViewMat`/`getProjMat`/`getWindowProjMat`; `UIRenderer` no longer builds
  its own.

- [x] **UI font** — `UIManager::setFont` holds the face every text leaf uses;
  `UITextConfig::font` is a per-leaf override for mixing faces. A null font is
  normal: it resolves to `FontLoader`'s default further down.

- [x] **Pre-rewrite UI (superseded)** — the immediate-mode `UiSystem`/`UiRenderer`/
  `LayoutCalculator` stack landed in full (five-pass solver, rounded-rect shader,
  hover/press styling, interaction keys) and was deleted in `a886a32` for the
  current `UIManager`/`UILayoutCalculator`/`UIRenderer`. Recover with
  `git show a886a32^:<path>` if any of it is ever wanted back.

### Text

- [x] **Text layout folded out of `TextRenderer`** — wrapping, line boxes and
  alignment live in one stateless `TextLayoutCalculator` over a `TextBlock` that
  carries input and solved output together. Line height is the max over every
  style on the line and baselines are placed at `lineTop + ascent`, never stepped;
  `TextMetrics::step` is shared by the measuring and emitting walks so a measured
  width cannot drift from a drawn one. `TextBlock` is neither copyable nor
  movable — its runs hold views into its own string.

- [x] **Async font loading** — `Font`'s constructor queues the bake on `FontLoader`
  and returns; a loading font answers from a placeholder, and the const accessors
  poll so the atlas uploads on the GL thread with no owner and no `update()`. The
  queue is shortest-job-first and the baker takes a **quarter** of the cores at
  below-normal priority — at full width it starved the render thread.
  Gap: nothing evicts a font whose bake failed.

- [x] **Default font on `FontLoader`** — bakes a system face as a small bitmap
  atlas, queued ahead of whatever triggered it, and every text entry point
  resolves through it, so a font that is still baking borrows a real face instead
  of drawing placeholder boxes. `TextBlock` records *which* font it solved with,
  since that resolution flips mid-life.

- [x] **Text renderer** — `TextRenderer` over `BatchRenderer<TextVertex>`, layered
  as `draw` / `drawSpan` (the chaining primitive) / `appendGlyph` / measure.
  Positional styling via `/s` tags plus a style list; `//s` escapes. Everything a
  style varies is per-vertex, so many styles still share one flush. `Utf8::next`
  because Latin-1 codepoints are multi-byte.

- [x] **Text effects** — outline, boldness, softness, shadow/glow, italic skew,
  underline, strikethrough. **All widths are em, not pixels**, or they drift under
  zoom. Effect width is capped by the baked distance range
  (`Font::getMaxEffectEm()`).

- [x] **Font resource + msdf-atlas-gen baker** — `Font` is an `IResource` producing
  an atlas + em-unit metrics, as `Mtsdf` or `Bitmap` (the type picks the filtering
  internally). The disk cache in `.cache/fonts/` is the real interface, so neither
  freetype nor msdfgen is needed to load one. Writes go to a temp file then
  rename, so a crash cannot leave a truncated file that reads as a valid hit.

### Engine

- [x] **`Application` loop wrapper** — owns the window loop, `Time::update`,
  per-window input and camera ordering, `swapBuffers` and `pollEvents`; scenes
  supply `onFrame`/`onWindowUpdate`/`onWindowRender`.

- [x] **`Delegate` binds plain callables** — a `bind(C*)` overload for lambdas and
  functors, `requires`-constrained so a signature mismatch errors at the call.
  Stores only a pointer, so the callable must outlive the delegate.

- [x] **Per-sprite shaders** — `SpriteComponent::shader`; `renderSprites` sorts by
  `(layer, shader, texture)` and flushes on a shader boundary.

- [x] **Separate alpha blend func** — `glBlendFuncSeparate`, because everything
  renders into a transparent FBO and the old plain `glBlendFunc` multiplied source
  alpha in twice, eating antialiased edges.

- [x] **HSV on `Color`** — `toHsv()` / `fromHsv()`, moved off the colour picker so
  anything can use them.
