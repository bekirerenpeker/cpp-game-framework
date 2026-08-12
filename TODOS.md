# Engine TODOs

Working backlog. Claude Code agents may add, edit, reorder, check off or remove
items — keep it current. Open items stay **one line** wherever they can; the reasoning
lives in CLAUDE.md, not here. Done items get one or two lines, just enough to know
they happened.

The engine side is far ahead of the game side: UI, text, windowing, audio, ECS and the
batched renderer are all real, and there is still no collision, no save/load and no scene
that is not a hand-written test function. **Gameplay systems** below is the honest critical
path; everything else is polish on things that already work.

## Implement Now

- [ ] **Organize namespaces** — everything is in `Engine`; split into `Engine::UI`, `Engine::Rendering`, `Engine::Audio`, `Engine::ECS` etc. Folds in the `UIWidgets` → `UI` rename.

## Bugs

- [ ] **Home / End / PageUp / PageDown may be dead** — not reproduced; key table is aligned and the arrows share the same path. Suspect the numpad, which glfw reports as `KP_7`/`KP_1`/`KP_9`/`KP_3`. Re-test and say which keys.
- [ ] **GL errors on shutdown** — three `GL_INVALID_OPERATION` on shader handles as the window closes; a `GlShader` is destroyed after its context is gone.
- [ ] **Registry contexts are never freed** — `~Registry` and `clear()` delete the pools but ignore `m_contexes`, and a context is stored as `void*` so no destructor could run anyway. `PhysicsWorld` is the first one holding heap members. Needs a type-erased holder with a virtual destructor, like `ISparseSet`.
- [ ] **Shutdown leaks** — `GlfwContext::quit()` is called from nowhere so glfw's ~90KB never frees, and a `ui_test` text widget outlives its `std::string`; the other ~67KB every scene reports is MSVC's `<chrono>` tzdb cache from `Time.cpp` and is not ours to fix.

## Gameplay systems — none of this exists yet

- [ ] **Surface materials** — friction and bounciness live on the rigidbody, so anything without one reads as zero and friction collapses against *every* static surface. Move them to `ColliderComponent` and `TileDefinition`. Next step in [PHYSICS_ROADMAP.md](PHYSICS_ROADMAP.md).
- [ ] **Physics for actual gameplay** — one-way tiles, `addForce`/`addImpulse`/radial impulse, layers and masks, render interpolation. Steps 2-6 of [PHYSICS_ROADMAP.md](PHYSICS_ROADMAP.md); none of it is expressible today.
- [ ] **Character controller** — a controllable body in `physics_test` first, then grounded/airborne states, coyote time, jump buffering. Forces the `onFixedUpdate`-before-`Input::update` decision.
- [ ] **World save/load** — ECS + tilemap through `JsonFile`/`BinaryFile`; chunked so a big world streams.
- [ ] **2D lighting** — tile flood-fill into a light texture sampled by the tile and sprite shaders.
- [ ] **Sprite animation** — frame ranges over a `TextureAtlas`, plus a small state machine component.
- [ ] **Particles** — pooled, batched through the existing quad renderer.
- [ ] **Item + inventory model** — the grid UI for it already landed.
- [ ] **Prefabs** — spawn an entity from a JSON description instead of by hand.

## Engine core

- [ ] **Scene abstraction** — `onEnter`/`onExit`/`update`/`render`, replacing the hand-written test functions and the `main.cpp` switch.
- [ ] **Named input actions** — actions over `InputAxis`, rebindable and serialized.
- [ ] **Settings file** — resolution, volume, keybinds, through `JsonFile`.
- [ ] **Hot-reload** — shaders first; it is the fastest iteration win in the engine.
- [ ] **Error convention** — one way to report a recoverable failure; right now it is a mix of `nullptr`, `bool` and a log line.

## Rendering

- [ ] **Sprite culling** — the tilemap culls against `ViewContext`, sprites do not.
- [ ] **Cache the sprite sort** — `renderSprites` re-sorts every entity every frame.
- [ ] **Render layers** — explicit sorting groups instead of the single `layer` int.
- [ ] **Post-processing chain** — a stack of passes, not the one hardcoded pass.
- [ ] **Debug draw channel** — persistent per-frame lines/boxes over `addLine`/`addFrame`; collision work will need it immediately.
- [ ] **Per-sprite material uniforms** — shared uniform set referenced by sprites, applied on shader switch.

## Tilemap

- [ ] **Terrain-type rework** — decouple terrain type from visual tile id; support `(terrainA, terrainB)` transition tiles. Data-model change, do before real tilemap content.
- [ ] **Vertex/perf pass** — pack `TileVertex::Color` to RGBA8, shrink `texIndex`, rebuild chunks partially. Do alongside the rework.

## UI — widgets & features

- [ ] **`contextMenu` and menu bar** — compositions on top of the overlay machinery.
- [ ] **`transition`** — animate style fields per key in the state store; needs a rule for what each field interpolates as. `cursor`, its other half, has landed.
- [ ] **Keyboard-operable widgets** — focus landed but only text fields use it; buttons, checkboxes, radios and sliders should take Space/Enter/arrows.
- [ ] **Focus ring** — a visible focus indicator for everything that is not a text field.
- [ ] **Drag and drop** — between widgets, for the inventory.
- [ ] **More widgets** — `image`, `progressBar`, `tabs`, `treeView`, `groupBox`, `dragFloat`.
- [ ] **Text field follow-ups** — undo/redo (Ctrl+Z/Y); triple-click to select a line, which needs a click *count* on `UINodeState`; IME/composition input, which the char-callback path cannot represent.

## UI — polish & known warts

- [ ] **Hit test lags a frame** — this frame's mouse against last frame's geometry.
- [ ] **No `minHeight`/`maxHeight`** — `UISizeSpec` carries min/max, `UIWidgets` never exposes them.
- [ ] **Floating children don't scroll** — they anchor to the unscrolled parent rect.
- [ ] **Clip rects are AABBs** — square children poke into a rounded corner's arc.
- [ ] **Scrollbars cross in the corner** — no corner gap when both are visible.
- [ ] **Dashed borders use the mean radius** — dashes drift on a per-corner radius.

## Text

- [ ] **Text perf** — pack `TextVertex` to RGBA8, cache layouts, named style tags.
- [ ] **Font eviction** — nothing removes a font whose bake failed.

## Performance

- [ ] **Physics solver cost** — 120 bodies needs ~3 substeps to settle and that is already too slow; the pair loop is ~28k exact tests a frame with no rejection. Thirteen ordered entries in [PHYSICS_ROADMAP.md](PHYSICS_ROADMAP.md) under Optimizations. Deliberately after the features.
- [ ] **One batch for UI rects and glyphs** — merge the shaders to cut draw calls.
- [ ] **Profiling scopes** — a scoped timer plus a frame-stats overlay; there is no way to see where a frame goes.
- [ ] **Pool allocators** — for per-frame churn (UI nodes, particles) instead of `new`/`delete`.

## Tooling, tests & docs

- [ ] **A test runner** — there is no framework at all; a tiny assert-based one over math, ECS and the layout solver would pay for itself.
- [ ] **UI layout inspector** — an overlay showing the node tree, solved sizes and keys. The solver is the hardest thing in the engine to debug blind.
- [ ] **Docs pass** — split CLAUDE.md into `docs/` per subsystem now that the UI has stabilised.

## Done

### Tilemap

- [x] **Transform-driven placement** — a tilemap's tile size and world offset come from its
  `TransformComponent` (`scale` and `position`; rotation ignored), shared by the renderer and
  the collision tests through `TileGrid`. Chunk meshes stay baked in tile units and the
  transform rides in the MVP, so moving or resizing never invalidates a chunk.

- [x] **Tilemap cleanup** — `Tileset` stored every definition twice and only ever read `.id`
  from the second copy; collapsed to one id-indexed vector plus a name→id map, which also
  fixed duplicate names orphaning an id and the unchecked `uint16_t` narrowing. `setAt` no
  longer allocates a 150KB chunk to store an empty tile. Coordinate math lives on
  `TilemapManager` and `TileGrid` instead of six copies across three files, and
  `TilemapComponent` is plain public data rather than two `friend` classes.

### Physics

- [x] **Fixed timestep** — accumulator and step count on `Time`, drained by
  `Application::onFixedUpdate`. Per frame, not per window. A dt clamp, a step cap and a discarded
  remainder are what keep one stall from becoming a death spiral.

- [x] **Collision and resolution** — collider and rigidbody components, all three narrowphase pairs
  behind one `Contact`, MTV resolution with bounciness and friction, contacts exposed as output,
  and deferred trigger enter/exit events. Remaining work is in [PHYSICS_ROADMAP.md](PHYSICS_ROADMAP.md).

- [x] **Tilemap collisions** — tile solidity, six tilemap tests, and axis-separated swept movement
  in `integrate`: X then Y, each clipped by a shape cast, so a tile grid's fake interior faces can
  never push a body sideways. `depenetrateTilemaps` runs last because the entity pair loop is what
  pushes bodies into tiles in the first place.

- [x] **Substepping** — `PhysicsManager::update(registry, dt, steps)` around a public `substep`.
  `beginStep` and the trigger dispatch stay outside the loop, or a pair touching in one substep and
  not the next would fire enter and exit inside a single frame.

- [x] **Scene queries and shape casts** — point/box/circle overlap, ray, and circle/box casts on
  `PhysicsManager`, each in closest and `*All` form, defined in `PhysicsQueries.cpp`. A cast is a
  ray against the target grown by the moving shape, so nothing steps: two boxes sum to a box,
  anything with a circle sums to a rounded box and shares one `testRayRoundedBox`.

### UI

- [x] **UI input capture** — `UIManager::isMouseUsed()`/`isKeyboardUsed()` (hover-or-capture, and whether anything holds focus) pushed into `Input::setUiCapture` each frame; every key and button query answers "nothing happened" while captured, so gameplay needs no guard and axes go quiet on their own. The UI reads between the clear and the set, so it never filters itself out.

- [x] **Multi-line text input, selection and clipboard** — `textArea` beside `textField` over one editing core: anchor-based selection, clipboard, word jump, per-line Home/End, PageUp/Down. Caret geometry comes from a real `TextBlock` laid out by the same calculator, so it cannot drift from the glyphs.

- [x] **Keyboard focus + text input** — `Input::getTypedText()` (frame UTF-8, glfw char callback) and `keyRepeated()` (glfw key callback, so the OS repeat rate). Focus is `UIManager::m_focusedKey`, claimed on press by the first `style.focusable` ancestor, with Tab cycling and an `onFocused` style state.

- [x] **Grid** — `openGrid`/`closeGrid`, cells are the children in order. Twelve columns by default with `.gridSpan` carving them up; `.columns` takes a count or a `UISizeSpec` track list. Brought `UIAlign::Stretch` with it.

- [x] **`UINodeState` drag outputs** — `dragDelta`, `pressOrigin`, `grabOffset`, `scrollDelta`, `isDoubleClicked`. Measured from the press, never accumulated: summing per-frame deltas drifts worst on the thing being dragged.

- [x] **Event consumption** — `style.blockInput` bounds press/held/released to the first node that claims the click; hover keeps bubbling past it, matching CSS `:hover`.

- [x] **Root sizes against the window** — `Grow`/`Percent` roots measure against `UIRenderer::getRootSize()`. `Fit`/`Fixed` never read it, so every root predating this is unchanged.

- [x] **`cursor`** — `Window::setCursor` over a `GlfwContext` cursor cache. `UICursor::Default` means "no opinion": the shape is the first node naming one, walking outward from the pointer.

- [x] **Clipping honours the parent's border** — `childClipRect` is inset by the *painted* border width; `clipRect` deliberately is not, so a node still reaches its own ring and shadow.

- [x] **`TextOverflow` clip + ellipsis** — `Ellipsis` is a layout post-pass run before alignment; both values bound glyphs at draw time, intersected with the caller's clip rather than replacing it.

- [x] **Richer input return values** — every bound-value widget returns `UIInputState` (`isChanged`/`isEditing`/`isReleased`), so expensive work hangs off release instead of every drag frame.

- [x] **Scroll** — `overflow = Scroll` is the whole interface: clips, zeroes the content floor, drives the offset from the store and draws the bars. `contentSize` comes from solved child sizes, not intrinsics, or a nested scroller reveals nothing. Bars are derived from geometry, not nodes.

- [x] **Retained state store** — `UIStateStore` keyed by `persistentKey`: a system struct plus a `string -> float` bag, with frame-age eviction. Folded in five ad-hoc maps and their keying bugs.

- [x] **Theming** — `UIThemeManager` with 21 colour roles derived from three seeds, a metric ramp and named text styles. Widgets read roles inline; the UI core reads only the scale.

- [x] **UI scale** — `UIThemeMetrics::scale` multiplies every pixel-valued field once in `addNode`. Scaling the *inputs* keeps the solve in real pixels, so hit testing needs no changes; feeding solved geometry back needs `unscale`.

- [x] **Widget layer over the primitives** — `UIWidgets` is the single public face: thin forwarders plus one free function and config struct per widget. No widget touches `UIManager`'s privates.

- [x] **Window widget** — movable, resizable, collapsible, geometry in the state store. Collapse discards content in `closeWindow`, since immediate mode cannot stop a caller declaring it.

- [x] **Overlay machinery** — `ignoreClip`, `ignoreInput` and `zIndex`, with hit resolution ordered by the same three keys the painter uses. `tooltip`, `colorPickerPopup` and `dropdown` sit on it.

- [x] **Optional style/layout structs** — `UIContainerStyle`, `UITextStyle` and `UILayoutConfig` are generated from one X-macro each, with `combine` and `fillDefaults`. Adding a field is one line.

- [x] **Per-state styles** — `UIContainerStyleSpec` carries `onHover`/`onHeld`/`onPressed`/`onReleased`, merged lowest priority first. `onHeld` reads `isActive`, so a grip dragged off itself stays lit.

- [x] **Mouse capture + paint layers** — one `m_activeKey` holds the pressed node until release wherever the cursor goes; `draw()` flushes boxes and glyphs per layer, or glyphs paint over boxes.

- [x] **Hit resolution** — resolved once in `clear()`, pairing this frame's mouse with last frame's geometry. Last hit in paint order wins, then hover bubbles to every ancestor.

- [x] **Overflow clipping** — two rects per node resolved top-down; both shaders discard outside, and hit testing reads the same rect.

- [x] **Containers paint through `UIRenderer`** — one quad carries background, image, border ring, per-corner radius, dashes and shadow from a single rounded-box SDF. Nothing visible emits no geometry.

- [x] **UI space switch** — `setSpace` picks screen or world, and the matrix, root origin and mouse all read the one field. Screen space is Y-up because glyph quads are built baseline-up.

- [x] **Per-corner border radius + image rotation** — `UICorners` with the SDF picking the radius per fragment; rotation reuses a spare vertex slot instead of growing the vertex.

- [x] **Engine-owned assets** — `FileManager::engineAsset()` resolves from `engine/assets` whatever the game's cwd, so the engine stays redistributable.

- [x] **Window projection on `ViewContext`** — view and proj stored separately; `UIRenderer` no longer builds its own.

- [x] **UI font** — `UIManager::setFont` for the shared face, `UITextConfig::font` to override per leaf. Null is normal and resolves to `FontLoader`'s default.

- [x] **Pre-rewrite UI (superseded)** — the old `UiSystem`/`UiRenderer`/`LayoutCalculator` stack landed in full and was deleted in `a886a32`. Recover with `git show a886a32^:<path>`.

### Text

- [x] **Loading fonts measure with the face they borrow** — `Font::getMetrics` answers from `FontLoader`'s default while baking instead of `PLACEHOLDER_METRICS`, so measuring a font directly agrees with the layout. Only metrics are borrowed; a glyph's uvs are meaningless against another atlas.

- [x] **Cold bake proven end to end** — `text_rendering_test` dropped its two `waitForLoad` calls, which had frozen the whole scene before the first frame, and refits per frame instead. The default font now outranks cost in `FontLoader`'s queue: a cheaper job overtaking it delays every font that borrows it.

- [x] **Text layout folded out of `TextRenderer`** — wrapping, line boxes and alignment in one stateless `TextLayoutCalculator` over a `TextBlock`. `TextMetrics::step` is shared by the measuring and emitting walks, so widths cannot drift.

- [x] **Async font loading** — the constructor queues the bake and returns; a loading font answers from a placeholder and the atlas uploads on the GL thread. The baker takes a quarter of the cores — at full width it starved the render thread.

- [x] **Default font on `FontLoader`** — bakes a system face, queued ahead of whatever triggered it, so a font still baking borrows a real face instead of drawing placeholder boxes.

- [x] **Text renderer** — `TextRenderer` over `BatchRenderer<TextVertex>`, with `/s` span tags. Everything a style varies is per-vertex, so many styles still share one flush.

- [x] **Text effects** — outline, boldness, softness, shadow, italic skew, underline, strikethrough. All widths in **em**, not pixels, or they drift under zoom.

- [x] **Font resource + msdf baker** — `Font` is an `IResource` producing an atlas plus em metrics. The disk cache in `.cache/fonts/` is the real interface, so neither freetype nor msdfgen is needed to load one.

### Engine

- [x] **`Application` loop wrapper** — owns the window loop, `Time::update`, per-window input and camera ordering; scenes supply `onFrame`/`onWindowUpdate`/`onWindowRender`.

- [x] **`Delegate` binds plain callables** — a `requires`-constrained `bind(C*)` for lambdas and functors. Stores only a pointer, so the callable must outlive the delegate.

- [x] **Per-sprite shaders** — `SpriteComponent::shader`; `renderSprites` sorts by `(layer, shader, texture)` and flushes on a shader boundary.

- [x] **Separate alpha blend func** — `glBlendFuncSeparate`; the plain version multiplied source alpha in twice against the transparent FBO and ate antialiased edges.

- [x] **HSV on `Color`** — `toHsv()`/`fromHsv()`, moved off the colour picker so anything can use them.
