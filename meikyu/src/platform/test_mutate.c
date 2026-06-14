/* Unit tests for the mutation scanner. Pure, no filesystem. */
#include "../../lib/seni/utest.h"
#include "mutate.h"
#include "mutate.c"
#include <string.h>

UTEST_MAIN()

UTEST(mutate, relational_and_logical) {
    const char *s = "a <= b && c == d";
    /*               0123456789012345   <= @2  && @7  == @12 */
    mutant m[16];
    int n = mutate_scan(s, (int)strlen(s), m, 16);
    ASSERT_EQ(3, n);
    ASSERT_EQ(2, m[0].offset);  ASSERT_EQ(2, m[0].len);  ASSERT_STREQ("<", m[0].repl);
    ASSERT_EQ(7, m[1].offset);  ASSERT_EQ(2, m[1].len);  ASSERT_STREQ("||", m[1].repl);
    ASSERT_EQ(12, m[2].offset); ASSERT_EQ(2, m[2].len);  ASSERT_STREQ("!=", m[2].repl);
}

UTEST(mutate, single_char_relational) {
    const char *s = "x < y > z"; /* < @2 -> <= ; > @6 -> >= */
    mutant m[16];
    int n = mutate_scan(s, (int)strlen(s), m, 16);
    ASSERT_EQ(2, n);
    ASSERT_EQ(1, m[0].len); ASSERT_STREQ("<=", m[0].repl);
    ASSERT_EQ(1, m[1].len); ASSERT_STREQ(">=", m[1].repl);
}

UTEST(mutate, not_equal_and_or) {
    const char *s = "p != q || r"; /* != @2 -> == ; || @7 -> && */
    mutant m[16];
    int n = mutate_scan(s, (int)strlen(s), m, 16);
    ASSERT_EQ(2, n);
    ASSERT_STREQ("==", m[0].repl);
    ASSERT_STREQ("&&", m[1].repl);
}

UTEST(mutate, skips_shifts_strings_chars_comments) {
    const char *s =
        "a << 2;\n"             /* << is a shift, not relational: skip */
        "s = \"<= && ==\";\n"   /* operators inside a string literal: skip */
        "c = '<';\n"            /* operator inside a char literal: skip */
        "/* < && */\n"          /* block comment: skip */
        "x == y; // >= != \n"   /* line comment after a real op */
        "z < w\n";              /* a real op */
    mutant m[32];
    int n = mutate_scan(s, (int)strlen(s), m, 32);
    /* only the real == and the real < survive */
    ASSERT_EQ(2, n);
    ASSERT_STREQ("!=", m[0].repl);
    ASSERT_STREQ("<=", m[1].repl);
}

UTEST(mutate, line_numbers) {
    const char *s = "a < b\nc == d\n";
    mutant m[16];
    int n = mutate_scan(s, (int)strlen(s), m, 16);
    ASSERT_EQ(2, n);
    ASSERT_EQ(1, m[0].line);
    ASSERT_EQ(2, m[1].line);
}
