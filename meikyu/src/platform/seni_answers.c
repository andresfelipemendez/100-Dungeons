#include "seni_answers.h"
#include <stdio.h>

annotate_result seni_answer_annotate(arena *a, u32 answer,
                                     const char *struct_name,
                                     const char *removed, const char *added,
                                     char *header) {
    if (answer == SENI_ANSWER_RENAME) {
        return annotate_rename(a, header, struct_name, removed, added);
    }
    if (answer == SENI_ANSWER_DROPPED) {
        return annotate_dropped(a, header, struct_name, removed);
    }
    {
        annotate_result r;
        r.code = header;
        r.err = NULL;
        return r;
    }
}

u32 seni_fill_questions(SeniReloadStatus *status,
                        const diff_question *qs, size_t n) {
    u32 stored = 0;
    size_t i;
    for (i = 0; i < n && stored < SENI_STATUS_MAX_QUESTIONS; i++) {
        SeniQuestion *sq = &status->questions[stored++];
        snprintf(sq->message, SENI_STATUS_MSG_MAX, "%s", qs[i].message);
        snprintf(sq->struct_name, SENI_STATUS_NAME_MAX, "%s", qs[i].struct_name);
        snprintf(sq->removed, SENI_STATUS_NAME_MAX, "%s", qs[i].removed);
        snprintf(sq->added, SENI_STATUS_NAME_MAX, "%s",
                 qs[i].added ? qs[i].added : "");
        sq->answer = SENI_ANSWER_NONE;
    }
    status->question_count = stored;
    return stored;
}
