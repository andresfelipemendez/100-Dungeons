#include "kansi.h"
#include "dodai.h"

typedef struct { void *handle; } kansi_proc;

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KANSI_MAX_PATH    512
#define KANSI_MAX_CMD     4096
#define KANSI_MAX_WATCH   16
#define KANSI_MAX_EXT     16
#define KANSI_MAX_PRE     16
#define KANSI_MAX_INC     32
#define KANSI_MAX_LIBDIR  8
#define KANSI_MAX_LIB     16
#define KANSI_MAX_FLAG    16
#define KANSI_MAX_DEFINE  16

#define KANSI_POLL_MS 50

typedef struct {
    char watch[KANSI_MAX_WATCH][KANSI_MAX_PATH];   int watch_count;
    char ext[KANSI_MAX_EXT][16];                   int ext_count;
    char pre[KANSI_MAX_PRE][KANSI_MAX_CMD];        int pre_count;
    char pre_in[KANSI_MAX_PRE][KANSI_MAX_PATH];    /* empty = unconditional */
    char pre_out[KANSI_MAX_PRE][KANSI_MAX_PATH];
    char obj[KANSI_MAX_INC][KANSI_MAX_PATH];       int obj_count;
    char include[KANSI_MAX_INC][KANSI_MAX_PATH];   int include_count;
    char libdir[KANSI_MAX_LIBDIR][KANSI_MAX_PATH]; int libdir_count;
    char lib[KANSI_MAX_LIB][64];                   int lib_count;
    char flag[KANSI_MAX_FLAG][64];                 int flag_count;
    char define[KANSI_MAX_DEFINE][256];            int define_count;
    char source[KANSI_MAX_PATH];
    char out[KANSI_MAX_PATH];
    char tmp[KANSI_MAX_PATH];
    char log[KANSI_MAX_PATH];
    int debounce_ms;
    int watch_only; /* report KANSI_CHANGED instead of building */
} kansi_cfg;

typedef enum {
    KANSI_STATE_IDLE,
    KANSI_STATE_WAITING,
    KANSI_STATE_BUILDING
} kansi_state;

struct kansi {
    kansi_cfg cfg;
    kansi_state state;
    unsigned long long last_stamp;
    unsigned long long last_scan_ms;
    unsigned long long change_seen_ms;
    int step;             /* index into pipeline; cfg.pre_count == compile */
    kansi_proc proc;
    /* event-based notification; falls back to polling when unavailable */
    dodai_watch watch;
    int event_mode;
    int note;             /* a matching event arrived since last handled */
    int rebuild_queued;   /* change arrived mid-build */
};


static int kansi_has_ext(const kansi_cfg *cfg, const char *path) {
    if (cfg->ext_count == 0) {
        return 1; /* no filter configured: everything counts */
    }
    size_t plen = strlen(path);
    for (int i = 0; i < cfg->ext_count; i++) {
        size_t elen = strlen(cfg->ext[i]);
        if (elen > plen) {
            continue;
        }
        const char *tail = path + plen - elen;
        size_t j;
        for (j = 0; j < elen; j++) {
            if (tolower((unsigned char)tail[j]) !=
                tolower((unsigned char)cfg->ext[i][j])) {
                break;
            }
        }
        if (j == elen) {
            return 1;
        }
    }
    return 0;
}

/* ---- config parser ---------------------------------------------------
   Line-based: `key value`, '#' comments, keys repeatable. Returns 0 and
   fills err on failure. */

static int kansi_parse_line(kansi_cfg *cfg, ito line, char *err, int err_size) {
    int hash = ito_find(line, '#');
    if (hash >= 0) {
        line = ito_slice(line, 0, (size_t)hash);
    }
    line = ito_trim(line);
    if (!line.len) {
        return 1;
    }
    ito key = ito_next_token(&line);   /* `key value...` */
    ito value = ito_trim(line);        /* rest of line, spaces kept */

#define KANSI_APPEND(arr, count, max, what)                                  \
    do {                                                                     \
        if (cfg->count >= (max)) {                                           \
            snprintf(err, err_size, "too many '%s' entries", what);          \
            return 0;                                                        \
        }                                                                    \
        ito_copy(cfg->arr[cfg->count++], sizeof(cfg->arr[0]), value);        \
    } while (0)

    if (ito_eq_c(key, "watch"))        KANSI_APPEND(watch, watch_count, KANSI_MAX_WATCH, "watch");
    else if (ito_eq_c(key, "pre"))     KANSI_APPEND(pre, pre_count, KANSI_MAX_PRE, "pre");
    else if (ito_eq_c(key, "obj"))     KANSI_APPEND(obj, obj_count, KANSI_MAX_INC, "obj");
    else if (ito_eq_c(key, "pre_newer")) {
        /* pre_newer <input> <output> <command...>: run the command only when
           output is missing or older than input */
        if (cfg->pre_count >= KANSI_MAX_PRE) {
            snprintf(err, err_size, "too many 'pre' entries");
            return 0;
        }
        ito input = ito_next_token(&value);
        ito output = ito_next_token(&value);
        ito command = ito_trim(value);
        if (!input.len || !output.len || !command.len) {
            snprintf(err, err_size, "pre_newer needs: <input> <output> <command>");
            return 0;
        }
        int i = cfg->pre_count++;
        ito_copy(cfg->pre_in[i], sizeof(cfg->pre_in[0]), input);
        ito_copy(cfg->pre_out[i], sizeof(cfg->pre_out[0]), output);
        ito_copy(cfg->pre[i], sizeof(cfg->pre[0]), command);
    }
    else if (ito_eq_c(key, "include")) KANSI_APPEND(include, include_count, KANSI_MAX_INC, "include");
    else if (ito_eq_c(key, "libdir"))  KANSI_APPEND(libdir, libdir_count, KANSI_MAX_LIBDIR, "libdir");
    else if (ito_eq_c(key, "lib"))     KANSI_APPEND(lib, lib_count, KANSI_MAX_LIB, "lib");
    else if (ito_eq_c(key, "flag"))    KANSI_APPEND(flag, flag_count, KANSI_MAX_FLAG, "flag");
    else if (ito_eq_c(key, "define"))  KANSI_APPEND(define, define_count, KANSI_MAX_DEFINE, "define");
    else if (ito_eq_c(key, "ext")) {
        /* space-separated list on one line */
        ito tok = ito_next_token(&value);
        while (tok.len) {
            if (cfg->ext_count >= KANSI_MAX_EXT) {
                snprintf(err, err_size, "too many 'ext' entries");
                return 0;
            }
            ito_copy(cfg->ext[cfg->ext_count++], sizeof(cfg->ext[0]), tok);
            tok = ito_next_token(&value);
        }
    }
    else if (ito_eq_c(key, "source"))      ito_copy(cfg->source, sizeof(cfg->source), value);
    else if (ito_eq_c(key, "out"))         ito_copy(cfg->out, sizeof(cfg->out), value);
    else if (ito_eq_c(key, "tmp"))         ito_copy(cfg->tmp, sizeof(cfg->tmp), value);
    else if (ito_eq_c(key, "log"))         ito_copy(cfg->log, sizeof(cfg->log), value);
    else if (ito_eq_c(key, "debounce_ms")) {
        char num[16];
        ito_copy(num, sizeof(num), value);
        cfg->debounce_ms = atoi(num);
    }
    else {
        snprintf(err, err_size, "unknown config key '" ITO_FMT "'", ITO_ARG(key));
        return 0;
    }
#undef KANSI_APPEND
    return 1;
}

static int kansi_parse_config(kansi_cfg *cfg, const char *text,
                              char *err, int err_size) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->debounce_ms = 300;

    ito rest = ito_from(text);
    int line_no = 0;
    while (rest.len) {
        ito line = ito_next_line(&rest);
        line_no++;
        char line_err[256] = { 0 };
        if (!kansi_parse_line(cfg, line, line_err, sizeof(line_err))) {
            snprintf(err, err_size, "line %d: %s", line_no, line_err);
            return 0;
        }
    }

    if (cfg->watch_count == 0) { snprintf(err, err_size, "config missing 'watch'"); return 0; }
    if (!cfg->source[0] && !cfg->out[0] && cfg->pre_count == 0) {
        cfg->watch_only = 1;
        return 1;
    }
    if (!cfg->source[0]) { snprintf(err, err_size, "config missing 'source'"); return 0; }
    if (!cfg->out[0])    { snprintf(err, err_size, "config missing 'out'");    return 0; }
    if (cfg->watch_count == 0) { snprintf(err, err_size, "config missing 'watch'"); return 0; }
    if (!cfg->tmp[0]) {
        snprintf(cfg->tmp, sizeof(cfg->tmp), "%s.tmp", cfg->out);
    }
    return 1;
}

/* ---- gcc command assembly -------------------------------------------- */

static int kansi_build_compile_cmd(const kansi_cfg *cfg, char *cmd, int cmd_size) {
    int n = snprintf(cmd, cmd_size, "gcc -shared");
#define KANSI_CAT(...)                                                  \
    do {                                                                \
        n += snprintf(cmd + n, cmd_size - n, __VA_ARGS__);              \
        if (n >= cmd_size) return 0;                                    \
    } while (0)
    for (int i = 0; i < cfg->flag_count; i++)    KANSI_CAT(" %s", cfg->flag[i]);
    for (int i = 0; i < cfg->define_count; i++)  KANSI_CAT(" -D%s", cfg->define[i]);
    KANSI_CAT(" %s", cfg->source);
    for (int i = 0; i < cfg->obj_count; i++)     KANSI_CAT(" %s", cfg->obj[i]);
    for (int i = 0; i < cfg->include_count; i++) KANSI_CAT(" -I%s", cfg->include[i]);
    for (int i = 0; i < cfg->libdir_count; i++)  KANSI_CAT(" -L%s", cfg->libdir[i]);
    for (int i = 0; i < cfg->lib_count; i++)     KANSI_CAT(" -l%s", cfg->lib[i]);
    KANSI_CAT(" -o %s", cfg->tmp);
#undef KANSI_CAT
    return 1;
}

/* ---- change detection -------------------------------------------------
   Stamp = commutative fold of (path, mtime, size) hashes over every watched
   file with a matching extension. Cheap full rescan; trees are small. */

typedef struct {
    const kansi_cfg *cfg;
    unsigned long long stamp;
} kansi_scan_ctx;

static unsigned long long kansi_fnv(const char *s) {
    unsigned long long h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ull;
    }
    return h;
}

static void kansi_scan_file(michi path_v, unsigned long long mtime,
                            unsigned long long size, void *user) {
    kansi_scan_ctx *ctx = (kansi_scan_ctx *)user;
    char path[MICHI_MAX];
    if (!ito_copy(path, sizeof(path), path_v.s)) {
        return;
    }
    if (!kansi_has_ext(ctx->cfg, path)) {
        return;
    }
    unsigned long long h = kansi_fnv(path);
    h ^= mtime * 1099511628211ull;
    h ^= size * 14695981039346656037ull;
    ctx->stamp += h;
}

static unsigned long long kansi_compute_stamp(const kansi_cfg *cfg) {
    kansi_scan_ctx ctx = { cfg, 0 };
    for (int i = 0; i < cfg->watch_count; i++) {
        dodai_walk(michi_from_cstr(cfg->watch[i]), kansi_scan_file, &ctx);
    }
    return ctx.stamp;
}

/* ---- pipeline ---------------------------------------------------------
   Steps 0..pre_count-1 are the raw pre commands; step pre_count is the gcc
   compile; the rename happens inline after the compile succeeds. */

/* 1 = condition satisfied, step unnecessary */
static int kansi_step_skippable(const kansi_cfg *cfg, int step) {
    if (step >= cfg->pre_count || !cfg->pre_in[step][0]) {
        return 0;
    }
    unsigned long long in_t, out_t;
    if (!dodai_mtime_ns(michi_from_cstr(cfg->pre_in[step]), &in_t)) {
        return 0; /* missing input: run the step, let it fail loudly */
    }
    if (!dodai_mtime_ns(michi_from_cstr(cfg->pre_out[step]), &out_t)) {
        return 0; /* no output yet */
    }
    return out_t >= in_t;
}

static int kansi_spawn_step(kansi *k) {
    char cmd[KANSI_MAX_CMD];
    const char *to_run;
    while (k->step < k->cfg.pre_count && kansi_step_skippable(&k->cfg, k->step)) {
        k->step++;
    }
    if (k->step < k->cfg.pre_count) {
        to_run = k->cfg.pre[k->step];
    } else {
        if (!kansi_build_compile_cmd(&k->cfg, cmd, sizeof(cmd))) {
            return 0;
        }
        to_run = cmd;
    }
    return dodai_spawn(ito_from(to_run), michi_from_cstr(k->cfg.log), /* empty = no redirect */
                       &k->proc.handle);
}

/* ---- public API -------------------------------------------------------- */

kansi *kansi_start(const char *config_path, char *err, int err_size) {
    char dummy[8];
    if (!err) {
        err = dummy;
        err_size = sizeof(dummy);
    }

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        snprintf(err, err_size, "cannot open '%s'", config_path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
        snprintf(err, err_size, "cannot read '%s'", config_path);
        free(text);
        fclose(f);
        return NULL;
    }
    text[size] = 0;
    fclose(f);

    kansi *k = (kansi *)calloc(1, sizeof(kansi));
    if (!k) {
        free(text);
        snprintf(err, err_size, "out of memory");
        return NULL;
    }
    int ok = kansi_parse_config(&k->cfg, text, err, err_size);
    free(text);
    if (!ok) {
        free(k);
        return NULL;
    }

    k->state = KANSI_STATE_IDLE;
    k->last_stamp = kansi_compute_stamp(&k->cfg);
    k->last_scan_ms = dodai_now_ms();
    k->event_mode = dodai_watch_begin(&k->watch, k->cfg.watch,
                                      k->cfg.watch_count);
    return k;
}

/* Event callback: an empty path means the OS event buffer overflowed --
   treat it as a change so nothing is missed. */
static void kansi_on_notify(michi path_v, void *user) {
    kansi *k = (kansi *)user;
    char path[MICHI_MAX];
    if (!ito_copy(path, sizeof(path), path_v.s)) {
        return;
    }
    if (!path[0] || kansi_has_ext(&k->cfg, path)) {
        k->note = 1;
    }
}

kansi_status kansi_update(kansi *k) {
    unsigned long long now = dodai_now_ms();

    /* Drain change notifications first (also while building, so saves
       during a compile queue a follow-up rebuild instead of vanishing). */
    if (k->event_mode) {
        dodai_watch_poll(&k->watch, kansi_on_notify, k);
        if (k->note) {
            k->note = 0;
            k->change_seen_ms = now;
            if (k->state == KANSI_STATE_BUILDING) {
                k->rebuild_queued = 1;
            } else {
                k->state = KANSI_STATE_WAITING;
            }
        }
    }

    if (k->state == KANSI_STATE_BUILDING) {
        int exit_code = 0;
        if (!dodai_proc_poll(k->proc.handle, &exit_code)) {
            return KANSI_BUILDING;
        }
        dodai_proc_close(k->proc.handle); k->proc.handle = NULL;
        if (exit_code != 0) {
            k->state = k->rebuild_queued ? KANSI_STATE_WAITING : KANSI_STATE_IDLE;
            k->rebuild_queued = 0;
            return KANSI_ERROR;
        }
        k->step++;
        if (k->step <= k->cfg.pre_count) {
            if (!kansi_spawn_step(k)) {
                k->state = KANSI_STATE_IDLE;
                return KANSI_ERROR;
            }
            return KANSI_BUILDING;
        }
        /* all steps done: publish the dll */
        k->state = k->rebuild_queued ? KANSI_STATE_WAITING : KANSI_STATE_IDLE;
        k->rebuild_queued = 0;
        if (!dodai_rename(michi_from_cstr(k->cfg.tmp), michi_from_cstr(k->cfg.out))) {
            k->state = KANSI_STATE_IDLE;
            return KANSI_ERROR;
        }
        return KANSI_BUILT;
    }

    /* polling fallback: throttled rescan */
    if (!k->event_mode && now - k->last_scan_ms >= KANSI_POLL_MS) {
        k->last_scan_ms = now;
        unsigned long long stamp = kansi_compute_stamp(&k->cfg);
        if (stamp != k->last_stamp) {
            k->last_stamp = stamp;
            k->change_seen_ms = now;
            k->state = KANSI_STATE_WAITING;
        }
    }

    if (k->cfg.watch_only) {
        if (k->state == KANSI_STATE_WAITING &&
            now - k->change_seen_ms >= (unsigned long long)k->cfg.debounce_ms) {
            k->state = KANSI_STATE_IDLE;
            return KANSI_CHANGED;
        }
        return k->state == KANSI_STATE_WAITING ? KANSI_WAITING : KANSI_IDLE;
    }

    if (k->state == KANSI_STATE_WAITING &&
        now - k->change_seen_ms >= (unsigned long long)k->cfg.debounce_ms) {
        /* truncate the log, then kick off the pipeline */
        if (k->cfg.log[0]) {
            FILE *log = fopen(k->cfg.log, "w");
            if (log) {
                fclose(log);
            }
        }
        k->step = 0;
        if (!kansi_spawn_step(k)) {
            k->state = KANSI_STATE_IDLE;
            return KANSI_ERROR;
        }
        k->state = KANSI_STATE_BUILDING;
        return KANSI_BUILDING;
    }

    return k->state == KANSI_STATE_WAITING ? KANSI_WAITING : KANSI_IDLE;
}

const char *kansi_log_path(kansi *k) {
    return k->cfg.log;
}

void kansi_stop(kansi *k) {
    if (!k) {
        return;
    }
    if (k->event_mode) {
        dodai_watch_end(&k->watch);
    }
    if (k->state == KANSI_STATE_BUILDING) {
        int code;
        while (!dodai_proc_poll(k->proc.handle, &code)) {
            /* let the in-flight step finish; steps are short-lived */
        }
        dodai_proc_close(k->proc.handle); k->proc.handle = NULL;
    }
    free(k);
}
