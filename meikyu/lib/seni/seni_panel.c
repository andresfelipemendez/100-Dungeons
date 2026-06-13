#include "seni_panel.h"
#include "engine/ui/ui.h"

/* widget id views must stay valid until ui_frame_end; the ids are built
   into static storage below. Message text is shown as a view straight into
   the (persistent) status struct -- no copy needed. */
static char seni_panel_id_store[SENI_STATUS_MAX_QUESTIONS][3][32];

static void seni_panel_draw(void *user) {
    SeniReloadStatus *st = (SeniReloadStatus *)user;
    u32 q;
    if (!st || st->question_count == 0) {
        return; /* nothing pending: no panel */
    }
    ui_panel_begin(ITO("seni_panel"), 620.0f);
    ui_label(ITO("SENI - reload paused"), 18);
    ui_label_dim(ITO("layout change is ambiguous; pick an answer "
                     "(it edits the state header)"), 14);
    for (q = 0; q < st->question_count; q++) {
        SeniQuestion *sq = &st->questions[q];
        /* first message line only -- the annotate suggestions the message
           carries are exactly what the buttons below will write */
        ito msg = ito_from(sq->message);
        ui_label(ito_next_line(&msg), 15);

        if (sq->answer == SENI_ANSWER_NONE) {
            ito_buf row, ren, drp;
            ito_buf_init(&row, seni_panel_id_store[q][0], 32);
            ito_buf_init(&ren, seni_panel_id_store[q][1], 32);
            ito_buf_init(&drp, seni_panel_id_store[q][2], 32);
            ito_buf_appendf(&row, "seni_q%u_row", q);
            ito_buf_appendf(&ren, "seni_q%u_ren", q);
            ito_buf_appendf(&drp, "seni_q%u_drp", q);
            ui_row_begin(ito_buf_view(&row));
            if (ui_button(ito_buf_view(&ren), ITO("rename"))) {
                sq->answer = SENI_ANSWER_RENAME;
            }
            ui_label_dim(ITO("data moves"), 13);
            if (ui_button(ito_buf_view(&drp), ITO("drop"))) {
                sq->answer = SENI_ANSWER_DROPPED;
            }
            ui_label_dim(ITO("data dies"), 13);
            ui_row_end();
        } else {
            ui_label_dim(ITO("answered -- header annotated, rebuilding..."), 14);
        }
    }
    ui_panel_end();
}

b32 seni_panel_register(SeniReloadStatus *status) {
    return ui_panel_register(ITO("seni"), seni_panel_draw, status);
}
