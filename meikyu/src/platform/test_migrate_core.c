/* Unit tests for migrate_core: routing + the size guard, no gcc/dlopen. */
#include "../../lib/seni/utest.h"
#include "migrate_core.h"
#include "../../lib/seni/arena.c"
#include "../../lib/seni/seni.c"
#include "migrate_core.c"

UTEST_MAIN()

/* a diff_result with N questions and the given struct_count, fields zeroed */
static diff_result mk(size_t questions, size_t struct_count) {
    diff_result dr;
    memset(&dr, 0, sizeof(dr));
    dr.err = NULL;
    dr.question_count = questions;
    dr.value.struct_count = struct_count;
    return dr;
}

UTEST(decide, clean_diff_proceeds) {
    diff_result dr = mk(0, 1);
    ASSERT_EQ((int)MIG_PROCEED, (int)migrate_decide(&dr, SENI_OVERRIDE_NONE));
}

UTEST(decide, textual_only_is_noop) {
    diff_result dr = mk(0, 0);
    ASSERT_EQ((int)MIG_NOOP, (int)migrate_decide(&dr, SENI_OVERRIDE_NONE));
}

UTEST(decide, questions_no_override_refuses) {
    diff_result dr = mk(3, 1);
    ASSERT_EQ((int)MIG_REFUSE_QUESTIONS, (int)migrate_decide(&dr, SENI_OVERRIDE_NONE));
}

UTEST(decide, questions_cold_override) {
    diff_result dr = mk(3, 1);
    ASSERT_EQ((int)MIG_RELOAD_COLD, (int)migrate_decide(&dr, SENI_OVERRIDE_RELOAD_COLD));
}

UTEST(decide, questions_drop_override) {
    diff_result dr = mk(3, 1);
    ASSERT_EQ((int)MIG_DROP_ALL, (int)migrate_decide(&dr, SENI_OVERRIDE_DROP_ALL));
}

UTEST(decide, override_ignored_without_questions) {
    diff_result dr = mk(0, 1); /* clean: override must not hijack a normal reload */
    ASSERT_EQ((int)MIG_PROCEED, (int)migrate_decide(&dr, SENI_OVERRIDE_RELOAD_COLD));
}

UTEST(size, zero_rejected) {
    ASSERT_FALSE(migrate_size_fits(0, 1024, 1024));
}
UTEST(size, fits) {
    ASSERT_TRUE(migrate_size_fits(512, 1024, 1024));
}
UTEST(size, too_big_for_hot) {
    ASSERT_FALSE(migrate_size_fits(2048, 1024, 4096));
}
UTEST(size, too_big_for_scratch) {
    ASSERT_FALSE(migrate_size_fits(2048, 4096, 1024));
}
