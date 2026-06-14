# 彫 horu

constructive solid geometry for the engine: build solids from primitives and
combine them with boolean ops, then triangulate to a renderable mesh. sibling
library to the other [lib/](..) modules -- the engine and games consume it, it
is not the engine. strict C89, no allocator (caller-sized buffers), no vendor
deps.

horu is two parts, exactly as scoped: the boolean works on an **abstract**
representation, and triangulation is a separate downstream pass.

## part 1 -- the abstract boolean (planes)

Solids are represented by **planes** (half-spaces), never triangles, so point
membership is a pure plane/tree question:

- `horu_plane_make`, `horu_plane_from_points` -- oriented planes (unit normal).
- `horu_side(plane, x,y,z)` -- the atom: +1 front / -1 behind / 0 on (epsilon).
- `horu_solid` -- a convex solid = intersection of outward-facing half-spaces;
  `horu_contains` is "behind every face". `horu_box` builds one (6 planes).
- `horu_csg` -- the boolean abstract: a tree whose leaves are convex solids and
  whose nodes are union / difference / intersection. `horu_csg_contains` walks
  it. Closed under all ops, `int`-indexed, pointer-free (hot-reload safe).

```c
horu_solid a = horu_box(0,0,0, 2,2,2);
horu_solid b = horu_box(1,0,0, 2,2,2);
horu_csg t; horu_csg_init(&t);
int la = horu_csg_leaf(&t, &a), lb = horu_csg_leaf(&t, &b);
horu_csg_op(&t, HORU_DIFFERENCE, la, lb);     /* a - b */
int inside = horu_csg_contains(&t, 0.5f, 0,0);
```

## part 2 -- triangulation (exact CSG via BSP polygon clip)

To render, solids are carried as **polygons** (convex, CCW, outward normal)
that are split and clipped against BSP trees -- the classic Naylor / csg.js
algorithm, data-oriented (flat node + polygon pools, internal-static scratch):

- `horu_split_poly` -- the atom: cut a polygon by a plane into front/back
  pieces, interpolating edge crossings. `horu_flip_poly` inverts one.
- `horu_box_polys` -- box -> its 6 face quads.
- `horu_csg_polys(op, a,na, b,nb, out,cap)` -- the exact boolean: build a BSP
  from one solid's polygons, clip the other against it, invert/clip/build per
  op (union / difference / intersection). Sharp edges, exact.
- `horu_mesh_from_polys` -- fan-triangulate polygons into a structure-of-arrays
  mesh (positions + 32-bit indices) the caller sizes.

```c
horu_poly cube[8], bar[8], out[256];
int nc = horu_box_polys(0,0,0, 2,2,2, cube, 8);
int nb = horu_box_polys(0,0,0, 6, 0.7f, 0.7f, bar, 8);
int n  = horu_csg_polys(HORU_DIFFERENCE, cube, nc, bar, nb, out, 256); /* drill */
/* out -> horu_mesh_from_polys -> vertex/index buffers -> renderer */
```

dungeon1 renders a cube drilled by three orthogonal bars this way; the holes
expose interior faces, proving the boolean.

## status

Both parts complete. Verified by volume invariants (`A-A=0`, `A∪disjoint=sum`,
`A∩A=A`, overlap cases) and rendered live in dungeon1.

`horu_csg_polys` is bounded to inputs of a few hundred polygons (fixed BSP node
pool); it does not deduplicate result vertices and uses no exact-arithmetic
solver -- robust for level/asset geometry, not a CAD kernel.

Coverage: gated at 90% (`cov=90` in `build.manifest`), measured ~91.7% MC/DC /
99% branch. Everything but the BSP boolean is effectively 100% branch + MC/DC;
the residual handful are defensive buffer-capacity guards in the recursive clip
(a polygon clipped past `HORU_POLY_MAX`, a negative BSP node index) that only
fire on pathological inputs -- kept for safety rather than chased with contrived
tests.

## test

```sh
meikyu --test horu              # build + run via kaji
meikyu --test horu --coverage   # + branch/MC-DC gate (cov=90)
meikyu --mutate horu            # mutation testing (operator-swap)
```
