/* ito test suite -- assert-based, no framework (kansi/kaji convention) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ito.h"

static void test_views(void) {
    ito s = ITO("hello world");
    assert(s.len == 11);
    assert(ito_eq_c(ito_slice(s, 6, 11), "world"));
    assert(ito_find(s, 'o') == 4);
    assert(ito_find_last(s, 'o') == 7);
    assert(ito_find_last(s, 'q') == -1);
    assert(ito_starts_with(s, ITO("hell")));
    assert(ito_ends_with(s, ITO("rld")));
    assert(ito_eq(ito_trim(ITO("  x \t")), ITO("x")));
}

static void test_tokens(void) {
    ito rest = ITO("a \"b c\" d");
    assert(ito_eq_c(ito_next_token(&rest), "a"));
    assert(ito_eq_c(ito_next_token(&rest), "b c"));   /* quoted, spaces */
    assert(ito_eq_c(ito_next_token(&rest), "d"));
    assert(ito_is_empty(ito_next_token(&rest)));

    ito lines = ITO("one\r\ntwo\nthree");
    assert(ito_eq_c(ito_next_line(&lines), "one"));   /* CR stripped */
    assert(ito_eq_c(ito_next_line(&lines), "two"));
    assert(ito_eq_c(ito_next_line(&lines), "three"));
}

static void test_buf_formatf(void) {
    char storage[128];
    ito_buf b;
    ito_buf_init(&b, storage, sizeof(storage));
    ito name = ITO("dungeon1");
    ito_buf_appendf(&b, "project '%S': %d files, %.*s, %llu, %5.2f, %x, %%",
                    name, 3, 2, "abcd", 9ull, 1.5, 255u);
    assert(strcmp(ito_buf_cstr(&b),
                  "project 'dungeon1': 3 files, ab, 9,  1.50, ff, %") == 0);
    assert(!b.overflow);

    /* sticky overflow refuses partial output */
    char tiny[8];
    ito_buf t;
    ito_buf_init(&t, tiny, sizeof(tiny));
    ito_buf_appendf(&t, "%S", ITO("way too long for eight"));
    assert(t.overflow);
}

int main(void) {
    test_views();
    test_tokens();
    test_buf_formatf();
    printf("ito: all tests passed\n");
    return 0;
}
