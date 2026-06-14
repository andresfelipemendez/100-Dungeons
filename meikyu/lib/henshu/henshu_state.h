#ifndef HENSHU_STATE_H
#define HENSHU_STATE_H

/* henshu (編集): the reusable CSG scene editor. This header is the ONE place the
   editor's hot state lives -- a project nests it in its game_state as
   `EditorState editor;` and seni migrates it across reloads (seni's nested-struct
   support). It is also prepended to the project's game_state.h snapshot so the
   reloader's layout diff can resolve the `editor` field (see the henshu lib's
   build notes). Keep it seni-parseable: scalars and fixed arrays only -- no
   pointers, no further nesting. */

#include "seni_annotations.h"

/* a flat list of shape entities; the solid is the LEFT FOLD of the list (see
   henshu.h). 32 shapes is plenty for a hand-built model and keeps the migrated
   block small. The arrays below MUST use the literal 32, not this macro: seni
   parses this header as raw text and cannot expand a macro in an array size. */
#define HENSHU_MAX_SHAPES 32

typedef struct {
    /* resizable editor panels: current widths (px) + which edge is being dragged
       (0 none, 1 scene, 2 inspector). prev_mouse_left detects the press edge that
       grabs a splitter. */
    float scene_w        SENI_DEFAULT(220.0f);
    float inspector_w    SENI_DEFAULT(240.0f);
    int   ui_drag        SENI_DEFAULT(0);
    int   prev_mouse_left SENI_DEFAULT(0);

    /* ---- horu CSG model ------------------------------------------------------
       Parallel scalar arrays, one entry per shape. There are no boolean "nodes":
       the solid is shape[0] folded with each later shape under its own op.
       kind: 0 box, 1 sphere, 2 cylinder, 3 polygon. op: how a shape combines with
       the running result -- 0 union, 1 difference, 2 intersection (ignored for
       slot 0, the base). x/y/z = center, sx/sy/sz = size. dirty triggers a
       re-mesh; selected is the editor's current pick (-1 = none). */
    int   csg_count;
    int   csg_selected;
    int   csg_dirty;
    int   csg_kind[32];          /* 32 == HENSHU_MAX_SHAPES (literal for seni) */
    int   csg_op[32];
    float csg_x[32],  csg_y[32],  csg_z[32];
    float csg_sx[32], csg_sy[32], csg_sz[32];

    /* viewport drag: which shape is grabbed (-1 none) + grab offset in world */
    int   drag_node;
    float drag_off_x, drag_off_y, drag_off_z;
} EditorState;

#endif /* HENSHU_STATE_H */
