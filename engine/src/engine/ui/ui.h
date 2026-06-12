#ifndef UI_H
#define UI_H

/* Immediate-mode editor UI. clay does the layout and stb_truetype the text,
   but both are private to ui.c -- this header is strict C89 and exposes only
   plain widget calls, so game code never sees a CLAY macro (clay requires
   C99; ui.c is the one engine file compiled as such, into its own object).

   All UI state, including clay's context, lives in cold memory and is
   rebuilt on every reload -- nothing here may be stored in hot memory.

   Usage per frame (between rnd_frame_begin and rnd_frame_end):
       ui_frame_begin(w, h, mouse_x, mouse_y, mouse_down);
       ui_panel_begin("panel", 240.0f);
           ui_label("EDITOR", 18);
           ui_row_begin("spin_row");
               if (ui_button("minus", "-")) { ... }
           ui_row_end();
       ui_panel_end();
       ui_frame_end(dt);
   Click edges are tracked internally; ui_button returns true once per
   click. ids must be unique within the frame. */

#include "base/base_types.h"

/* Bytes ui_init needs (clay context + font bake scratch). */
u64 ui_memory_required(void);

/* (Re)initializes clay + the font atlas inside `memory` (cold/transient).
   Call after rnd_init on every reload. */
b32 ui_init(void *memory, u64 size, f32 screen_w, f32 screen_h);

void ui_frame_begin(f32 screen_w, f32 screen_h,
                    f32 mouse_x, f32 mouse_y, b32 mouse_down);
void ui_frame_end(f32 dt);

/* Widgets. Text pointers must stay valid until ui_frame_end (frame-static
   buffers are fine for formatted values). */
void ui_panel_begin(const char *id, f32 width);
void ui_panel_end(void);
void ui_row_begin(const char *id);
void ui_row_end(void);
void ui_label(const char *text, int font_size);
void ui_label_dim(const char *text, int font_size); /* secondary color */
b32  ui_button(const char *id, const char *label);

/* --- extension hook ------------------------------------------------------
   Libraries (e.g. seni) contribute panels without the game drawing them:
   register a callback and ui_frame_end invokes it inside the layout scope,
   as a root-level element laid out after the game's own UI. Callbacks use
   the widget API above. The registry is wiped by ui_init (callbacks point
   into the dll image that registered them), so extensions re-register on
   every reload, from the game's cold-rebuild path right after ui_init.
   Registering an existing name replaces its callback. */
typedef void (*UiPanelFn)(void *user);
b32 ui_panel_register(const char *name, UiPanelFn fn, void *user);

#endif /* UI_H */
