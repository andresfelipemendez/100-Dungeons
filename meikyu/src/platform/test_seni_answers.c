/* Unit tests for the pure cores in seni_answers.c: the rename/drop/no-op
   annotation dispatch and the question-mailbox fill clamp. No window, no
   dodai, no SDL -- just seni + the ABI types. */
#include "../../lib/seni/utest.h"
#include "seni_answers.h"
#include "../../lib/seni/arena.c"
#include "../../lib/seni/seni.c"
#include "seni_answers.c"

UTEST_MAIN()

/* RENAME dispatch -> annotate_rename: the removed field gets a SENI_WAS marker
   so the next diff treats it as the same slot under the new name. */
UTEST(answers, annotate_rename_dispatch) {
    static char buf[1u << 20];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    const char *lit = "typedef struct {\n    float x;\n    int armor;\n} unit;\n";
    char *hdr = arena_copy_string(&a, lit, strlen(lit));

    annotate_result r = seni_answer_annotate(&a, SENI_ANSWER_RENAME,
                                             "unit", "health", "armor", hdr);
    ASSERT_TRUE(r.err == NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_WAS(health)") != NULL);
}

/* DROPPED dispatch -> annotate_dropped: a SENI_DROPPED marker on the removed
   field, no SENI_WAS. */
UTEST(answers, annotate_dropped_dispatch) {
    static char buf[1u << 20];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    const char *lit = "typedef struct {\n    float x;\n    int armor;\n} unit;\n";
    char *hdr = arena_copy_string(&a, lit, strlen(lit));

    annotate_result r = seni_answer_annotate(&a, SENI_ANSWER_DROPPED,
                                             "unit", "health", "", hdr);
    ASSERT_TRUE(r.err == NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_DROPPED(health)") != NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_WAS") == NULL);
}

/* Any non-RENAME/DROPPED answer is a no-op: header returned untouched, no
   error. Guards against acting on NONE/APPLIED/garbage. */
UTEST(answers, annotate_noop_passthrough) {
    static char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char *hdr = arena_copy_string(&a, "hi", 2);

    annotate_result r = seni_answer_annotate(&a, SENI_ANSWER_NONE,
                                             "unit", "health", "", hdr);
    ASSERT_TRUE(r.err == NULL);
    ASSERT_TRUE(r.code == hdr); /* same pointer: untouched */
}

/* fill copies each question into the mailbox, NULL-safe on .added, and stamps
   the answer slot NONE so the UI starts unanswered. */
UTEST(answers, fill_basic) {
    SeniReloadStatus status;
    diff_question qs[2];
    qs[0].struct_name = "unit"; qs[0].removed = "health"; qs[0].added = "armor";
    qs[0].message = "rename health -> armor?"; qs[0].type = 0;
    qs[1].struct_name = "world"; qs[1].removed = "seed"; qs[1].added = NULL;
    qs[1].message = "drop seed?"; qs[1].type = 0;

    u32 n = seni_fill_questions(&status, qs, 2);
    ASSERT_EQ((u32)2, n);
    ASSERT_EQ((u32)2, status.question_count);
    ASSERT_STREQ("armor", status.questions[0].added);
    ASSERT_STREQ("", status.questions[1].added); /* NULL -> "" */
    ASSERT_EQ((u32)SENI_ANSWER_NONE, status.questions[0].answer);
}

/* The clamp: more questions than the mailbox holds -> store exactly
   SENI_STATUS_MAX_QUESTIONS, report that count (the documented truncation). */
UTEST(answers, fill_clamps_at_cap) {
    SeniReloadStatus status;
    size_t over = SENI_STATUS_MAX_QUESTIONS + 5;
    static diff_question qs[SENI_STATUS_MAX_QUESTIONS + 5];
    size_t i;
    for (i = 0; i < over; i++) {
        qs[i].struct_name = "s"; qs[i].removed = "f"; qs[i].added = NULL;
        qs[i].message = "q"; qs[i].type = 0;
    }
    u32 n = seni_fill_questions(&status, qs, over);
    ASSERT_EQ((u32)SENI_STATUS_MAX_QUESTIONS, n);
    ASSERT_EQ((u32)SENI_STATUS_MAX_QUESTIONS, status.question_count);
}
