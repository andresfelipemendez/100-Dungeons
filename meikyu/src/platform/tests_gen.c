#include "tests_gen.h"

/* the std token, defaulting to c99 when the row omits it */
static const char *lt_std(const BmLibTest *lt) {
    if (ito_eq_c(lt->std, "c89")) return "c89";
    if (ito_eq_c(lt->std, "c11")) return "c11";
    return "c99";
}

void tests_gen_emit(ito_buf *out, const BuildManifest *m, const BuildPlatform *p,
                    const char *engine_root, const char *cc, b32 coverage) {
    const char *covdir = coverage ? "/cov" : "";
    int i, j;

    /* ${B} resolves to the per-OS builddir; tool cc sets kaji's compiler */
    ito_buf_appendf(out, "builddir %.*s\n", (int)p->builddir.len, p->builddir.ptr);
    ito_buf_appendf(out, "tool cc %s\n\n", cc);

    for (i = 0; i < m->lib_test_count; i++) {
        const BmLibTest *lt = &m->lib_test[i];
        int nlen = (int)lt->name.len;
        const char *nm = lt->name.ptr;

        ito_buf_appendf(out, "target test_%.*s exe\n", nlen, nm);

        /* sources: implicit test.c, then each extra src relative to the lib */
        ito_buf_appendf(out, "  in \"%s/lib/%.*s/test.c\"", engine_root, nlen, nm);
        for (j = 0; j < lt->src_count; j++) {
            ito s = lt->src[j];
            int slash = ito_find_last(s, '/');
            ito base = (slash < 0) ? s : ito_slice(s, (size_t)slash + 1, s.len);
            if (ito_starts_with(base, ITO("dodai_"))) {
                /* per-OS dodai source: keep the dir prefix, swap the file */
                ito dir = (slash < 0) ? (ito){0, 0}
                                      : ito_slice(s, 0, (size_t)slash + 1);
                ito_buf_appendf(out, " \"%s/lib/%.*s/%.*s%.*s\"",
                                engine_root, nlen, nm,
                                (int)dir.len, dir.ptr,
                                (int)p->dodai_src.len, p->dodai_src.ptr);
            } else {
                ito_buf_appendf(out, " \"%s/lib/%.*s/%.*s\"",
                                engine_root, nlen, nm, (int)s.len, s.ptr);
            }
        }
        ito_buf_append(out, ITO("\n"));

        ito_buf_appendf(out, "  out ${B}/gen%s/test_%.*s%.*s\n",
                        covdir, nlen, nm,
                        (int)p->exe_suffix.len, p->exe_suffix.ptr);

        /* includes: the lib dir + engine src are always visible, then extras */
        ito_buf_appendf(out, "  include \"%s/lib/%.*s\" \"%s/src\"",
                        engine_root, nlen, nm, engine_root);
        for (j = 0; j < lt->include_count; j++) {
            ito inc = lt->include[j];
            ito_buf_appendf(out, " \"%s/%.*s\"",
                            engine_root, (int)inc.len, inc.ptr);
        }
        ito_buf_append(out, ITO("\n"));

        ito_buf_appendf(out, "  std %s\n", lt_std(lt));
        if (lt->pedantic) ito_buf_append(out, ITO("  pedantic\n"));
        if (coverage)     ito_buf_append(out, ITO("  coverage\n"));
        for (j = 0; j < lt->link_count; j++) {
            ito_buf_appendf(out, "  lib %.*s\n",
                            (int)lt->link[j].len, lt->link[j].ptr);
        }
        ito_buf_append(out, ITO("\n"));
    }
}
