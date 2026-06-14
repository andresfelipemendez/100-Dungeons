# 摘 tsumami

3D manipulation for the engine: ray-picking and a drag gizmo for moving objects
in a viewport. pure and headless -- the gizmo is a state machine driven by
synthetic rays + mouse edges, no UI, no render -- so it is fully testable and
reusable for CSG-node, mesh, or entity editing. sibling library to the other
[lib/](..) modules; the engine and games (and [henshu](../henshu)) consume it,
it is not the engine. strict C89, no allocator, no vendor deps.

## shape

- `tsu_ray_from_screen` -- build a world-space pick ray from the camera basis
  (eye/fwd/right/up), projection half-angle, aspect, and the mouse in pixels.
- `tsu_ray_aabb` -- slab-method ray/box hit distance (`< 0` = miss). the atom.
- `tsu_target` -- a pickable: an id, a center + half-extent AABB, an origin, and
  an `axis` (`-1` = free body, `0/1/2` = an axis-locked handle).
- `tsu_pick(ray, targets, n)` -- nearest hit; **handles win over bodies** so an
  axis arrow in front of its own body still grabs.
- `tsu_gizmo` + `tsu_gizmo_update` -- the drag state machine: on the press edge
  it picks; while held it slides the picked target along its axis (or free, on a
  view-facing plane) and reports the moved id + new center. callers apply the
  move to their own model.

```c
tsu_gizmo g; tsu_gizmo_init(&g);
/* per frame: */
tsu_ray ray = tsu_ray_from_screen(eye, fwd, right, up, tan_half_fov, aspect,
                                  mouse_x, mouse_y, screen_w, screen_h);
int id; tsu_v3 moved;
if (tsu_gizmo_update(&g, ray, mouse_down, fwd, targets, n, &id, &moved)) {
    /* target `id` moved to `moved` -- write it back to the model */
}
```

## test

```sh
meikyu --test tsumami              # build + run via kaji
meikyu --test tsumami --coverage   # + branch/MC-DC gate (cov=100)
meikyu --mutate tsumami            # mutation testing (operator-swap)
```

100% MC/DC. Mutation survivors are ray-AABB slab-method float-boundary mutants
(`<` vs `<=` at exact ray-grazing intersections) -- measure-zero cases no
deterministic test can hit (equivalent mutants).
