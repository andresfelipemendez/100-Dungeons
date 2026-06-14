/* michi test suite -- assert-based, no framework (kansi/kaji/ito convention) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "michi.h"

static int eq(michi m, const char *c) { return ito_eq_c(m.s, c); }

static void test_view_and_convert(void) {
    michi p = PATH("src/game.c");
    assert(p.s.len == 10);
    assert(ito_eq_c(michi_str(p), "src/game.c"));
    assert(eq(michi_from_cstr("a/b"), "a/b"));
}

static void test_join(void) {
    michi_buf b;
    michi_set(&b, PATH("build-mac"));
    michi_join(&b, PATH("gen"));
    assert(eq(michi_view(&b), "build-mac/gen"));          /* one sep inserted */

    michi_set(&b, PATH("build-mac/"));
    michi_join(&b, PATH("gen"));
    assert(eq(michi_view(&b), "build-mac/gen"));          /* no double sep */

    michi_set(&b, PATH("build-mac"));
    michi_join(&b, PATH("/gen"));
    assert(eq(michi_view(&b), "build-mac/gen"));          /* component had sep */

    michi_buf_reset(&b);
    michi_join(&b, PATH("first"));
    assert(eq(michi_view(&b), "first"));                  /* empty: no leading sep */
}

static void test_joinf(void) {
    michi_buf b;
    michi_set(&b, PATH("build-mac"));
    michi_joinf(&b, "game_loaded_%lu.so", (unsigned long)42);
    assert(eq(michi_view(&b), "build-mac/game_loaded_42.so"));
    assert(strcmp(michi_cstr(&b), "build-mac/game_loaded_42.so") == 0);

    /* joinf onto an EMPTY buffer: cur.len > 0 is false -> no leading sep
       (covers that condition independently for MC/DC) */
    michi_buf_reset(&b);
    michi_joinf(&b, "f%d", 1);
    assert(eq(michi_view(&b), "f1"));

    /* joinf where the buffer already ends in '/': second condition false */
    michi_set(&b, PATH("dir/"));
    michi_joinf(&b, "f%d", 2);
    assert(eq(michi_view(&b), "dir/f2"));
}

/* MC/DC for michi_join's inner (component.len > 0 && component.ptr[0]=='/'):
   an EMPTY component flips the length condition -> the sep still goes in. */
static void test_join_empty_component(void) {
    michi_buf b;
    michi_set(&b, PATH("dir"));
    michi_join(&b, michi_from_cstr(""));
    assert(eq(michi_view(&b), "dir/"));
}

static void test_decompose(void) {
    assert(eq(michi_dir(PATH("a/b/c.txt")), "a/b"));
    assert(eq(michi_dir(PATH("c.txt")), "."));            /* no sep */
    assert(eq(michi_dir(PATH("/abs")), "/"));             /* root */
    assert(eq(michi_base(PATH("a/b/c.txt")), "c.txt"));
    assert(eq(michi_base(PATH("bare")), "bare"));
    assert(eq(michi_base(PATH("/abs")), "abs"));         /* sep at index 0 */
    assert(eq(michi_base(PATH("dir\\win.c")), "win.c"));  /* backslash sep */
    assert(eq(michi_ext(PATH("model.vert")), ".vert"));
    assert(eq(michi_ext(PATH("a/b/x.tar.gz")), ".gz"));   /* last dot */
    assert(michi_ext(PATH("noext")).s.len == 0);
    assert(michi_ext(PATH(".bashrc")).s.len == 0);        /* hidden, no ext */
}

static void test_normalize(void) {
    michi_buf b;
    michi_set(&b, PATH("C:\\Users\\dev\\src"));
    michi_normalize(&b);
    assert(eq(michi_view(&b), "C:/Users/dev/src"));
}

static void test_overflow(void) {
    michi_buf b;
    michi_buf_reset(&b);
    char big[MICHI_MAX];
    memset(big, 'x', sizeof(big));
    michi long_one = michi_from_ito((ito){ big, sizeof(big) });
    michi_join(&b, long_one);          /* MICHI_MAX bytes + NUL won't fit */
    assert(b.b.overflow);              /* sticky overflow, not silent */
}

int main(void) {
    test_view_and_convert();
    test_join();
    test_joinf();
    test_join_empty_component();
    test_decompose();
    test_normalize();
    test_overflow();
    printf("michi: all tests passed\n");
    return 0;
}
