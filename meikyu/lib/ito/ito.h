#ifndef ITO_H
#define ITO_H

/* ito (糸): pointer+size strings -- a view into memory someone else owns.
   No hidden NUL terminators, no strlen scans, slicing without mutation --
   strtok and friends stay out of the codebase. Views never allocate;
   converting to a C string happens only at OS boundaries via ito_copy /
   ito_buf_cstr. Header-only; include freely.

   The first-party string library for the whole monorepo (engine platform,
   dodai, kansi, kaji, seni). Boundary rules live in the spec: ABI structs
   and hot memory stay flat char[] (ito carries a pointer); printf format
   literals stay const char*. */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct {
    const char *ptr;
    size_t      len;
} ito;

#define ITO(lit) ((ito){ (lit), sizeof(lit) - 1 })

/* printf an ito through any printf-family call (keeps -Wformat checking):
       dodai_log("open " ITO_FMT, ITO_ARG(path));                        */
#define ITO_FMT    "%.*s"
#define ITO_ARG(s) (int)(s).len, (s).ptr

static ito ito_from(const char *c) {
    ito s;
    s.ptr = c;
    s.len = c ? strlen(c) : 0;
    return s;
}

static int ito_is_empty(ito s) {
    return s.len == 0;
}

static int ito_eq(ito a, ito b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.ptr, b.ptr, a.len) == 0);
}

static int ito_eq_c(ito a, const char *b) {
    return ito_eq(a, ito_from(b));
}

static int ito_starts_with(ito s, ito prefix) {
    return s.len >= prefix.len && memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

static int ito_ends_with(ito s, ito suffix) {
    return s.len >= suffix.len &&
           memcmp(s.ptr + s.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

static ito ito_slice(ito s, size_t from, size_t to) {
    ito r;
    if (from > s.len) from = s.len;
    if (to > s.len) to = s.len;
    if (to < from) to = from;
    r.ptr = s.ptr + from;
    r.len = to - from;
    return r;
}

static int ito_find(ito s, char c) {
    for (size_t i = 0; i < s.len; i++) {
        if (s.ptr[i] == c) {
            return (int)i;
        }
    }
    return -1;
}

/* last occurrence (strrchr): -1 when absent */
static int ito_find_last(ito s, char c) {
    for (size_t i = s.len; i > 0; i--) {
        if (s.ptr[i - 1] == c) {
            return (int)(i - 1);
        }
    }
    return -1;
}

static int ito_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static ito ito_trim(ito s) {
    while (s.len && ito_is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len && ito_is_space(s.ptr[s.len - 1])) {
        s.len--;
    }
    return s;
}

/* Iterators: pull the next line/token off the front of *rest. Empty result
   with empty *rest means done. */
static ito ito_next_line(ito *rest) {
    ito line;
    int nl = ito_find(*rest, '\n');
    if (nl < 0) {
        line = *rest;
        rest->ptr += rest->len;
        rest->len = 0;
    } else {
        line = ito_slice(*rest, 0, (size_t)nl);
        *rest = ito_slice(*rest, (size_t)nl + 1, rest->len);
    }
    if (line.len && line.ptr[line.len - 1] == '\r') {
        line.len--;
    }
    return line;
}

static ito ito_next_token(ito *rest) {
    ito s = *rest;
    while (s.len && ito_is_space(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    /* "double quoted" token: value may hold spaces (generated configs quote
       absolute paths -- clone dirs are user-controlled). No escapes; a
       missing closing quote just runs to end of input. */
    if (s.len && s.ptr[0] == '"') {
        s.ptr++;
        s.len--;
        size_t q = 0;
        while (q < s.len && s.ptr[q] != '"') {
            q++;
        }
        ito tok = ito_slice(s, 0, q);
        *rest = ito_slice(s, q < s.len ? q + 1 : q, s.len);
        return tok;
    }
    size_t n = 0;
    while (n < s.len && !ito_is_space(s.ptr[n])) {
        n++;
    }
    ito tok = ito_slice(s, 0, n);
    *rest = ito_slice(s, n, s.len);
    return tok;
}

/* OS boundary: NUL-terminated copy into a fixed buffer. Returns 0 when the
   view does not fit (dst still terminated). */
static int ito_copy(char *dst, size_t cap, ito s) {
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
} ito_buf;

static void ito_buf_init(ito_buf *b, char *storage, size_t cap) {
    b->buf = storage;
    b->cap = cap;
    b->len = 0;
    b->overflow = 0;
    if (cap) {
        storage[0] = 0;
    }
}

static void ito_buf_append(ito_buf *b, ito s) {
    if (b->overflow || b->len + s.len + 1 > b->cap) {
        b->overflow = 1;
        return;
    }
    memcpy(b->buf + b->len, s.ptr, s.len);
    b->len += s.len;
    b->buf[b->len] = 0;
}

static void ito_buf_append_c(ito_buf *b, const char *c) {
    ito_buf_append(b, ito_from(c));
}

/* One printf conversion at a time: parse the %spec, va_arg the matching
   type, render with snprintf into the remaining space. %S consumes an ito
   by value (width/precision are not applied to %S). Unknown conversions
   are emitted literally -- the formatter never guesses at varargs it
   cannot type. NOTE: -Wformat cannot check %S call sites; use
   ITO_FMT/ITO_ARG where compiler checking matters. */
static void ito_buf_vappendf(ito_buf *b, const char *fmt, va_list args) {
    const char *p = fmt;
    while (*p && !b->overflow) {
        if (*p != '%') {
            const char *lit = p;
            while (*p && *p != '%') {
                p++;
            }
            ito_buf_append(b, (ito){ lit, (size_t)(p - lit) });
            continue;
        }
        p++; /* past '%' */
        if (*p == '%') {
            ito_buf_append(b, ITO("%"));
            p++;
            continue;
        }
        if (*p == 'S') {
            ito_buf_append(b, va_arg(args, ito));
            p++;
            continue;
        }

        /* rebuild one spec, resolving '*' from the args */
        char spec[48];
        size_t si = 0;
        spec[si++] = '%';
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
            if (si < sizeof(spec) - 8) {
                spec[si++] = *p;
            }
            p++;
        }
        if (*p == '*') {
            si += (size_t)snprintf(spec + si, sizeof(spec) - si, "%d",
                                   va_arg(args, int));
            p++;
        } else {
            while (*p >= '0' && *p <= '9') {
                if (si < sizeof(spec) - 8) {
                    spec[si++] = *p;
                }
                p++;
            }
        }
        if (*p == '.') {
            spec[si++] = *p++;
            if (*p == '*') {
                si += (size_t)snprintf(spec + si, sizeof(spec) - si, "%d",
                                       va_arg(args, int));
                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    if (si < sizeof(spec) - 8) {
                        spec[si++] = *p;
                    }
                    p++;
                }
            }
        }
        int len_ll = 0, len_l = 0, len_z = 0;
        while (*p == 'l' || *p == 'h' || *p == 'z') {
            if (*p == 'z') {
                len_z = 1;
            } else if (*p == 'l') {
                len_l ? (len_ll = 1) : (len_l = 1);
            } /* h/hh promote to int; nothing to track */
            if (si < sizeof(spec) - 8) {
                spec[si++] = *p;
            }
            p++;
        }
        char conv = *p ? *p : 0;
        if (conv) {
            spec[si++] = conv;
            p++;
        }
        spec[si] = 0;

        size_t room = b->cap - b->len;
        int n = -1;
        switch (conv) {
        case 'd': case 'i':
            if (len_ll)     n = snprintf(b->buf + b->len, room, spec, va_arg(args, long long));
            else if (len_l) n = snprintf(b->buf + b->len, room, spec, va_arg(args, long));
            else if (len_z) n = snprintf(b->buf + b->len, room, spec, va_arg(args, size_t));
            else            n = snprintf(b->buf + b->len, room, spec, va_arg(args, int));
            break;
        case 'u': case 'x': case 'X': case 'o':
            if (len_ll)     n = snprintf(b->buf + b->len, room, spec, va_arg(args, unsigned long long));
            else if (len_l) n = snprintf(b->buf + b->len, room, spec, va_arg(args, unsigned long));
            else if (len_z) n = snprintf(b->buf + b->len, room, spec, va_arg(args, size_t));
            else            n = snprintf(b->buf + b->len, room, spec, va_arg(args, unsigned));
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            n = snprintf(b->buf + b->len, room, spec, va_arg(args, double));
            break;
        case 'c':
            n = snprintf(b->buf + b->len, room, spec, va_arg(args, int));
            break;
        case 's':
            n = snprintf(b->buf + b->len, room, spec, va_arg(args, const char *));
            break;
        case 'p':
            n = snprintf(b->buf + b->len, room, spec, va_arg(args, void *));
            break;
        case 'S':
            /* %S with flags/width reaches here (the fast path above only
               handles bare %S): still consume the ito so later varargs
               stay aligned; width/precision are ignored by contract */
            ito_buf_append(b, va_arg(args, ito));
            n = 0;
            room = b->cap;
            break;
        default:
            /* unknown conversion: emit the spec text itself, consume no
               argument (guessing a varargs type would be UB) */
            ito_buf_append_c(b, spec);
            n = 0;
            room = b->cap; /* the append above already accounted */
            break;
        }
        if (n < 0 || (size_t)n + 1 > room) {
            b->overflow = 1;
            if (b->cap) {
                b->buf[b->len] = 0;
            }
            return;
        }
        b->len += (size_t)n;
    }
    if (b->cap) {
        b->buf[b->len] = 0;
    }
}

static void ito_buf_appendf(ito_buf *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ito_buf_vappendf(b, fmt, args);
    va_end(args);
}

static ito ito_buf_view(const ito_buf *b) {
    ito s;
    s.ptr = b->buf;
    s.len = b->len;
    return s;
}

/* OS boundary: the builder's storage is NUL-terminated after every
   append, so it doubles as the C string. */
static const char *ito_buf_cstr(const ito_buf *b) {
    return b->buf;
}

/* format once into a caller buffer and return the view -- the common
   "build a small frame-static string for clay / a label" idiom. The view
   points into `buf`, which must outlive the use. Overflow truncates (the
   view is just shorter); use ito_buf directly when overflow must be seen. */
static ito ito_format(char *buf, size_t cap, const char *fmt, ...) {
    ito_buf b;
    va_list args;
    ito_buf_init(&b, buf, cap);
    va_start(args, fmt);
    ito_buf_vappendf(&b, fmt, args);
    va_end(args);
    return ito_buf_view(&b);
}

#endif /* ITO_H */
