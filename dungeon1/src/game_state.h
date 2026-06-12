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
} game_state;
