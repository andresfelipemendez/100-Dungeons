# 編集 henshu

the reusable CSG scene editor for the engine: a game nests the editor's hot
state in its `game_state` and wires it in a few calls; the editor owns the shape
model, the inspector/scene panels, the move gizmo, the CSG mesh and the
resizable panel grips. games define only their own in-game UI. sibling library
to the other [lib/](..) modules -- the engine and games consume it, it is not
the engine.

henshu is two parts, split by dependency so the model logic stays testable:

## part 1 -- core (`henshu.c`, testable)

Operations on `EditorState` plus the CSG evaluation that turns the shape list
into geometry. Depends only on the leaf libs [horu](../horu) (CSG) and
[tsumami](../tsumami) (pick targets) -- no render, no UI -- so it builds and is
coverage-gated like them (`lib_test henshu cov=90`, currently **100% MC/DC**).

**Reentrant:** no file statics. `henshu_eval_all`'s working memory (horu's BSP
scratch + the fold's ping-pong + per-shape buffers) is a caller-owned
`henshu_scratch` -- pass a region of `HENSHU_SCRATCH_SIZE` bytes (~7.8 MB; the
game carves it from its transient block). Two editors, or parallel tests, never
collide. A `_Static`-style negative-array check keeps `csg_kind[]`'s literal 32
in lockstep with `HENSHU_MAX_SHAPES`.

The model is a FLAT LIST of shape entities -- no boolean "nodes". Each slot is
one primitive; the rendered solid is the LEFT FOLD of the list:

```
result = shape[0];   for i>0:  result = shape[i].op(result, shape[i])
```

so a difference/intersection shape acts on everything earlier in the list.

- `henshu_default` / `henshu_model_ok` -- starter model; validity check (a model
  carried over from an older format re-inits).
- `henshu_add` / `henshu_set_kind` / `henshu_set_op` -- edit the selection.
- `henshu_eval_shape` / `henshu_eval_all` -- shape -> polygons; fold the list
  (boolean work in a caller-provided scratch >= `HORU_CSG_SCRATCH`).
- `henshu_targets` -- tsumami pick targets: every shape is a draggable body, the
  selection also gets three axis-arrow handles.

## part 2 -- glue (`henshu_ui.c`, game-dll only)

The render/UI half: scene + inspector panels, the move gizmo (pick/drag + arrow
meshes), CSG mesh upload/draw, the grid texture, the resize grips. Sits on the
engine (render + ui + theme), compiled into the game dll via unity include, not
unit-tested. A game wires it in five calls:

```c
henshu_cold_init(&cold, scratch, csg_pipeline, gizmo_pipeline); /* GPU objects */
henshu_register_panels(&gs->editor);                            /* after ui_init */
/* per frame: */
if (gs->editor.csg_dirty) { henshu_rebuild(&gs->editor, &cold); gs->editor.csg_dirty = 0; }
henshu_update(&gs->editor, &cold, &view);   /* resize + viewport pick/drag */
henshu_draw_scene(&gs->editor, &cold, mvp); /* mesh + gizmo */
henshu_draw_overlay(&gs->editor, &view);    /* panel grips, after the UI */
```

Shaders are project assets, so the game supplies the two pipelines (a
triplanar-grid pipeline for the CSG mesh, a plain solid pipeline for the gizmo
arrows -- kept plain so the grid does not bleed onto the gizmo).

## nested hot state (`henshu_state.h`)

`EditorState` is the editor's hot state, **owned by the lib**. A project nests
it: `game_state` holds `EditorState editor;` and [seni](../seni) migrates it as
a nested struct across reloads. For seni's layout diff to resolve the `editor`
field, `EditorState`'s definition must be in the bytes seni reads, so the
project's marker declares

```
layout_include lib/henshu/henshu_state.h
```

which `project_gen` prepends to the `game_state.h` snapshot (kaji's `copy`
concatenates multiple `in`s). Constraint: the struct's arrays use a **literal**
size (`[32]`), never a macro -- seni parses the snapshot as raw text and cannot
expand `HENSHU_MAX_SHAPES`.

## status / known debt

Works; coverage-gated. Scale-gated items, fine for hand-built models (~32
shapes), flagged so they are known rather than surprising:

- **Full rebuild on any dirty.** `henshu_rebuild` re-folds the *whole* shape list
  and destroys+recreates both GPU buffers on every edit -- so dragging one shape
  re-meshes everything each frame. No incremental / dirty-region path; the
  per-frame GPU buffer realloc is avoidable (reuse capacity, re-upload only).
- **Glue mesh-build statics.** `henshu_ui.c` still holds the triangulation
  buffers (`g_polys`/`g_verts`/`g_index`) as file statics -- single editor
  instance for the render half (the testable core is reentrant; the glue is
  not). Could share the one transient scratch region.
- **Eval copies the result** poly-by-poly instead of returning the ping-pong
  buffer.

## test

```sh
meikyu --test henshu              # build + run the core suite via kaji
meikyu --test henshu --coverage   # + branch/MC-DC gate (cov=90; at 100%)
```

The `lib_test henshu` row in `build.manifest` builds `test.c` with `henshu.c`,
`../horu/horu.c`, `../tsumami/tsumami.c` and the `lib/{horu,tsumami,seni}`
include roots. Only the core is tested; the render/UI glue is exercised live in
dungeon1.
