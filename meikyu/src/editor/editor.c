#include "editor.h"
#include "engine/ui/ui.h"

#include <stdio.h>

/* panels (single editor translation unit, see editor_unity.c) */
b32  inspector_init(void);
void inspector_draw(PlatformMemory *memory);
void build_panel_draw(PlatformApi *api);

typedef struct {
    f32 fps_smoothed;
} EditorState;

static EditorState ed;

b32 editor_init(PlatformMemory *memory) {
    (void)memory;
    ed.fps_smoothed = 0.0f;
    if (!inspector_init()) {
        /* non-fatal: the editor still runs, the inspector says why */
    }
    return 1;
}

void editor_frame(PlatformMemory *memory, PlatformApi *api, GameInput *input,
                  f32 screen_w, f32 screen_h) {
    (void)memory;
    (void)api;
    ui_frame_begin(screen_w, screen_h, input->mouse_x, input->mouse_y,
                   input->mouse_left);
    /* No engine chrome panel: the game registers its own scene + inspector
       panels and ui_frame_end draws them. */
    ui_frame_end(input->dt);
}
