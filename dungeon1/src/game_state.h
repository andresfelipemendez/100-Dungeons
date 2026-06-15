/* Hot game state: survives dll reloads, migrated by seni when this layout
   changes. seni constraints: scalars (int/float/char/double), fixed-size arrays
   of them, and nested structs defined earlier in the layout (the editor's
   EditorState, prepended to this file's snapshot -- see the project marker's
   `layout_include` key).

   When a reload pauses with questions (the seni panel lists them), answer here:
   `int new_name SENI_WAS(old_name);` for a rename,
   `SENI_DROPPED(old_name)` (no semicolon) for a deliberate removal. */

#include "seni_annotations.h"
#include "henshu_state.h"   /* EditorState -- the reusable CSG editor's hot state */
#include "mono_state.h"     /* mono_world -- the entity table */

typedef struct {
    int initialized;

    float cam_target_x, cam_target_y, cam_target_z;
    float cam_radius;
    float cam_angle;
    float cam_pitch;

    float clear_r, clear_g, clear_b;

    float spin_rate SENI_DEFAULT(1.8f);

    /* the previous rotating barrel: its own spin angle, parked beside the CSG */
    float barrel_angle SENI_DEFAULT(0.0f);

    /* the CSG scene editor's hot state, owned by the henshu lib and migrated as a
       nested struct. Everything editor-related lives here now. */
    EditorState editor;

    /* the entity table, owned by the mono lib -- migrated as a nested struct
       (its Entity ents[] is an array-of-structs). */
    mono_world world;

    /* the entity outliner's current pick: a mono_id (0 = none) stored as int,
       since seni parses int but not the unsigned mono_id typedef. */
    int entity_sel;
} game_state;
