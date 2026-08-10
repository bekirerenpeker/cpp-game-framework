# Physics & Collision — implementation roadmap

Living plan for the collision/physics system. Steps are ordered so each one is
testable on its own. Reasoning that outlives the step belongs in CLAUDE.md or next
to the code; this file is the order of work.

## The one invariant

**Every narrowphase test returns the same `Contact`.** Shape pairs differ, resolution
does not:

```cpp
struct Contact
{
    bool isTouching = false;
    Vec2 point, normal;       // normal points A -> B
    float depth;
    Entity entity = NULL_ENTITY;   // only set when testing against the whole registry
};
```

`Box`/`Circle` are plain structs built from `(TransformComponent, ColliderComponent)`
by `toBox`/`toCircle` — the test functions never see a component or an `Entity`. That is
what keeps the resolver shape-blind, keeps the tilemap from needing its own tests, and
makes rotated colliders a later addition rather than a rewrite.

The tilemap is **not** a fourth shape. It is a query: *given a world AABB, yield the
solid tile boxes it overlaps.* Its collisions run through `testBoxBox` and
`testBoxCircle` like everything else. Three tests, not six.

Shape casts follow from the same idea: a sweep is a ray against the target grown by the
moving shape, so `testRayRoundedBox` covers every cast and nothing steps.

---

## Done

- **Fixed timestep** — accumulator and step count on `Time`, with a dt clamp, a step cap
  and a discarded remainder so a stall cannot death-spiral. Drained by
  `Application::onFixedUpdate`, per frame rather than per window.
- **Components** — `ColliderComponent` (one component, `shape` picks which size field is
  read; `TransformComponent::scale` multiplies the extents) and `RigidBodyComponent`
  (`mass` rather than inverse mass; `bounciness`, `friction`, `gravityScale`, `BodyType`).
  No rigidbody at all = static.
- **`PhysicsManager` + `PhysicsWorld`** — stateless singleton over
  `registry.getContext<PhysicsWorld>()`. `step` runs beginStep → integrate →
  collideEntities → dispatchTriggerEvents. Broadphase is still the naive double loop.
- **All three narrowphase pairs** — box/box, circle/circle, box/circle, each returning a
  `Contact`; the dispatch canonicalises pair order and negates the normal on the one flip.
- **Resolution** — slop-adjusted positional correction split by inverse mass, then a
  normal impulse with `max` bounciness and a rest threshold, then Coulomb-clamped friction.
- **Contacts as output** — `getContacts(registry)` and a per-entity overload that
  reorients the normal away from the queried entity. `impactSpeed` is sampled before the
  solver zeroes it.
- **Trigger events** — detect, record, skip the solve; packed `uint64_t` pair keys diffed
  against the previous step and fired through `Signal` *after* the pair loop, so an enter
  is knowable and a listener cannot invalidate the view it is iterating.
- **Scene queries and shape casts** — point/box/circle overlap, ray, and circle/box casts
  in closest and `*All` forms on `PhysicsManager`, all closed-form.
- **`physics_test` scene** — every body type, a kinematic platform, a trigger box, debug
  draw local to the scene, a UI window driving world and spawn settings live, a query-mode
  selector, and a trigger enter/exit counter proving the diffing fires once per crossing.

Layout: `Collisions.hpp` declares the namespace; `engine/src/physics/collisions/` holds
`CollisionTests`, `CollisionQueries`, `CollisionResolve` and `ShapeCasts`;
`PhysicsManager.cpp` is the step loop and `PhysicsQueries.cpp` the query members.

---

## Next

### 1. Tilemap collisions

Two halves.

**Tilemap side.** Solidity is authoring data and belongs on `Tileset::TileDefinition`
(`None | Full | OneWay | Custom(min,max)`), looked up by tile id through a flat vector.
`TileData` stays 2 bytes. Cache a **solid bitmask per chunk** next to the mesh — 32
`uint32_t` rows, 128 bytes — rebuilt wherever `isDirty` is already handled, so a query
is bit ops instead of per-tile map lookups. One-way platforms are much cheaper designed
in now than retrofitted.

**Physics side.** The AABB→tile-range query, plus **axis-separated movement**: apply
`velocity.x`, resolve horizontally, apply `velocity.y`, resolve vertically. Per-tile MTV
without this catches on seams — walking across a flat floor, least-penetration at a tile
boundary comes out horizontal and shoves you sideways. Clamping per-step displacement to
under half a tile kills most tunneling too, and is far simpler than swept AABBs.

`ColliderShape::Tilemap` already exists and every test skips it, so this is additive.

### 2. Playable character in the test scene

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

### 3. Grid broadphase

Uniform spatial hash sized to a few tiles, rebuilt each step; test against the current
cell and its neighbours. Replaces the body of the pair loop and nothing else — the scene
queries in `PhysicsQueries.cpp` should route through it at the same time, since they are
all full-registry scans today. No threading: in a Terraria-like the dominant cost is
entity-vs-tilemap, which has no pair loop at all.

### 4. Hardening pass

The named failure modes, not a vague "fix bugs":

- resting jitter — a body oscillating on a floor; the penetration slop exists, tune it.
- resolution order dependence — sparse-set iteration order changes the outcome.
- corner cases where two axes tie on penetration depth.
- fast movers vs thin colliders that the half-tile clamp does not cover.
- one-way platforms: up-moving, and standing on the edge.
- initial overlap in a shape cast, which currently reports distance 0 with a zero normal.

### 5. Optional rotated colliders

Opt-in per collider. Adds `testObbObb` + `testObbCircle` and one branch in the dispatch;
because the tests take `Box`/`Circle` structs and not components, nothing else changes.
The analytic shape casts do **not** survive this — Minkowski sums of OBBs are not OBBs, so
a rotated cast needs conservative advancement. Deliberately last: OBBs mean SAT, real
manifolds and eventually angular velocity, torque and inertia, for a game whose colliders
are all upright.

### Not scheduled

- **Layers and masks** — intentionally out. A broader layer component that drives
  rendering and queries too is the better shape, and it is not designed yet. Until then
  everything tests against everything.
- **Render interpolation** — store `prevPosition`, lerp by the leftover accumulator at
  render time. Without it, motion beats visibly against the display refresh. Cheap,
  add when it becomes annoying.
