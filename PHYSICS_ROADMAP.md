# Physics & Collision — implementation roadmap

Living plan for the collision/physics system. Steps are ordered so each one is
testable on its own. Reasoning that outlives the step belongs in CLAUDE.md or next
to the code; this file is the order of work.

## The one invariant

**Every narrowphase test returns the same `Contact`.** Shape pairs differ, resolution
does not:

```cpp
struct Contact { Vec2 normal; float depth; Vec2 point; };   // normal points A -> B

bool testBoxBox(const Box&, const Box&, Contact& out);
bool testCircleCircle(const Circle&, const Circle&, Contact& out);
bool testBoxCircle(const Box&, const Circle&, Contact& out);
```

`Box`/`Circle` are plain structs built from `(TransformComponent, ColliderComponent)`
by a small adapter — the test functions never see a component or an `Entity`. That is
what keeps the resolver shape-blind, keeps the tilemap from needing its own tests, and
makes rotated colliders a later addition rather than a rewrite.

The tilemap is **not** a fourth shape. It is a query: *given a world AABB, yield the
solid tile boxes it overlaps.* Its collisions run through `testBoxBox` and
`testBoxCircle` like everything else. Three tests, not six.

---

## Steps

### 1. Fixed timestep on `Time`

Accumulator in `Time`, drained by the caller:

```cpp
float fixedDeltaTime() const;      // 1/60 default, settable
bool  consumeFixedStep();          // decrements the accumulator; false when drained
```

Clamp to ~5 steps per frame — an unclamped accumulator turns one stall into a death
spiral. `consumeFixedStep` rather than `isFixedUpdateFrame`: a bool can never say
"three steps this frame", and a long frame would silently run physics slow.

### 2. `Application::onFixedUpdate`

New `Delegate<void(float)> m_onFixedUpdate`, run as
`while (Time::get().consumeFixedStep()) { physics step; m_onFixedUpdate(fixedDt); }`.

`Delegate` is single-bind, so `Application` calls the physics step itself and the
delegate is the scene's slot, not physics'.

**Open:** where in the loop. Today `onFrame` runs *before* `Input::update` (input is
per-window, inside the window loop). Physics does not care, but a character controller
in `onFixedUpdate` would read one-frame-stale input. Decide when the controller lands.

### 3. Debug draw for colliders — *before any collision code*

Wireframe boxes and circles over the existing `addLine`/`addFrame`, plus contact
normals once there are contacts. Already on TODOS.md as "Debug draw channel". Every
step after this is debugged by looking at it; without it the whole roadmap is blind.

### 4. `ColliderComponent`

One component, not one per shape — the physics loop stays a single view, and "does this
entity collide at all" stays one pool lookup.

```cpp
enum class ColliderShape : uint8_t { Box, Circle, Tilemap };

struct ColliderComponent
{
    ColliderShape shape = ColliderShape::Box;
    Vec2 halfExtents = VEC2_ONE * 0.5f;   // Box
    float radius = 0.5f;                  // Circle
    Vec2 offset = VEC2_ZERO;
    bool isTrigger = false;
};
```

Two size fields rather than an overloaded one: four wasted bytes buys a field name that
means what it says. `shape` decides which is read.

`Tilemap` reads the sibling `TilemapComponent` on the same entity instead of carrying
geometry — one component type still answers "is this collidable".

**Decide:** does `TransformComponent::scale` multiply the extents? Recommended yes (box
by `abs(scale)`, circle by its max component), so a scaled sprite's collider follows it.
Physics uses `position.xy` only and never writes `z`.

### 5. `RigidBodyComponent`

```cpp
enum class BodyType : uint8_t { Static, Kinematic, Dynamic };

struct RigidBodyComponent
{
    Vec2 velocity = VEC2_ZERO;
    float inverseMass = 1.0f, restitution = 0.0f, friction = 0.0f, gravityScale = 1.0f;
    BodyType type = BodyType::Dynamic;
};
```

This replaces a "skip resolution" flag. **No rigidbody at all = static** — it blocks,
it is never moved, it is never integrated. Kinematic moves and pushes but is never
pushed back (moving platforms). Static-vs-static is never tested, which is also why
tilemap-vs-tilemap needs no opt-out field.

"Trigger" and "static" are different axes and belong on different components: trigger
is what a contact *means*, body type is how the entity *responds*.

### 6. `PhysicsManager` skeleton + `physics_test` scene

Stateless singleton in the house style — `PhysicsManager::get().step(registry, dt)` —
with its persistent state in `registry.getContext<PhysicsWorld>()`, so nothing leaks
between scenes and two registries work without the manager knowing.

Four phases from the start, even while three are empty:

```
integrate -> broadphase pairs -> narrowphase contacts -> solve
```

Broadphase is a naive double loop behind a `pairs()` call. The grid in step 10 replaces
the body of that one function and nothing else.

Ship `game/src/tests_scenes/physics_test.cpp` in this step — it is the harness for
everything below, and a new `.cpp` needs a CMake *configure*, not just a build.

### 7. Box/box, detection **and** resolution

The whole solve path, once, on the easiest shape:

- MTV: overlap per axis, push out along the axis of least penetration.
- Split the correction by inverse mass; dynamic-vs-static moves only the dynamic one.
- Zero the velocity along the contact normal. Restitution/friction optional, `e = 0`
  default.
- **Emit contacts as output.** Grounded checks, wall slides and coyote time are all
  `normal.y > 0.7` on that list. Bolted on later it is always worse.
- Trigger colliders: detect, record, skip the solve. Cheap here, and it lets detection
  be tested without resolution muddying it.

Ignore `transform.rotation` — see step 12. Rest of the shapes are additions to this;
this is the step that has to be right.

### 8. Circle/circle and box/circle

Detection only. Both return a `Contact`, so the solver from step 7 is untouched.

### 9. Tilemap collisions

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

---

## Further out

### 10. Grid broadphase

Uniform spatial hash sized to a few tiles, rebuilt each step; test against the current
cell and its neighbours. Swaps in behind `pairs()` from step 6. No threading — in a
Terraria-like the dominant cost is entity-vs-tilemap, which has no pair loop at all.
The phase split leaves the door open if profiling ever disagrees.

### 11. Hardening pass

The named failure modes, not a vague "fix bugs":

- resting jitter — a body oscillating on a floor; needs a small penetration slop.
- resolution order dependence — sparse-set iteration order changes the outcome.
- corner cases where two axes tie on penetration depth.
- fast movers vs thin colliders that the half-tile clamp does not cover.
- one-way platforms: up-moving, and standing on the edge.
- triggers overlapping without ever resolving.

### 12. Trigger events

Enter/stay/exit, not just "overlapping". Packed pair keys (`uint64_t` from two entity
ids) in `PhysicsWorld`, current diffed against previous each step, fired through the
existing `Signal`/`Sink`. The `isTrigger` bool and the contact list land back in step 7;
only the event diffing is here.

### 13. Optional rotated colliders

Opt-in per collider. Adds `testObbObb` + `testObbCircle` and one branch in the dispatch;
because the tests take `Box`/`Circle` structs and not components, nothing else changes.
Deliberately not first — OBBs mean SAT, real manifolds and eventually angular velocity
and inertia, for a game whose colliders are all upright.

### Not scheduled

- **Layers and masks** — intentionally out. A broader layer component that drives
  rendering and queries too is the better shape, and it is not designed yet. Until then
  everything tests against everything.
- **Render interpolation** — store `prevPosition`, lerp by the leftover accumulator at
  render time. Without it, motion beats visibly against the display refresh. Cheap,
  add when it becomes annoying.
