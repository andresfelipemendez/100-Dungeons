/* Hot game state: survives dll reloads, migrated by seni when this layout
   changes. seni constraints: scalars (int/float/char/double) and fixed-size
   arrays of them ONLY. No pointers, no nested structs, no typedef'd types.

   When a reload pauses with questions (the seni panel lists them), answer
   here: `int new_name SENI_WAS(old_name);` for a rename,
   `SENI_DROPPED(old_name)` (no semicolon) for a deliberate removal. */

#include "seni_annotations.h"

typedef struct {
    int initialized;

    float cam_target_x, cam_target_y, cam_target_z;
    float cam_radius;
    float cam_angle;
    float cam_pitch;

    float clear_r, clear_g, clear_b;

    float spin_rate SENI_DEFAULT(1.8f);

    /* ---- horu CSG editor model -----------------------------------------
       A FLAT LIST of shape entities as parallel scalar arrays (seni allows
       only scalars + fixed arrays). There are no boolean "nodes": the solid
       is the LEFT FOLD of the list -- result = shape[0], then each later
       shape folds in with its own op. csg_kind: 0 box, 1 sphere, 2 cylinder,
       3 polygon. csg_op: how a shape combines with the running result --
       0 union, 1 difference, 2 intersection (ignored for slot 0, the base).
       x/y/z = center, sx/sy/sz = size. csg_dirty triggers a re-mesh;
       csg_selected is the editor's current pick (-1 = none). */
    int   csg_count;
    int   csg_selected;
    int   csg_dirty;
    int   csg_kind[32];
    int   csg_op[32];
    float csg_x[32], csg_y[32], csg_z[32];
    float csg_sx[32], csg_sy[32], csg_sz[32];
    /* the old op-node tree is gone (flat shape list now); these were dropped,
       not renamed -- csg_op is a new field, zero-initialised on migration. */
    SENI_DROPPED(csg_root)
    SENI_DROPPED(csg_a)
    SENI_DROPPED(csg_b)

    /* drag state: which box is grabbed (-1 none) + grab offset in world */
    int   drag_node;
    float drag_off_x, drag_off_y, drag_off_z;
} game_state;
