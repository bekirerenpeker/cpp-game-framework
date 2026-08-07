# Engine TODOs

Working backlog. Claude Code agents may add, edit, reorder, check off or remove
items — keep it current. Done items get **one or two lines**, just enough to know
they happened; the reasoning lives in CLAUDE.md, not here.

## bugs

- [ ] **Home / End / PageUp / PageDown reportedly do nothing** — not reproduced. The key
  table is aligned (`KeyCode::Home` is enum 62 and `glfwKeyCodes[62]` is 268), `keyRepeated`
  is the same path the arrows use and those work, and the handlers in `applyEdits` read
  correctly. Two open possibilities: the symptom was really the two rendering bugs fixed
  alongside it (a one-line field ate its own text when scrolled, so End moved the caret
  invisibly), or the keys being pressed are the **numeric keypad's** Home/End/PgUp/PgDn,
  which glfw reports as `KP_7`/`KP_1`/`KP_9`/`KP_3` and nothing maps. Re-test and say which
  keys.

## Implement Now

- [ ] **`contextMenu` and menu bar** — compositions on top of overlay machinery.

## UI & text — next up

- [ ] **Text field follow-ups** — deferred from the selection work, in rough order of value:
  **undo/redo** (Ctrl+Z / Ctrl+Y, an edit-history ring per field — not yet certain it is
  wanted); **triple-click to select a line**, which needs a click *count* on `UINodeState`
  rather than the current double-click bool; and **IME / composition input**, which the
  char-callback path cannot represent at all and which any CJK user would need.

- [ ] **`transition`** — animate style fields per key in the state store; needs a rule for what each field interpolates as. `cursor`, its other half, has landed.

- [ ] **More widgets** — `image`, `progressBar`, `tabs`, `treeView`, `groupBox`, `dragFloat`.

- [ ] **One batch for UI rects and glyphs** — merge shaders to cut draw calls.

- [ ] **Text perf** — pack `TextVertex` to RGBA8, layout cache, named style tags.

- [ ] **Known warts** — hit test lags a frame; dashed borders use mean radius; no `minHeight`/`maxHeight`; scrollbars cross in corner; floating children don't scroll; clip rects are AABBs, so square children still poke into a rounded corner's arc.

## Other systems

- [ ] **Terrain-type tilemap rework** — decouple terrain type from visual tile id; support `(terrainA, terrainB)` transition tiles. Data-model change, do before real tilemap content.

- [ ] **Tilemap vertex/perf pass** — pack `TileVertex::Color` to RGBA8, shrink `texIndex`, rebuild chunks partially. Do alongside terrain rework.

- [ ] **Per-sprite material uniforms** — shared uniform set referenced by sprites, applied on shader switch.

- [ ] **Docs pass** — split CLAUDE.md into `docs/` per subsystem once UI stabilises.

- [ ] **Rework namespace names** — `UIWidgets` → `UI`, standardise across the project.

## Done

### UI

- [x] **Multi-line text input, selection and clipboard** — `textArea` beside `textField`,
  sharing one editing core. Caret geometry comes from a real `TextBlock` laid out by
  `TextLayoutCalculator`, so a caret x cannot drift from the glyphs; byte-index ↔ line
  mapping is read off the runs, with blank lines handled by the fact that only an explicit
  newline can produce one. Selection is an anchor beside the caret — shift+arrows, Ctrl+A,
  mouse drag, double-click word — drawn as one floating quad per line behind the text.
  Clipboard is Ctrl+C/X/V through two new `Window` methods. Also Ctrl+arrows by word,
  per-line Home/End with Ctrl+Home/End for the whole text, PageUp/PageDown, and Up/Down
  with a remembered column. The area scrolls through the ordinary `overflow = Scroll`
  machinery, so scrollbars and the wheel came for free.

- [x] **Keyboard focus + text input** — `Input::getTypedText()` returns the frame's UTF-8
  (glfw char callback accumulated on the `Window`, drained in `update` beside the wheel);
  a string rather than one char because a frame can carry several and a codepoint several
  bytes. `Input::keyRepeated()` fires on press and on every OS auto-repeat, from a real
  glfw key callback, so the delay and rate are the user's own — general across every key,
  not an editing-key special case. Focus is `UIManager::m_focusedKey`, claimed on press by
  walking out to the first `style.focusable` node, plus Tab/Shift+Tab cycling in
  declaration order; `UINodeState::isFocused` and an `onFocused` style state fall out of
  it. `textField` and `numberFieldFloat`/`Int` in `UIWidgetsText.cpp` — caret, click to
  position, arrows/Home/End/Backspace/Delete, horizontal scroll, blink, placeholder,
  filtering, steppers, wheel. Also added `Utf8::prev`/`encode` and promoted `runWidth`
  into `TextMetrics::measure` so caret x cannot drift from where glyphs land.

- [x] **Grid** — `openGrid`/`closeGrid`, cells are just the children in order. Twelve
  columns by default and `.gridSpan = n` carves them up CSS-style; `.columns` takes a
  count or a written-out `UISizeSpec` track list, so a column can hug the widest cell in
  it. Row-major with wrapping, per-track distribution reusing the level-up/level-down
  shape. Skipped as planned: auto-fit, minmax, dense packing, row span, column-major.
  Brought `UIAlign::Stretch` with it — a grid stretches its cells to their band by
  default, and flow layout's `alignCross` honours it too.

- [x] **`UINodeState` drag outputs** — `mousePos`, `dragDelta`, `pressOrigin` (mouse *and*
  node rect at press), `grabOffset`, `scrollDelta`, `isDoubleClicked`. `dragDelta` is
  measured **from the press, never accumulated frame to frame**, which is the whole point:
  summing per-frame deltas drifts, and drifts worst on the node being dragged since it
  moves under its own answer. The press data lives as plain members on `UIManager` next to
  `m_activeKey` rather than in `UIStateStore` — only one node is ever captured, so a table
  keyed per node would be waste. `dragVec2` lost `"dragActive"` and `"dragPointerOrigin"`
  and now keeps only the caller's value baseline, which the engine cannot know. Sliders and
  the colour picker deliberately stay on `relativeMousePos`: they are absolute positional
  drags with no grab offset by design, so a `grabOffset` would change how they feel.

- [x] **Event consumption** — `style.blockInput` bounds press/held/released to the hover
  chain up to and including the first node that sets it; **hover itself keeps bubbling all
  the way**, matching CSS `:hover`, so a panel stays lit while the cursor is on its
  children. Stored as `m_pressKeyCount` — the same walk that builds `m_hoveredKeys`, cut
  short — so the default costs nothing and `false` is bit-identical to the old behaviour.
  Copied onto `UILayoutNode` (the snapshot nulls `.node`) and, unlike `ignoresInput`,
  deliberately **not** sticky down the subtree: it says this node owns a click that landed
  in it, not that its descendants cannot be hit. The built-in widgets set it on themselves,
  so a button in a clickable row no longer fires the row too. Falls out for free: a panel
  with an `onPressed` style stops flashing when something inside it is clicked.

- [x] **Root sizes against the window** — `resolveRootSize` gained `Grow → available` and
  `Percent → available * value`, threaded in as a third `calculate` argument from
  `UIRenderer::getRootSize()` (the window in screen space, zero in world space, where there
  is no box for a ratio to be a ratio of). **`Fit` and `Fixed` never read it**, which is
  what makes every root that predates this bit-identical — including `openWindow`'s
  wrapper, which is `Fit`. A full-screen menu is now `grow`/`grow` plus centre aligns on a
  root; `ui_test` toggles one with F1.

- [x] **`cursor`** — the dead style field is wired: `Window::setCursor(CursorShape)` over a
  `GLFWcursor*` cache owned by `GlfwContext`, which is what owns the `glfwTerminate`
  lifetime they must die before. `UICursor::Default` means **"no opinion"**, not "arrow" —
  the shape is the first node naming one, walking outward from the pointer, so a container
  only affects the cursor when it actually sets one and ancestors inherit down. Resolved
  once per frame at the end of `resolveInput`. Per-state cursors (`onHover`, `onPressed`)
  came free from the existing style flattening. Enum gained the four resize shapes.

- [x] **Clipping honours the parent's border** — `childClipRect` is inset by the border
  width, so a child of an `overflow: Hidden` container no longer paints over the ring its
  parent drew inside the same rect. `clipRect` is deliberately **not** inset — the node
  still has to reach its own ring and shadow. The inset is the width the renderer *actually*
  paints (`UILayoutNode::borderInset`), replicating `UIRenderer`'s own rule: a transparent
  border draws nothing however wide it is set, and the shader clamps to half the short side,
  so anything else would carve pixels out of a ring that was never there. `scrollbarOf`
  insets by the same value. Still an AABB, so rounded corners leak — logged as a wart.

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
