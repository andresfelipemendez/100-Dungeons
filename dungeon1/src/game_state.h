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
       A CSG tree as parallel scalar arrays (seni allows only scalars +
       fixed arrays). Each slot is either a box leaf or a boolean node.
       csg_kind: 0 = box, 1 = union, 2 = difference, 3 = intersection.
       For a box, x/y/z = center, sx/sy/sz = size. For an op, a/b = child
       slot indices. csg_root is the node rendered; csg_dirty triggers a
       re-mesh; csg_selected is the editor's current pick (-1 = none). */
    int   csg_count;
    int   csg_root;
    int   csg_selected;
    int   csg_dirty;
    int   csg_kind[32];
    float csg_x[32], csg_y[32], csg_z[32];
    float csg_sx[32], csg_sy[32], csg_sz[32];
    int   csg_a[32], csg_b[32];

    /* drag state: which box is grabbed (-1 none) + grab offset in world */
    int   drag_node;
    float drag_off_x, drag_off_y, drag_off_z;
} game_state;
