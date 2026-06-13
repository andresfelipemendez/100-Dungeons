#ifndef MICHI_H
#define MICHI_H

/* michi (道): filesystem paths as a first-party type, layered on ito.
   A `michi` is a distinct path VIEW (it carries an ito) so a bare string
   cannot be passed where a path is expected -- the string->path conversion
   is the explicit michi_from_* call. `michi_buf` is an owning fixed-cap
   builder: an ito_buf over inline storage, so append / sticky-overflow /
   NUL-termination / appendf all come from ito. The path cap and the
   never-silently-truncate policy live here, once.

   michi_buf must not be copied by value (its inner ito_buf points into its
   own inline buf); pass &mb. Header-only; include freely. Separators are
   '/'; michi_normalize folds '\\' to '/'. */

#include "../ito/ito.h"

#define MICHI_MAX 4096

/* ---- path view ---------------------------------------------------------- */
typedef struct { ito s; } michi;

#define PATH(lit) ((michi){ { (lit), sizeof(lit) - 1 } })

static michi michi_from_ito(ito s)        { michi m; m.s = s; return m; }
static michi michi_from_cstr(const char *c) { michi m; m.s = ito_from(c); return m; }
static ito   michi_str(michi m)           { return m.s; } /* for logging / %S */

/* ---- path builder (owns MICHI_MAX bytes) -------------------------------- */
typedef struct { char buf[MICHI_MAX]; ito_buf b; } michi_buf;

static void  michi_buf_reset(michi_buf *mb) { ito_buf_init(&mb->b, mb->buf, MICHI_MAX); }
static michi michi_view(const michi_buf *mb) { michi m; m.s = ito_buf_view(&mb->b); return m; }
static const char *michi_cstr(const michi_buf *mb) { return ito_buf_cstr(&mb->b); }

static void michi_set(michi_buf *mb, michi p) {
    michi_buf_reset(mb);
    ito_buf_append(&mb->b, p.s);
}

/* append one component with exactly one '/' between -- none if the buffer
   is empty, already ends in '/', or the component begins with '/' */
static void michi_join(michi_buf *mb, michi component) {
    ito cur = ito_buf_view(&mb->b);
    int sep = cur.len > 0 && cur.ptr[cur.len - 1] != '/'
           && !(component.s.len > 0 && component.s.ptr[0] == '/');
    if (sep) {
        ito_buf_append(&mb->b, ITO("/"));
    }
    ito_buf_append(&mb->b, component.s);
}

/* append a formatted component (e.g. "game_loaded_%lu.so"), '/'-separated
   from what's already there. The component is assumed not to start with a
   separator (callers format file/dir names, not absolute paths). */
static void michi_joinf(michi_buf *mb, const char *fmt, ...) {
    ito cur = ito_buf_view(&mb->b);
    va_list args;
    if (cur.len > 0 && cur.ptr[cur.len - 1] != '/') {
        ito_buf_append(&mb->b, ITO("/"));
    }
    va_start(args, fmt);
    ito_buf_vappendf(&mb->b, fmt, args);
    va_end(args);
}

/* ---- decomposition (read-only sub-views) -------------------------------- */
/* last '/' or '\\' (handle either separator) */
static int michi_last_sep(michi p) {
    int f = ito_find_last(p.s, '/');
    int b = ito_find_last(p.s, '\\');
    return b > f ? b : f;
}

/* dirname: "." when there is no separator, "/" when the only one is at 0 */
static michi michi_dir(michi p) {
    int i = michi_last_sep(p);
    michi m;
    if (i < 0)       m.s = ITO(".");
    else if (i == 0) m.s = ITO("/");
    else             m.s = ito_slice(p.s, 0, (size_t)i);
    return m;
}

/* basename: the component after the last separator (whole path if none) */
static michi michi_base(michi p) {
    int i = michi_last_sep(p);
    michi m;
    m.s = (i < 0) ? p.s : ito_slice(p.s, (size_t)i + 1, p.s.len);
    return m;
}

/* extension incl. the dot (".c"); empty when none or a leading-dot hidden
   file (".bashrc") */
static michi michi_ext(michi p) {
    michi base = michi_base(p);
    int d = ito_find_last(base.s, '.');
    michi m;
    if (d <= 0) {
        m.s.ptr = base.s.ptr + base.s.len;
        m.s.len = 0;
    } else {
        m.s = ito_slice(base.s, (size_t)d, base.s.len);
    }
    return m;
}

/* fold backslashes to forward slashes in place (windows paths, and so a
   path is safe to paste into generated #include / .incbin strings) */
static void michi_normalize(michi_buf *mb) {
    for (size_t i = 0; i < mb->b.len; i++) {
        if (mb->buf[i] == '\\') {
            mb->buf[i] = '/';
        }
    }
}

#endif /* MICHI_H */
