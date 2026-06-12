#ifndef KAJI_STR_H
#define KAJI_STR_H

/* Pointer+size strings: a view into memory someone else owns. No hidden
   NUL terminators, no strlen scans, slicing without mutation -- strtok and
   friends stay out of the codebase. Views never allocate; converting to a
   C string happens only at OS boundaries via kstr_copy into a fixed
   buffer. Header-only; include freely. */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct {
    const char *ptr;
    size_t      len;
} kstr;

#define KSTR(lit) ((kstr){ (lit), sizeof(lit) - 1 })

static kstr kstr_from(const char *c) {
    kstr s;
    s.ptr = c;
    s.len = c ? strlen(c) : 0;
    return s;
}

static int kstr_is_empty(kstr s) {
    return s.len == 0;
}

static int kstr_eq(kstr a, kstr b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.ptr, b.ptr, a.len) == 0);
}

static int kstr_eq_c(kstr a, const char *b) {
    return kstr_eq(a, kstr_from(b));
}

static int kstr_starts_with(kstr s, kstr prefix) {
    return s.len >= prefix.len && memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

static int kstr_ends_with(kstr s, kstr suffix) {
    return s.len >= suffix.len &&
           memcmp(s.ptr + s.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

static kstr kstr_slice(kstr s, size_t from, size_t to) {
    kstr r;
    if (from > s.len) from = s.len;
    if (to > s.len) to = s.len;
    if (to < from) to = from;
    r.ptr = s.ptr + from;
    r.len = to - from;
    return r;
}

static int kstr_find(kstr s, char c) {
    for (size_t i = 0; i < s.len; i++) {
        if (s.ptr[i] == c) {
            return (int)i;
        }
    }
    return -1;
}

static int kstr_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static kstr kstr_trim(kstr s) {
    while (s.len && kstr_is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len && kstr_is_space(s.ptr[s.len - 1])) {
        s.len--;
    }
    return s;
}

/* Iterators: pull the next line/token off the front of *rest. Empty result
   with empty *rest means done. */
static kstr kstr_next_line(kstr *rest) {
    kstr line;
    int nl = kstr_find(*rest, '\n');
    if (nl < 0) {
        line = *rest;
        rest->ptr += rest->len;
        rest->len = 0;
    } else {
        line = kstr_slice(*rest, 0, (size_t)nl);
        *rest = kstr_slice(*rest, (size_t)nl + 1, rest->len);
    }
    if (line.len && line.ptr[line.len - 1] == '\r') {
        line.len--;
    }
    return line;
}

static kstr kstr_next_token(kstr *rest) {
    kstr s = *rest;
    while (s.len && kstr_is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    size_t n = 0;
    while (n < s.len && !kstr_is_space(s.ptr[n])) {
        n++;
    }
    kstr tok = kstr_slice(s, 0, n);
    *rest = kstr_slice(s, n, s.len);
    return tok;
}

/* OS boundary: NUL-terminated copy into a fixed buffer. Returns 0 when the
   view does not fit (dst still terminated). */
static int kstr_copy(char *dst, size_t cap, kstr s) {
    if (cap == 0) {
        return 0;
    }
    size_t n = s.len < cap - 1 ? s.len : cap - 1;
    memcpy(dst, s.ptr, n);
    dst[n] = 0;
    return n == s.len;
}

/* Bounded builder over a caller-owned buffer; overflow is sticky and
   checked once at the end instead of at every append. */
typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
    int    overflow;
} kstr_buf;

static void kstr_buf_init(kstr_buf *b, char *storage, size_t cap) {
    b->buf = storage;
    b->cap = cap;
    b->len = 0;
    b->overflow = 0;
    if (cap) {
        storage[0] = 0;
    }
}

static void kstr_buf_append(kstr_buf *b, kstr s) {
    if (b->overflow || b->len + s.len + 1 > b->cap) {
        b->overflow = 1;
        return;
    }
    memcpy(b->buf + b->len, s.ptr, s.len);
    b->len += s.len;
    b->buf[b->len] = 0;
}

static void kstr_buf_append_c(kstr_buf *b, const char *c) {
    kstr_buf_append(b, kstr_from(c));
}

static void kstr_buf_appendf(kstr_buf *b, const char *fmt, ...) {
    if (b->overflow) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, args);
    va_end(args);
    if (n < 0 || b->len + (size_t)n + 1 > b->cap) {
        b->overflow = 1;
        if (b->cap) {
            b->buf[b->len] = 0;
        }
        return;
    }
    b->len += (size_t)n;
}

static kstr kstr_buf_view(const kstr_buf *b) {
    kstr s;
    s.ptr = b->buf;
    s.len = b->len;
    return s;
}

#endif /* KAJI_STR_H */
