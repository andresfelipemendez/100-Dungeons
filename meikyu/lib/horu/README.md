# 彫 horu

constructive solid geometry for the engine: carve solids from primitives via
boolean ops. sibling library to the other [lib/](..) modules -- the engine and
games consume it, it is not the engine.

a solid is a flat soup of CCW triangles (`horu_mesh`); normals derive from
winding, positions reuse linalg's `vec3` so a mesh drops straight into engine
rendering with no conversion layer. boolean union / difference / intersection
run the classic BSP clip (Naylor-Amanatides-Thibault): build a BSP tree from
one solid, clip the other's polygons against it, keep the fragments the op asks
for, stitch. robust enough for level geometry, header-only types, no
exact-arithmetic solver.

## usage

```c
#include "horu.h"

horu_mesh room  = horu_box(vec3_make(0,0,0), vec3_make(8,4,8));
horu_mesh pillar = horu_box(vec3_make(0,0,0), vec3_make(1,4,1));
horu_mesh carved = horu_difference(&room, &pillar); /* room minus pillar */

/* feed carved.tris / carved.count to the renderer ... */

horu_free(&room);
horu_free(&pillar);
horu_free(&carved);
```

## status

- **done:** mesh type, `horu_push` / `horu_free`, `horu_box`, `horu_sphere`,
  tests (volume via divergence theorem as a watertightness check).
- **pending:** the BSP boolean core. steps 1-3 (tree build, polygon split,
  fragment tagging) are mechanical; step 4 -- `horu_select_fragments`, which
  fragment sets each op keeps + the winding flip on the carved-away solid -- is
  the part that defines union vs. difference vs. intersection. see the TODO in
  `horu.c`. `horu_union/difference/intersection` return the empty mesh until
  it lands; `test_boolean_pending` pins that and flips to real assertions once
  wired.

## test

```sh
./test.sh        # gcc -std=c99, links horu.c + linalg, runs build/test.out
```
