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
    if (input->dt > 0.0001f) {
        ed.fps_smoothed = ed.fps_smoothed * 0.95f + (1.0f / input->dt) * 0.05f;
    }
    static char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "fps %.0f", ed.fps_smoothed);

    ui_frame_begin(screen_w, screen_h, input->mouse_x, input->mouse_y,
                   input->mouse_left);

    ui_panel_begin("editor_main", 260.0f);
        ui_label("EDITOR", 18);
        ui_label_dim(fps_text, 14);
        inspector_draw(memory);
        build_panel_draw(api);
    ui_panel_end();

    /* registered extension panels (e.g. seni's reload questions) are drawn
       by ui_frame_end inside the layout scope */
    ui_frame_end(input->dt);
}
