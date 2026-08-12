# Physics & Collision — implementation roadmap

Living plan for the collision/physics system. Steps are ordered so each one is
testable on its own. Reasoning that outlives the step belongs in CLAUDE.md or next
to the code; this file is the order of work.

## The invariants

**Every narrowphase test returns the same `Contact`,** normal running A → B. Shape pairs
differ; resolution does not.

**Shape tests never see a component.** `Box`/`Circle` are plain structs from
`toBox`/`toCircle`. That is what keeps the resolver shape-blind and makes a new shape an
addition rather than a rewrite. Entity-facing wrappers sit above them and funnel through one
`dispatch` helper, so a new shape is one more callback, not a switch in every function.

**The tilemap is a query, not a shape.** *Given a world AABB, yield the solid tile boxes it
overlaps* — then run the same three tests. Three tests, not six.

**A cast is a ray against a grown target.** Minkowski sum, so `testRayRoundedBox` covers
every shape cast and nothing steps.

---

## Done

**The loop.** Fixed timestep on `Time` — dt clamp, step cap, discarded remainder, so a stall
cannot death-spiral — drained by `Application::onFixedUpdate`, per frame not per window.
`PhysicsManager::update(registry, dt, steps)` splits the frame into substeps and wraps them
in one `beginStep` / `dispatchTriggerEvents` pair; `substep` runs integrate →
collideEntities → depenetrateTilemaps. Broadphase is still the naive double loop.

**Components.** `ColliderComponent` — one component, `shape` picks which size field is read,
`TransformComponent::scale` multiplies the extents. `RigidBodyComponent` — `mass` rather
than inverse mass, plus `bounciness`, `friction`, `gravityScale`, `BodyType`. No rigidbody
at all = static.

**Shapes and resolution.** Box/box, circle/circle, box/circle, each returning a `Contact`;
the dispatch canonicalises pair order and negates the normal on the one asymmetric pair.
`resolveContact` splits into `correctPositions` (slop-adjusted, divided by inverse mass) and
`applyImpulse` (`max` bounciness, rest threshold, Coulomb-clamped friction) so a swept mover
can take the velocity half alone.

**Tilemap collisions.** `TileGrid::fromTransform` is the adapter — `position` is the corner
of tile (0,0), `scale` is the tile size, honoured identically by `TilemapRenderer` and every
test. Solidity is `TileDefinition::isSolid` through `TilemapManager::isSolidAt`. Six tests in
`TilemapCollisions.cpp`: point, ray, box, circle and both casts, the ray by grid traversal
and the rest by walking the tiles in the query's AABB.

**Axis-separated movement.** `integrate` sweeps X then Y instead of moving outright, so a
tile grid's fake interior faces can never push a body sideways along a flat floor. Falls out
of `sweepTilemaps` + `testEntityCastEntity`; with no tilemap present it degenerates exactly
to `position += velocity * dt`. `depenetrateTilemaps` runs last because the pair loop is what
pushes bodies into tiles — it has no idea one side is resting on one.

**Contacts and triggers as output.** `getContacts(registry)` plus a per-entity overload that
reorients the normal away from the queried entity; `impactSpeed` sampled before the solver
zeroes it. Trigger pairs are packed `uint64_t` keys diffed against the previous frame and
fired through `Signal` *after* the loop, so an enter is knowable and a listener cannot
invalidate the view it is iterating.

**Queries and casts.** Point/box/circle overlap, ray, and circle/box casts in closest and
`*All` forms on `PhysicsManager`, all closed-form.

**`physics_test` scene.** A tiled room with a resizable wall/ceiling perimeter and generated
ledges, every body type, a kinematic platform, a trigger box, scene-local debug draw, a UI
window driving room, world and spawn settings live, a query-mode selector, and a trigger
enter/exit counter proving the diffing fires once per crossing.

Layout: `Collisions.hpp` declares the namespace; `engine/src/physics/collisions/` holds
`CollisionTests`, `CollisionQueries`, `CollisionResolve`, `ShapeCasts`, `TilemapCollisions`
and `EntityCollisions`; `PhysicsManager.cpp` is the step loop and `PhysicsQueries.cpp` the
query members.

---

## Next

### 1. Surface materials

Friction and bounciness live on `RigidBodyComponent`, and the combination rules are
`max(b1, b2)` and `sqrt(f1 * f2)`. Anything without a rigidbody — a static box, a tilemap —
reads as zero on its side, so **bounciness survives but friction collapses to nothing**
against every static surface in the engine today. That is a real hole, not a tilemap quirk.

Move the material fields onto `ColliderComponent`, and add a per-tile pair on
`TileDefinition` so ice and mud are authoring data. Decide at the same time whether a
missing material means "frictionless" or "use the other side's".

### 2. One-way and custom-bounds tiles

`TileDefinition::isSolid` is a bool; the shape it wants is `None | Full | OneWay |
Custom(min,max)`. One-way platforms need the sweep to ignore a tile whose surface the body is
moving *away* from, which is a small change in `forEachSolidTile` and nowhere else — but
only while the callers still go through it, so it is much cheaper now than retrofitted.

### 3. Forces and impulses

Gameplay can only write `velocity` directly today, which means every caller re-derives
`impulse / mass` and nothing can accumulate several pushes in one step. Wants
`addForce` / `addImpulse` on the body plus an `applyRadialImpulse(centre, radius, strength)`
built on the existing `circleTest` — explosions and knockback are the two things every game
needs and neither is expressible right now.

### 4. Playable character in the test scene

A controllable body dropped into the existing scene — the ball pit. This is the step that
actually exercises everything above, because a controller is the one thing that notices
when contacts are subtly wrong.

- Grounded / wall-touching from the contact list (`normal.y > 0.7`), not a separate raycast.
- A capsule is the usual answer; a box plus a circle-cast for the feet is the cheap one,
  and the casts are already there.
- Dynamic body so the pit shoves it around, with high friction, zero bounciness and
  direct velocity control on the horizontal axis.
- **Resolves the open question from the fixed-step work:** `onFixedUpdate` runs before
  `Input::update` (input is per-window, inside the window loop), so a controller reading
  input there gets it one frame stale. Either the controller latches input in `onFrame`
  and consumes it in `onFixedUpdate`, or the loop order changes. Decide here, with
  something on screen to feel the difference.

### 5. Layers and masks

Everything tests against everything. A real game needs the player to pass through pickups,
enemies to ignore each other, a bullet to hit terrain but not its shooter, and a query to ask
about one category. A layer id plus a collision matrix, checked in the pair loop and passed
as an optional filter on every query and cast.

Was parked as "not designed yet" pending a broader layer component shared with rendering.
That is still the nicer shape, but the physics side is blocking real gameplay and the two can
be reconciled later. It also cuts the pair count, so it pays for itself twice.

### 6. Render interpolation

Physics runs at a fixed rate and rendering does not, so motion currently beats visibly
against the display refresh. Store `prevPosition` at the top of `update`, lerp by the
leftover accumulator at render time. Cheap, and every fixed-step engine needs it — the
reason it reads as "polish" is that the test scenes are all short.

### 7. Hardening pass

The named failure modes, not a vague "fix bugs":

- resting jitter — a body oscillating on a floor; the penetration slop exists, tune it.
- resolution order dependence — sparse-set iteration order changes the outcome.
- corner cases where two axes tie on penetration depth.
- a shape cast that starts already overlapping, which reports distance 0 with a zero normal.
  `integrate` currently lets that move through so an embedded body can escape, and
  `depenetrateTilemaps` is what stops it becoming a phase-through.
- a kinematic body that reaches a tile stops dead — `applyImpulse` early-returns on zero
  inverse mass, so it keeps its velocity and never makes progress. Anything driven by
  position thresholds wedges.
- one-way platforms: moving up through them, and standing on the edge.

### 8. Continuous collision between entities

The tilemap path cannot tunnel because it sweeps, but entity-vs-entity still moves then
pushes out, so a fast body passes straight through a thin one. Opt-in per body, since making
it unconditional costs a cast per pair. `testEntityCastEntity` already exists — this is
mostly deciding when to spend it.

### 9. Joints and constraints

A distance constraint gets ropes, chains, swinging platforms and grappling hooks; a hinge
gets doors and ragdolls. Wants a constraint list on `PhysicsWorld` solved after contacts,
which is also the point at which iteration count starts to matter.

### 10. Rotated colliders and angular motion

Opt-in per collider. Adds `testObbObb` + `testObbCircle` and one branch in the dispatch;
because the tests take `Box`/`Circle` structs and not components, nothing else changes.

Deliberately last, because it is the one entry that touches everything else: OBBs mean SAT
and real manifolds, angular motion means `angularVelocity` / `torque` / inertia on the body
and an impulse applied at a contact *point* rather than through the centre, and the analytic
shape casts do **not** survive it — Minkowski sums of OBBs are not OBBs, so a rotated cast
needs conservative advancement.

---

## Optimizations

**Features first — none of this is scheduled.** Parked here so the reasoning is not lost.

Nothing below changes behaviour; if an entry would, it says so. Rough cost at the moment,
120 bodies × 4 substeps, one frame:

- `collideEntities` — 7140 pairs × 4 = **~28,500 exact narrowphase tests**, with no cheap
  rejection in front of them and 2–3 sparse-set lookups per side per test.
- `sweepTilemaps` — 2 axes × 120 bodies × 4 = **960 calls**, each building a `View` and
  walking a tile range.
- `depenetrateTilemaps` — **480+ full tile-range scans**, plus a heap allocation per substep.

Ordered by effort, cheapest first.

### 1. Stop allocating every substep

`depenetrateTilemaps` builds a `std::vector<Entity>` of tilemaps on every call and
`collideEntities` builds one of colliders; at 4 substeps that is 8 heap round-trips a frame
for lists that rarely change. Hoist both into scratch buffers on `PhysicsWorld`.

### 2. Hoist the per-body `View` construction

`sweepTilemaps` constructs a `View<TilemapComponent>` and an `EntityHandle` per axis per
body — 960 views a frame to iterate a list that is almost always length one. Gather the
tilemaps once per substep and pass the span down.

### 3. Cache component pointers in the gathered list

Every `testEntities` re-`tryGet`s collider, transform and rigidbody through the sparse sets,
so the pair loop pays 4–6 lookups per pair before any maths. Gather
`{Entity, Transform*, Collider*, RigidBody*}` once per substep and the loop pays none.

### 4. AABB reject in front of the narrowphase

The single biggest cheap win. The pair loop goes straight to the exact test today; a stored
per-body AABB and a four-comparison overlap check rejects almost every pair for a handful of
instructions. Does not remove the O(n²), but makes the constant tiny.

### 5. Solver iterations, separate from substeps

**Probably the right fix for deep piles.** A substep currently re-runs integration and every
tilemap sweep just to get another contact-solve pass, which is why raising the count is so
expensive. Running only the impulse pass N times over the existing contact list costs a
fraction of that and is what actually settles a stack. Changes behaviour — for the better,
but it needs re-tuning against `correctionPercent`.

### 6. Sleeping bodies

A body whose velocity stays under a threshold for N steps stops integrating and drops out of
the pair loop until something touches it. In a ball pit most bodies are asleep within
seconds, so this is the largest real-world win of anything here. Needs a wake path on
contact, on teleport and on any external velocity write.

### 7. Per-chunk solid bitmask

Already listed under tilemap collisions and worth repeating here: `isSolidAt` is a chunk hash
lookup plus a tileset indirection **per tile**, inside every `forEachSolidTile`. 32
`uint32_t` rows per chunk, rebuilt where `isDirty` is already handled, turns the inner loop
into bit tests.

### 8. Grid broadphase

Uniform spatial hash sized to a few tiles, rebuilt each substep; test against the current
cell and its neighbours. Replaces the body of the pair loop and nothing else — the scene
queries in `PhysicsQueries.cpp` should route through it at the same time, since they are all
full-registry scans today. No threading yet: with a tile world the dominant cost is
entity-vs-tilemap, which has no pair loop to parallelise in the first place.

### 9. Walk the swept tiles instead of scanning the AABB

`testBoxCastTilemap` and `testCircleCastTilemap` scan every tile in the swept bounding box.
A fast mover across a large room scans a lot of empty space to find one hit. `testRayTilemap`
already does the right thing with grid traversal; the shape casts want the same walk, widened
by the shape's extent.

### 10. Reuse broadphase pairs across substeps

Bodies move very little inside one frame, so the pair set is nearly identical between
substeps. Build it on the first substep with a small margin on the AABBs and reuse it for the
rest. Approximate by construction — a fast pair can be missed for one frame.

### 11. Contact caching and warm starting

Persist contacts across steps and seed each solve with the previous accumulated impulse.
This is what lets a tall stack settle in one or two iterations instead of ten, so it partly
subsumes both substepping and item 5. The hard part is stable contact identity across
frames — a pair key plus a feature id — which is also the prerequisite for stay events.

### 12. Islands, then threads

Partition the contact graph into connected components and solve each independently. Makes
resolution order deterministic per island, and is the only safe basis for threading the
solver. Not worth attempting before 5, 6 and 8 are in.

### 13. Data layout

Pack the hot physics fields into parallel arrays rather than reaching through components.
Real gains at high body counts, but it fights the ECS the rest of the engine is built on, so
it is last on purpose and may never be worth it.

### Not scheduled

Deliberately out of scope, not merely unstarted:

- **Soft bodies, cloth and fluids** — a different solver, not an extension of this one.
- **Polygon and per-triangle colliders** — boxes, circles and a tile grid cover 2D level
  geometry; arbitrary convex hulls mean GJK/EPA everywhere and every closed form here dies.
- **Determinism across machines** — needs fixed-point or a locked FP contract. Worth
  revisiting only if networked play ever matters.
