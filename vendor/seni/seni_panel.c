#include "seni_panel.h"
#include "engine/ui/ui.h"

#include <stdio.h>
#include <string.h>

/* widget text/id pointers must stay valid until ui_frame_end; everything
   the panel shows is copied into static storage first. rebuilt every frame
   (immediate mode). */
#define SENI_PANEL_MAX_LINES 64
#define SENI_PANEL_LINE_MAX  192

static char seni_panel_line_store[SENI_PANEL_MAX_LINES][SENI_PANEL_LINE_MAX];
static char seni_panel_id_store[SENI_STATUS_MAX_QUESTIONS][3][32];

static void seni_panel_draw(void *user) {
    SeniReloadStatus *st = (SeniReloadStatus *)user;
    u32 q;
    u32 li = 0;
    if (!st || st->question_count == 0) {
        return; /* nothing pending: no panel */
    }
    ui_panel_begin("seni_panel", 620.0f);
    ui_label("SENI - reload paused", 18);
    ui_label_dim("layout change is ambiguous; pick an answer (it edits the state header)", 14);
    for (q = 0; q < st->question_count; q++) {
        SeniQuestion *sq = &st->questions[q];
        const char *p = sq->message;
        const char *e;
        size_t n;
        /* first message line only -- the annotate suggestions the message
           carries are exactly what the buttons below will write */
        e = p;
        while (*e != '\0' && *e != '\n') e++;
        n = (size_t)(e - p);
        if (n >= SENI_PANEL_LINE_MAX) n = SENI_PANEL_LINE_MAX - 1;
        if (li < SENI_PANEL_MAX_LINES) {
            memcpy(seni_panel_line_store[li], p, n);
            seni_panel_line_store[li][n] = '\0';
            ui_label(seni_panel_line_store[li], 15);
            li++;
        }
        if (sq->answer == SENI_ANSWER_NONE) {
            sprintf(seni_panel_id_store[q][0], "seni_q%lu_row", (unsigned long)q);
            sprintf(seni_panel_id_store[q][1], "seni_q%lu_ren", (unsigned long)q);
            sprintf(seni_panel_id_store[q][2], "seni_q%lu_drp", (unsigned long)q);
            ui_row_begin(seni_panel_id_store[q][0]);
            if (ui_button(seni_panel_id_store[q][1], "rename")) {
                sq->answer = SENI_ANSWER_RENAME;
            }
            ui_label_dim("data moves", 13);
            if (ui_button(seni_panel_id_store[q][2], "drop")) {
                sq->answer = SENI_ANSWER_DROPPED;
            }
            ui_label_dim("data dies", 13);
            ui_row_end();
        } else {
            ui_label_dim("answered -- header annotated, rebuilding...", 14);
        }
    }
    ui_panel_end();
}

b32 seni_panel_register(SeniReloadStatus *status) {
    return ui_panel_register("seni", seni_panel_draw, status);
}
