# 物 mono

the entity model for the engine: one fat `Entity` struct that carries every
field, held in a fixed **sparse** array — not archetype/component arrays. a game
nests `mono_world world;` in its hot state and addresses entities by stable
generational handles. pure + headless (no render, no UI, no allocator), so it is
coverage-gated like horu/tsumami. sibling [lib/](..) module; the engine and
games consume it, it is not the engine. strict C89.

## why fat AoS, not ECS

one type, one array. migration, serialization, editing, and undo all become
one-type / one-array operations — the simplicity axis that matters at dungeon
scale (the cache cost of fat AoS is irrelevant at hundreds–thousands of
whole-entity updates; hoist a hot field to a parallel array only if profiling
ever demands it). the shipped-games pattern (Doom `edict_t`, Quake). systems add
their fields to `Entity` as they land — the struct grows, the model doesn't.

## handles

an entity ref is a `mono_id` — generation in the high 16 bits, slot index in the
low 16 — never a pointer (the hot-state law). a slot's `kind == MONO_NONE` means
free (sparse, with holes — never compacted, so indices stay stable). freeing a
slot bumps its generation, so a handle held across a free+reuse resolves to NULL
instead of silently aliasing the new occupant. `MONO_INVALID` (id 0) is the
zeroed/null ref; generations start at 1 so it never matches a live slot.

```c
mono_world w;
mono_init(&w);
mono_id e = mono_spawn(&w, MONO_PROP);   /* identity transform; MONO_INVALID if full */
Entity *p = mono_get(&w, e);             /* NULL if dead or stale */
p->px = 3.0f;
mono_destroy(&w, e);                      /* mono_get(&w, e) is now NULL */

/* iterate the live set directly: */
int i;
for (i = 0; i < MONO_MAX; i++)
    if (w.ents[i].kind != MONO_NONE) { /* ... w.ents[i] ... */ }
```

## hot state + seni

`mono_state.h` is the one place the entity layout lives. it nests into a
project's `game_state` and seni migrates it via the **array-of-structs**
migration (`Entity ents[1024]`). like henshu's `EditorState`, prepend it to the
`game_state.h` snapshot with the marker's `layout_include` so the reload diff
resolves the field. constraint: the array size is a **literal** (`[1024]`) — seni
reads the layout as raw text and cannot expand `MONO_MAX`. every `Entity` member
must be seni-legal (scalar / fixed array / nested scalar struct, pointer-free).

`mono_world` is ~48 KB at `MONO_MAX = 1024` (well under the 1 MB hot block);
bump `HOT_SIZE` only when `Entity` grows fat or the cap rises.

## status

Core complete: spawn / destroy / get / alive + the sparse generational table,
100% MC/DC. Not yet wired into a game (a project nests `mono_world` and seni
migrates it — same step as henshu's editor state). `Entity` is minimal (kind +
transform); render/physics/etc. fields land here as those systems do.

## test

```sh
meikyu --test mono              # build + run via kaji
meikyu --test mono --coverage   # + branch/MC-DC gate (cov=100)
```
