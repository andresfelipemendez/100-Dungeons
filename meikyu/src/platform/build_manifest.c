#include "build_manifest.h"
#include <string.h>
#include <stdio.h>

/* split "key=value" into key (before '=') and value (after). Missing '=' ->
   empty value pointing just past the field. */
static void kv_split(ito field, ito *key, ito *val) {
    size_t i;
    for (i = 0; i < field.len; i++) {
        if (field.ptr[i] == '=') {
            *key = ito_slice(field, 0, i);
            *val = ito_slice(field, i + 1, field.len);
            return;
        }
    }
    *key = field;
    val->ptr = field.ptr + field.len;
    val->len = 0;
}

static void set_err(arena *a, ito *err, const char *msg) {
    size_t n = strlen(msg);
    char *c = arena_copy_string(a, msg, n);
    err->ptr = c;
    err->len = c ? n : 0;
}

/* split a comma-separated value into trimmed views, appended to arr. Empty
   tokens are skipped. 0 on overflow (sets *err). */
static b32 split_commas(arena *a, ito *err, ito v, ito *arr, int *count, int max) {
    while (v.len) {
        int comma = ito_find(v, ',');
        ito tok;
        if (comma < 0) {
            tok = v;
            v.len = 0;
        } else {
            tok = ito_slice(v, 0, (size_t)comma);
            v = ito_slice(v, (size_t)comma + 1, v.len);
        }
        tok = ito_trim(tok);
        if (!tok.len) {
            continue;
        }
        if (*count >= max) {
            set_err(a, err, "too many entries in lib_test list field");
            return 0;
        }
        arr[(*count)++] = tok;
    }
    return 1;
}

b32 build_manifest_parse(arena *a, ito text, BuildManifest *out, ito *err) {
    memset(out, 0, sizeof(*out));
    while (text.len) {
        ito line = ito_trim(ito_next_line(&text));
        ito key, rest;
        if (!line.len || line.ptr[0] == '#') {
            continue;
        }
        key = ito_next_token(&line);
        rest = ito_trim(line);
        if (ito_eq_c(key, "host_src")) {
            if (out->host_src_count >= BM_MAX_HOST_SRC) {
                set_err(a, err, "too many host_src"); return 0;
            }
            out->host_src[out->host_src_count++] = rest;
        } else if (ito_eq_c(key, "include_root")) {
            if (out->include_root_count >= BM_MAX_INCLUDE_ROOT) {
                set_err(a, err, "too many include_root"); return 0;
            }
            out->include_root[out->include_root_count++] = rest;
        } else if (ito_eq_c(key, "sdl_version")) {
            out->sdl_version = rest;
        } else if (ito_eq_c(key, "vulkan_pin")) {
            out->vulkan_pin = rest;
        } else if (ito_eq_c(key, "glslc_candidate")) {
            if (out->glslc_count >= BM_MAX_GLSLC) {
                set_err(a, err, "too many glslc_candidate"); return 0;
            }
            out->glslc_candidate[out->glslc_count++] = rest;
        } else if (ito_eq_c(key, "cc_candidate")) {
            if (out->cc_count >= BM_MAX_GLSLC) {
                set_err(a, err, "too many cc_candidate"); return 0;
            }
            out->cc_candidate[out->cc_count++] = rest;
        } else if (ito_eq_c(key, "platform")) {
            int idx;
            if (out->platform_count >= BM_MAX_PLATFORM) {
                set_err(a, err, "too many platform rows"); return 0;
            }
            idx = out->platform_count++;
            out->platform[idx].name = ito_next_token(&rest);
            for (;;) {
                ito field = ito_next_token(&rest);
                ito fk, fv;
                if (!field.len) {
                    break;
                }
                kv_split(field, &fk, &fv);
                if      (ito_eq_c(fk, "builddir")) out->platform[idx].builddir = fv;
                else if (ito_eq_c(fk, "exe"))      out->platform[idx].exe = fv;
                else if (ito_eq_c(fk, "dll"))      out->platform[idx].dll = fv;
                else if (ito_eq_c(fk, "dodai"))    out->platform[idx].dodai = fv;
                else if (ito_eq_c(fk, "rpath"))    out->platform[idx].rpath = fv;
                else if (ito_eq_c(fk, "sdl"))      out->platform[idx].sdl = fv;
                else { set_err(a, err, "unknown platform field"); return 0; }
            }
            if (!out->platform[idx].name.len) {
                set_err(a, err, "platform row missing name"); return 0;
            }
        } else if (ito_eq_c(key, "coverage_min_mcdc")) {
            out->coverage_min_mcdc = rest;
        } else if (ito_eq_c(key, "llvmcov_candidate")) {
            if (out->llvmcov_count >= BM_MAX_GLSLC) {
                set_err(a, err, "too many llvmcov_candidate"); return 0;
            }
            out->llvmcov_candidate[out->llvmcov_count++] = rest;
        } else if (ito_eq_c(key, "llvmprofdata_candidate")) {
            if (out->llvmprofdata_count >= BM_MAX_GLSLC) {
                set_err(a, err, "too many llvmprofdata_candidate"); return 0;
            }
            out->llvmprofdata_candidate[out->llvmprofdata_count++] = rest;
        } else if (ito_eq_c(key, "lib_test")) {
            BmLibTest *lt;
            if (out->lib_test_count >= BM_MAX_LIB_TEST) {
                set_err(a, err, "too many lib_test rows"); return 0;
            }
            lt = &out->lib_test[out->lib_test_count++];
            lt->name = ito_next_token(&rest);
            lt->pedantic = 1; /* default on (rest zeroed by memset) */
            if (!lt->name.len) {
                set_err(a, err, "lib_test row missing name"); return 0;
            }
            for (;;) {
                ito field = ito_next_token(&rest);
                ito fk, fv;
                if (!field.len) {
                    break;
                }
                kv_split(field, &fk, &fv);
                if (ito_eq_c(fk, "std")) {
                    lt->std = fv;
                } else if (ito_eq_c(fk, "pedantic")) {
                    lt->pedantic = !ito_eq_c(fv, "0");
                } else if (ito_eq_c(fk, "src")) {
                    if (!split_commas(a, err, fv, lt->src, &lt->src_count,
                                      BM_MAX_LT_LIST)) return 0;
                } else if (ito_eq_c(fk, "include")) {
                    if (!split_commas(a, err, fv, lt->include, &lt->include_count,
                                      BM_MAX_LT_LIST)) return 0;
                } else if (ito_eq_c(fk, "link")) {
                    if (!split_commas(a, err, fv, lt->link, &lt->link_count,
                                      BM_MAX_LT_LIST)) return 0;
                } else {
                    set_err(a, err, "unknown lib_test field"); return 0;
                }
            }
        } else {
            set_err(a, err, "unknown manifest key");
            return 0;
        }
    }
    if (out->platform_count == 0) {
        set_err(a, err, "no platform rows");
        return 0;
    }
    return 1;
}

b32 build_manifest_select(const BuildManifest *m, ito os, BuildPlatform *out, ito *err) {
    int i;
    memset(out, 0, sizeof(*out));
    for (i = 0; i < m->platform_count; i++) {
        if (ito_eq(m->platform[i].name, os)) {
            out->builddir     = m->platform[i].builddir;
            out->exe_suffix   = m->platform[i].exe;
            out->dll_suffix   = m->platform[i].dll;
            out->dodai_src    = m->platform[i].dodai;
            out->rpath        = m->platform[i].rpath;
            out->sdl_strategy = m->platform[i].sdl;
            return 1;
        }
    }
    {
        static const char msg[] = "no platform row for this OS";
        err->ptr = msg;
        err->len = sizeof(msg) - 1;
    }
    return 0;
}

/* expand a leading $NAME path segment via getenv_fn; copies the rest verbatim.
   only the leading-$ form is supported (the only form the manifest uses). */
static void expand_dollar(ito cand, const char *(*getenv_fn)(const char *),
                          char *out, size_t cap) {
    if (cand.len && cand.ptr[0] == '$') {
        char name[128];
        size_t i = 1;
        ito tail;
        const char *v;
        while (i < cand.len && cand.ptr[i] != '/' && (i - 1) < sizeof(name) - 1) {
            name[i - 1] = cand.ptr[i];
            i++;
        }
        name[i - 1] = 0;
        v = getenv_fn(name);
        tail = ito_slice(cand, i, cand.len);
        snprintf(out, cap, "%s" ITO_FMT, v ? v : "", ITO_ARG(tail));
    } else {
        snprintf(out, cap, ITO_FMT, ITO_ARG(cand));
    }
}

/* walk a candidate list: expand each, take the first that exists. */
static b32 resolve_candidates(const ito *cands, int n,
                              const char *(*getenv_fn)(const char *),
                              b32 (*exists_fn)(const char *),
                              char *out_buf, size_t cap) {
    int i;
    for (i = 0; i < n; i++) {
        char probe[1024];
        expand_dollar(cands[i], getenv_fn, probe, sizeof(probe));
        if (probe[0] && exists_fn(probe)) {
            snprintf(out_buf, cap, "%s", probe);
            return 1;
        }
    }
    return 0;
}

b32 build_manifest_resolve_glslc(const BuildManifest *m,
                                 const char *(*getenv_fn)(const char *),
                                 b32 (*exists_fn)(const char *),
                                 char *out_buf, size_t cap) {
    return resolve_candidates(m->glslc_candidate, m->glslc_count,
                              getenv_fn, exists_fn, out_buf, cap);
}

b32 build_manifest_resolve_cc(const BuildManifest *m,
                              const char *(*getenv_fn)(const char *),
                              b32 (*exists_fn)(const char *),
                              char *out_buf, size_t cap) {
    return resolve_candidates(m->cc_candidate, m->cc_count,
                              getenv_fn, exists_fn, out_buf, cap);
}

b32 build_manifest_resolve_llvmcov(const BuildManifest *m,
                                   const char *(*getenv_fn)(const char *),
                                   b32 (*exists_fn)(const char *),
                                   char *out_buf, size_t cap) {
    return resolve_candidates(m->llvmcov_candidate, m->llvmcov_count,
                              getenv_fn, exists_fn, out_buf, cap);
}

b32 build_manifest_resolve_llvmprofdata(const BuildManifest *m,
                                        const char *(*getenv_fn)(const char *),
                                        b32 (*exists_fn)(const char *),
                                        char *out_buf, size_t cap) {
    return resolve_candidates(m->llvmprofdata_candidate, m->llvmprofdata_count,
                              getenv_fn, exists_fn, out_buf, cap);
}
