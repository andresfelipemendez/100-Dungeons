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

static void test_basics(void) {
    assert(ito_from(NULL).len == 0);                 /* NULL -> empty */
    assert(ito_from("abc").len == 3);
    assert(ito_is_empty(ITO("")));
    assert(!ito_is_empty(ITO("x")));
    /* ito_eq MC/DC: length mismatch, both empty, equal, same-len-differ */
    assert(!ito_eq(ITO("ab"), ITO("abc")));          /* len differs */
    assert(ito_eq(ITO(""), ITO("")));                /* both empty (short-circuit) */
    assert(ito_eq(ITO("ab"), ITO("ab")));            /* equal non-empty */
    assert(!ito_eq(ITO("ab"), ITO("ac")));           /* same len, differ */
    assert(ito_eq_c(ITO("hi"), "hi"));
    assert(!ito_eq_c(ITO("hi"), "ho"));
}

static void test_prefix_suffix_slice(void) {
    ito s = ITO("hello");
    assert(!ito_starts_with(s, ITO("hello!")));      /* prefix longer */
    assert(!ito_starts_with(s, ITO("help")));        /* mismatch */
    assert(ito_starts_with(s, ITO("hel")));
    assert(!ito_ends_with(s, ITO("xhello")));        /* suffix longer */
    assert(!ito_ends_with(s, ITO("llp")));           /* mismatch */
    assert(ito_ends_with(s, ITO("llo")));
    /* slice clamps: from>len, to>len, to<from */
    assert(ito_slice(s, 20, 25).len == 0);           /* from clamps to len */
    assert(ito_eq_c(ito_slice(s, 2, 99), "llo"));    /* to clamps to len */
    assert(ito_slice(s, 4, 1).len == 0);             /* to<from -> empty */
}

static void test_space_trim(void) {
    assert(ito_is_space(' ') && ito_is_space('\t') &&
           ito_is_space('\r') && ito_is_space('\n'));
    assert(!ito_is_space('x'));
    assert(ito_eq_c(ito_trim(ITO("plain")), "plain"));   /* nothing to trim */
    assert(ito_is_empty(ito_trim(ITO("   \t\n"))));        /* all space */
    assert(ito_eq_c(ito_trim(ITO("\t\nlead")), "lead"));
    assert(ito_eq_c(ito_trim(ITO("trail  ")), "trail"));
}

static void test_line_token_edges(void) {
    ito ls = ITO("\nbody");           /* empty first line */
    assert(ito_is_empty(ito_next_line(&ls)));
    assert(ito_eq_c(ito_next_line(&ls), "body"));   /* last, no newline (nl<0) */
    assert(ito_is_empty(ls));

    ito q = ITO("  \"unterminated");  /* leading spaces + missing close quote */
    assert(ito_eq_c(ito_next_token(&q), "unterminated"));
    assert(ito_is_empty(ito_next_token(&q)));        /* only-space rest -> empty */
}

static void test_copy(void) {
    char dst[8];
    assert(ito_copy(dst, sizeof(dst), ITO("fits")) == 1 &&
           strcmp(dst, "fits") == 0);
    assert(ito_copy(dst, sizeof(dst), ITO("way too long")) == 0 &&
           strcmp(dst, "way too") == 0);             /* truncated, terminated */
    assert(ito_copy(dst, 0, ITO("x")) == 0);         /* cap 0 */
}

static void test_buf_edges(void) {
    char z[1];
    ito_buf zb;
    ito_buf_init(&zb, z, 0);                          /* cap 0: no storage[0]=0 */
    assert(zb.cap == 0 && zb.len == 0);

    char s[8];
    ito_buf b;
    ito_buf_init(&b, s, sizeof(s));
    ito_buf_append_c(&b, "abc");
    assert(ito_eq_c(ito_buf_view(&b), "abc"));
    assert(strcmp(ito_buf_cstr(&b), "abc") == 0);
    ito_buf_append(&b, ITO("defg"));                  /* 3+4+1=8 == cap: fits */
    assert(!b.overflow && ito_eq_c(ito_buf_view(&b), "abcdefg"));
    ito_buf_append(&b, ITO("!"));                     /* now overflows */
    assert(b.overflow);
    ito_buf_append(&b, ITO("x"));                     /* sticky: stays overflow */
    assert(b.overflow && ito_eq_c(ito_buf_view(&b), "abcdefg"));
}

static void test_format_conversions(void) {
    char s[256];
    /* signed/unsigned + length modifiers l, ll, z, h */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%d %ld %lld %zd", 1, 2L, 3LL,
                               (size_t)4), "1 2 3 4"));
    assert(ito_eq_c(ito_format(s, sizeof(s), "%u %lu %llu %zu", 1u, 2ul, 3ull,
                               (size_t)4), "1 2 3 4"));
    assert(ito_eq_c(ito_format(s, sizeof(s), "%x %X %o", 255u, 255u, 8u),
                    "ff FF 10"));
    assert(ito_eq_c(ito_format(s, sizeof(s), "%hd", 5), "5"));   /* h modifier */
    /* floats (every conversion letter incl. the upper-case case labels) */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%.1f %.0e %g", 1.5, 100.0, 2.0),
                    "1.5 1e+02 2"));
    assert(ito_eq_c(ito_format(s, sizeof(s), "%i %.1F %.0E %G", 7, 1.5, 100.0,
                               2.0), "7 1.5 1E+02 2"));
    /* char, string, pointer (just non-empty), flags + width + precision */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%c[%-4s][%04d]", 'Q', "hi", 7),
                    "Q[hi  ][0007]"));
    /* '*' width and '*' precision resolved from args */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%*d %.*f", 4, 7, 2, 3.14159),
                    "   7 3.14"));
    /* %S with flags reaches the switch (not the fast path) */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%-5S|", ITO("ab")), "ab|"));
    /* unknown conversion: emitted literally, no arg consumed */
    assert(ito_eq_c(ito_format(s, sizeof(s), "a%qb"), "a%qb"));
    /* remaining flags (+, space, #) for the flag-chain MC/DC, and %p */
    assert(ito_eq_c(ito_format(s, sizeof(s), "%+d % d %#x", 5, 5, 255u),
                    "+5  5 0xff"));
    {
        int x;
        ito p = ito_format(s, sizeof(s), "%p", (void *)&x);
        assert(p.len > 0);                            /* pointer prints something */
    }
    /* a trailing '%' with no conversion: conv resolves to 0, emitted literally */
    assert(ito_eq_c(ito_format(s, sizeof(s), "end%"), "end%"));
}

/* exercise the per-spec truncation guards (si < sizeof(spec)-8) for the FLAG
   and LENGTH-modifier loops -- a malicious format must not overflow the 48-byte
   rebuild buffer. NOTE: the width-digit and precision-digit guards are NOT
   tested here: covering them needs a 40-digit width/precision, i.e. an
   astronomically large number that makes snprintf attempt to emit billions of
   pad chars (effectively a hang). They stay as defensive, uncovered code. */
static void test_format_long_specs(void) {
    char s[256];
    /* 41 '0' flags (value stays small) -> flag-loop guard */
    assert(ito_format(s, sizeof(s),
        "%00000000000000000000000000000000000000000d", 7).len > 0);
    /* 40 length 'l' modifiers -> length-loop guard (extra l's ignored) */
    assert(ito_format(s, sizeof(s),
        "%lllllllllllllllllllllllllllllllllllllllld", 7LL).len > 0);
}

static void test_format_overflow(void) {
    char s[8];
    ito_buf b;
    ito_buf_init(&b, s, sizeof(s));
    ito_buf_appendf(&b, "%d", 1234567890);            /* number won't fit */
    assert(b.overflow);

    /* an early conversion overflows, leaving more format text: the loop top
       must see the sticky overflow and stop (the !overflow guard's false arm) */
    char t[4];
    ito_buf c;
    ito_buf_init(&c, t, sizeof(t));
    ito_buf_appendf(&c, "%S then more", ITO("toolong"));
    assert(c.overflow);
}

int main(void) {
    test_views();
    test_tokens();
    test_buf_formatf();
    test_basics();
    test_prefix_suffix_slice();
    test_space_trim();
    test_line_token_edges();
    test_copy();
    test_buf_edges();
    test_format_conversions();
    test_format_long_specs();
    test_format_overflow();
    printf("ito: all tests passed\n");
    return 0;
}
