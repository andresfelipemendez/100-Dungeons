#include "kaji.h"
#include "kaji_str.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

#define KAJI_MAX_TARGETS 32
#define KAJI_MAX_DEPS    16
#define KAJI_MAX_INS     16
#define KAJI_MAX_LIST    32
#define KAJI_MAX_POSTS   8
#define KAJI_MAX_TOOLS   8
#define KAJI_PATH_MAX    512
#define KAJI_NAME_MAX    64

typedef enum {
    KAJI_KIND_NONE,
    KAJI_KIND_COPY,
    KAJI_KIND_SHADER,
    KAJI_KIND_OBJECT,
    KAJI_KIND_PCH,
    KAJI_KIND_DLL,
    KAJI_KIND_EXE
} kaji_kind;

typedef struct {
    int  is_dir; /* copydir vs copy */
    char from[KAJI_PATH_MAX];
    char to[KAJI_PATH_MAX];
} kaji_post;

typedef struct {
    char      name[KAJI_NAME_MAX];
    kaji_kind kind;
    char deps[KAJI_MAX_DEPS][KAJI_NAME_MAX];     int dep_count;
    char ins[KAJI_MAX_INS][KAJI_PATH_MAX];       int in_count;
    char also[KAJI_MAX_INS][KAJI_PATH_MAX];      int also_count;
    char out[KAJI_PATH_MAX];
    char include[KAJI_MAX_LIST][KAJI_PATH_MAX];  int include_count;
    char flag[KAJI_MAX_LIST][128];               int flag_count;
    char define[KAJI_MAX_LIST][128];             int define_count;
    char lib[KAJI_MAX_LIST][64];                 int lib_count;
    char libdir[KAJI_MAX_LIST][KAJI_PATH_MAX];   int libdir_count;
    char obj[KAJI_MAX_LIST][KAJI_PATH_MAX];      int obj_count;
    kaji_post posts[KAJI_MAX_POSTS];             int post_count;
    int  visiting, visited, planned;             /* plan-time DFS marks */
} kaji_target;

struct kaji {
    char builddir[KAJI_PATH_MAX];
    char tool_name[KAJI_MAX_TOOLS][KAJI_NAME_MAX];
    char tool_cmd[KAJI_MAX_TOOLS][KAJI_PATH_MAX];
    int  tool_count;
    kaji_target targets[KAJI_MAX_TARGETS];
    int  target_count;
};

/* ---- vars -------------------------------------------------------------- */

static int kaji_platform_is_posix(void) {
    return kaji_platform_is_linux() || kaji_platform_is_macos();
}

static const char *kaji_so_ext(void) {
    return kaji_platform_is_posix() ? ".so" : ".dll";
}

static const char *kaji_exe_ext(void) {
    return kaji_platform_is_posix() ? "" : ".exe";
}

static const char *kaji_tool(const kaji *k, kstr name) {
    for (int i = 0; i < k->tool_count; i++) {
        if (kstr_eq_c(name, k->tool_name[i])) {
            return k->tool_cmd[i];
        }
    }
    return NULL;
}

/* Expands ${B}, ${SO}, ${EXE} and ${<tool>}. Returns 0 on overflow or
   unknown variable. */
static int kaji_expand(const kaji *k, kstr in, char *out, size_t out_size) {
    kstr_buf b;
    kstr_buf_init(&b, out, out_size);
    while (in.len) {
        if (in.len >= 2 && in.ptr[0] == '$' && in.ptr[1] == '{') {
            kstr rest = kstr_slice(in, 2, in.len);
            int close = kstr_find(rest, '}');
            if (close < 0) {
                return 0;
            }
            kstr var = kstr_slice(rest, 0, (size_t)close);
            const char *val;
            if (kstr_eq(var, KSTR("B")))        val = k->builddir;
            else if (kstr_eq(var, KSTR("SO")))  val = kaji_so_ext();
            else if (kstr_eq(var, KSTR("EXE"))) val = kaji_exe_ext();
            else                                val = kaji_tool(k, var);
            if (!val) {
                return 0;
            }
            kstr_buf_append_c(&b, val);
            in = kstr_slice(rest, (size_t)close + 1, rest.len);
        } else {
            kstr_buf_append(&b, kstr_slice(in, 0, 1));
            in = kstr_slice(in, 1, in.len);
        }
    }
    return !b.overflow;
}

/* ---- config parser ------------------------------------------------------
   `key value...`; keys may end in _win/_linux to apply on one OS only. */

static kaji_kind kaji_kind_from(kstr s) {
    if (kstr_eq(s, KSTR("copy")))   return KAJI_KIND_COPY;
    if (kstr_eq(s, KSTR("shader"))) return KAJI_KIND_SHADER;
    if (kstr_eq(s, KSTR("object"))) return KAJI_KIND_OBJECT;
    if (kstr_eq(s, KSTR("pch")))    return KAJI_KIND_PCH;
    if (kstr_eq(s, KSTR("dll")))    return KAJI_KIND_DLL;
    if (kstr_eq(s, KSTR("exe")))    return KAJI_KIND_EXE;
    return KAJI_KIND_NONE;
}

/* strips a _win/_linux/_mac suffix; returns 0 if the key is for another OS */
static int kaji_key_applies(kstr *key) {
    if (kstr_ends_with(*key, KSTR("_win"))) {
        *key = kstr_slice(*key, 0, key->len - 4);
        return !kaji_platform_is_posix();
    }
    if (kstr_ends_with(*key, KSTR("_linux"))) {
        *key = kstr_slice(*key, 0, key->len - 6);
        return kaji_platform_is_linux();
    }
    if (kstr_ends_with(*key, KSTR("_mac"))) {
        *key = kstr_slice(*key, 0, key->len - 4);
        return kaji_platform_is_macos();
    }
    return 1;
}

typedef struct {
    kaji *k;
    kaji_target *t;
    int line_no;
    char *err;
    int err_size;
} kaji_parse_ctx;

static int kaji_fail(kaji_parse_ctx *ctx, const char *msg, kstr what) {
    snprintf(ctx->err, (size_t)ctx->err_size, "line %d: %s '%.*s'",
             ctx->line_no, msg, (int)what.len, what.ptr);
    return 0;
}

static int kaji_append_tokens(kaji_parse_ctx *ctx, kstr values, kstr key,
                              char *arr, size_t elem_size, int *count, int max) {
    for (;;) {
        kstr tok = kstr_next_token(&values);
        if (kstr_is_empty(tok)) {
            return 1;
        }
        if (*count >= max) {
            return kaji_fail(ctx, "too many entries for", key);
        }
        if (!kaji_expand(ctx->k, tok, arr + (size_t)(*count) * elem_size, elem_size)) {
            return kaji_fail(ctx, "bad ${...} expansion in", tok);
        }
        (*count)++;
    }
}

static int kaji_parse_target_key(kaji_parse_ctx *ctx, kstr key, kstr values) {
    kaji_target *t = ctx->t;
    if (!t) {
        return kaji_fail(ctx, "key outside a target:", key);
    }
#define KAJI_LIST(name, arr, count, max) \
    if (kstr_eq(key, KSTR(name))) \
        return kaji_append_tokens(ctx, values, key, &t->arr[0][0], sizeof(t->arr[0]), &t->count, max)
    KAJI_LIST("dep", deps, dep_count, KAJI_MAX_DEPS);
    KAJI_LIST("in", ins, in_count, KAJI_MAX_INS);
    KAJI_LIST("also", also, also_count, KAJI_MAX_INS);
    KAJI_LIST("include", include, include_count, KAJI_MAX_LIST);
    KAJI_LIST("flag", flag, flag_count, KAJI_MAX_LIST);
    KAJI_LIST("define", define, define_count, KAJI_MAX_LIST);
    KAJI_LIST("lib", lib, lib_count, KAJI_MAX_LIST);
    KAJI_LIST("libdir", libdir, libdir_count, KAJI_MAX_LIST);
    KAJI_LIST("obj", obj, obj_count, KAJI_MAX_LIST);
#undef KAJI_LIST

    if (kstr_eq(key, KSTR("out"))) {
        kstr v = kstr_trim(values);
        if (!kaji_expand(ctx->k, v, t->out, sizeof(t->out))) {
            return kaji_fail(ctx, "bad ${...} expansion in", v);
        }
        return 1;
    }
    if (kstr_eq(key, KSTR("post"))) {
        kstr what = kstr_next_token(&values);
        kstr from = kstr_next_token(&values);
        kstr to = kstr_next_token(&values);
        int is_dir = kstr_eq(what, KSTR("copydir"));
        if ((!is_dir && !kstr_eq(what, KSTR("copy"))) ||
            kstr_is_empty(from) || kstr_is_empty(to)) {
            return kaji_fail(ctx, "post needs copy|copydir <from> <to>, got", what);
        }
        if (t->post_count >= KAJI_MAX_POSTS) {
            return kaji_fail(ctx, "too many entries for", key);
        }
        kaji_post *po = &t->posts[t->post_count++];
        po->is_dir = is_dir;
        if (!kaji_expand(ctx->k, from, po->from, sizeof(po->from)) ||
            !kaji_expand(ctx->k, to, po->to, sizeof(po->to))) {
            return kaji_fail(ctx, "bad ${...} expansion in", from);
        }
        return 1;
    }
    return kaji_fail(ctx, "unknown key", key);
}

static int kaji_parse_line(kaji_parse_ctx *ctx, kstr line) {
    int hash = kstr_find(line, '#');
    if (hash >= 0) {
        line = kstr_slice(line, 0, (size_t)hash);
    }
    line = kstr_trim(line);
    if (kstr_is_empty(line)) {
        return 1;
    }
    kstr values = line;
    kstr key = kstr_next_token(&values);
    if (!kaji_key_applies(&key)) {
        return 1;
    }
    kaji *k = ctx->k;

    if (kstr_eq(key, KSTR("builddir"))) {
        kstr v = kstr_trim(values);
        if (!kstr_copy(k->builddir, sizeof(k->builddir), v)) {
            return kaji_fail(ctx, "builddir too long:", v);
        }
        return 1;
    }
    if (kstr_eq(key, KSTR("tool"))) {
        kstr name = kstr_next_token(&values);
        kstr cmd = kstr_trim(values);
        if (kstr_is_empty(name) || kstr_is_empty(cmd)) {
            return kaji_fail(ctx, "tool needs <name> <command>, got", name);
        }
        int slot = -1;
        for (int i = 0; i < k->tool_count; i++) {
            if (kstr_eq_c(name, k->tool_name[i])) {
                slot = i;
            }
        }
        if (slot < 0) {
            if (k->tool_count >= KAJI_MAX_TOOLS) {
                return kaji_fail(ctx, "too many entries for", key);
            }
            slot = k->tool_count++;
            kstr_copy(k->tool_name[slot], sizeof(k->tool_name[0]), name);
        }
        if (!kstr_copy(k->tool_cmd[slot], sizeof(k->tool_cmd[0]), cmd)) {
            return kaji_fail(ctx, "tool command too long:", cmd);
        }
        return 1;
    }
    if (kstr_eq(key, KSTR("target"))) {
        kstr name = kstr_next_token(&values);
        kstr kind = kstr_next_token(&values);
        if (kstr_is_empty(name) || kaji_kind_from(kind) == KAJI_KIND_NONE) {
            return kaji_fail(ctx, "target needs <name> <copy|shader|object|dll|exe>, got", kind);
        }
        if (k->target_count >= KAJI_MAX_TARGETS) {
            return kaji_fail(ctx, "too many targets:", name);
        }
        ctx->t = &k->targets[k->target_count++];
        memset(ctx->t, 0, sizeof(*ctx->t));
        kstr_copy(ctx->t->name, sizeof(ctx->t->name), name);
        ctx->t->kind = kaji_kind_from(kind);
        return 1;
    }
    return kaji_parse_target_key(ctx, key, values);
}

static int kaji_parse(kaji *k, kstr text, char *err, int err_size) {
    kaji_parse_ctx ctx = { k, NULL, 0, err, err_size };

    /* defaults */
    snprintf(k->builddir, sizeof(k->builddir), "build");
    snprintf(k->tool_name[0], sizeof(k->tool_name[0]), "cc");
    snprintf(k->tool_cmd[0], sizeof(k->tool_cmd[0]), "gcc");
    k->tool_count = 1;

    /* builddir lines first: ${B} in any target must already resolve to the
       right platform value no matter where the keys sit in the file */
    kstr rest = text;
    ctx.line_no = 0;
    while (rest.len) {
        kstr line = kstr_next_line(&rest);
        ctx.line_no++;
        kstr values = kstr_trim(line);
        kstr key = kstr_next_token(&values);
        if (!kaji_key_applies(&key)) {
            continue;
        }
        if (kstr_eq(key, KSTR("builddir"))) {
            if (!kaji_parse_line(&ctx, line)) {
                return 0;
            }
        }
    }

    rest = text;
    ctx.line_no = 0;
    while (rest.len) {
        kstr line = kstr_next_line(&rest);
        ctx.line_no++;
        kstr probe = kstr_trim(line);
        kstr key = kstr_next_token(&probe);
        if (!kaji_key_applies(&key) || kstr_eq(key, KSTR("builddir"))) {
            continue; /* other-OS keys and the prepass lines */
        }
        if (!kaji_parse_line(&ctx, line)) {
            return 0;
        }
    }

    /* validate */
    for (int i = 0; i < k->target_count; i++) {
        kaji_target *v = &k->targets[i];
        if (!v->out[0]) {
            snprintf(err, (size_t)err_size, "target '%s' has no out", v->name);
            return 0;
        }
        if (v->in_count == 0) {
            snprintf(err, (size_t)err_size, "target '%s' has no in", v->name);
            return 0;
        }
        for (int d = 0; d < v->dep_count; d++) {
            int found = 0;
            for (int j = 0; j < k->target_count; j++) {
                if (strcmp(k->targets[j].name, v->deps[d]) == 0) {
                    found = 1;
                }
            }
            if (!found) {
                snprintf(err, (size_t)err_size, "target '%s' depends on unknown '%s'",
                         v->name, v->deps[d]);
                return 0;
            }
        }
    }
    return 1;
}

kaji *kaji_load(const char *config_path, char *err, int err_size) {
    char dummy[8];
    if (!err) {
        err = dummy;
        err_size = sizeof(dummy);
    }
    FILE *f = fopen(config_path, "rb");
    if (!f) {
        snprintf(err, (size_t)err_size, "cannot open '%s'", config_path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
        snprintf(err, (size_t)err_size, "cannot read '%s'", config_path);
        free(text);
        fclose(f);
        return NULL;
    }
    text[size] = 0;
    fclose(f);

    kaji *k = (kaji *)calloc(1, sizeof(kaji));
    if (!k) {
        free(text);
        snprintf(err, (size_t)err_size, "out of memory");
        return NULL;
    }
    kstr view = { text, (size_t)size };
    int ok = kaji_parse(k, view, err, err_size);
    free(text);
    if (!ok) {
        free(k);
        return NULL;
    }
    return k;
}

void kaji_free(kaji *k) {
    free(k);
}

/* ---- planning ---------------------------------------------------------- */

static kaji_target *kaji_find(kaji *k, kstr name) {
    for (int i = 0; i < k->target_count; i++) {
        if (kstr_eq_c(name, k->targets[i].name)) {
            return &k->targets[i];
        }
    }
    return NULL;
}

static int kaji_target_stale(const kaji_target *t) {
    unsigned long long out_t;
    if (!kaji_platform_mtime(t->out, &out_t)) {
        return 1;
    }
    for (int i = 0; i < t->in_count; i++) {
        unsigned long long in_t;
        if (!kaji_platform_mtime(t->ins[i], &in_t) || in_t > out_t) {
            return 1;
        }
    }
    for (int i = 0; i < t->also_count; i++) {
        unsigned long long a_t;
        if (kaji_platform_mtime(t->also[i], &a_t) && a_t > out_t) {
            return 1;
        }
    }
    for (int i = 0; i < t->obj_count; i++) {
        unsigned long long o_t;
        if (kaji_platform_mtime(t->obj[i], &o_t) && o_t > out_t) {
            return 1;
        }
    }
    return 0;
}

/* builds the compile/link command for shader/object/dll/exe targets */
static int kaji_command_for(kaji *k, const kaji_target *t,
                            char *cmd, size_t cmd_size, const char *out_path) {
    kstr_buf b;
    kstr_buf_init(&b, cmd, cmd_size);

    if (t->kind == KAJI_KIND_SHADER) {
        const char *glslc = kaji_tool(k, KSTR("glslc"));
        if (!glslc) {
            return 0;
        }
        kstr_buf_appendf(&b, "%s %s -o %s", glslc, t->ins[0], out_path);
        return !b.overflow;
    }

    kstr_buf_append_c(&b, kaji_tool(k, KSTR("cc")));
    if (t->kind == KAJI_KIND_OBJECT) {
        kstr_buf_append(&b, KSTR(" -c"));
    } else if (t->kind == KAJI_KIND_PCH) {
        kstr_buf_append(&b, KSTR(" -x c-header"));
    } else if (t->kind == KAJI_KIND_DLL) {
        kstr_buf_append(&b, KSTR(" -shared"));
    }
    if (kaji_platform_is_posix() && t->kind != KAJI_KIND_EXE) {
        kstr_buf_append(&b, KSTR(" -fPIC"));
    }
    for (int i = 0; i < t->flag_count; i++)    kstr_buf_appendf(&b, " %s", t->flag[i]);
    for (int i = 0; i < t->define_count; i++)  kstr_buf_appendf(&b, " -D%s", t->define[i]);
    for (int i = 0; i < t->in_count; i++)      kstr_buf_appendf(&b, " %s", t->ins[i]);
    for (int i = 0; i < t->obj_count; i++)     kstr_buf_appendf(&b, " %s", t->obj[i]);
    for (int i = 0; i < t->include_count; i++) kstr_buf_appendf(&b, " -I%s", t->include[i]);
    for (int i = 0; i < t->libdir_count; i++)  kstr_buf_appendf(&b, " -L%s", t->libdir[i]);
    for (int i = 0; i < t->lib_count; i++)     kstr_buf_appendf(&b, " -l%s", t->lib[i]);
    kstr_buf_appendf(&b, " -o %s", out_path);
    return !b.overflow;
}

static int kaji_add_step(kaji_run *run, const char *cmd, int copy_kind) {
    if (run->step_count >= KAJI_RUN_MAX_STEPS) {
        return 0;
    }
    snprintf(run->cmds[run->step_count], KAJI_RUN_CMD_MAX, "%s", cmd);
    run->copy_step[run->step_count] = copy_kind;
    run->step_count++;
    return 1;
}

static int kaji_plan_target(kaji *k, kaji_target *t, kaji_run *run, int *forced);

static int kaji_plan_deps(kaji *k, kaji_target *t, kaji_run *run, int *forced) {
    for (int i = 0; i < t->dep_count; i++) {
        kaji_target *d = kaji_find(k, kstr_from(t->deps[i]));
        if (!kaji_plan_target(k, d, run, forced)) {
            return 0;
        }
    }
    return 1;
}

static int kaji_plan_target(kaji *k, kaji_target *t, kaji_run *run, int *forced) {
    if (t->visited) {
        if (t->planned) {
            *forced = 1; /* rebuilt under another parent this run */
        }
        return 1;
    }
    if (t->visiting) {
        return 0; /* dependency cycle */
    }
    t->visiting = 1;
    int dep_forced = 0;
    if (!kaji_plan_deps(k, t, run, &dep_forced)) {
        return 0;
    }
    t->visiting = 0;
    t->visited = 1;

    /* a rebuilt dependency may not have touched disk yet (its step runs
       later), so being downstream of any planned step forces a rebuild */
    if (!dep_forced && !kaji_target_stale(t)) {
        return 1;
    }
    *forced = 1;
    t->planned = 1;

    kaji_platform_make_dirs_for(t->out);

    char cmd[KAJI_RUN_CMD_MAX];
    if (t->kind == KAJI_KIND_COPY) {
        snprintf(cmd, sizeof(cmd), "%s|%s", t->ins[0], t->out);
        return kaji_add_step(run, cmd, 1);
    }

    const char *out_path = t->out;
    if (t->kind == KAJI_KIND_DLL) {
        /* publish atomically: build to tmp, rename at run completion */
        snprintf(run->publish_out, sizeof(run->publish_out), "%s", t->out);
        snprintf(run->publish_tmp, sizeof(run->publish_tmp), "%s.tmp", t->out);
        out_path = run->publish_tmp;
    }
    if (!kaji_command_for(k, t, cmd, sizeof(cmd), out_path)) {
        return 0;
    }
    if (!kaji_add_step(run, cmd, 0)) {
        return 0;
    }
    for (int i = 0; i < t->post_count; i++) {
        char post[KAJI_RUN_CMD_MAX];
        snprintf(post, sizeof(post), "%s|%s", t->posts[i].from, t->posts[i].to);
        if (!kaji_add_step(run, post, t->posts[i].is_dir ? 2 : 1)) {
            return 0;
        }
    }
    return 1;
}

/* ---- running ----------------------------------------------------------- */

/* Starts the current step: copies run on a worker thread (a ship bundle's
   asset tree must never stall the frame loop), compiles/links as child
   processes -- both polled the same way. Publishes at the end. */
static int kaji_start_copy(kaji_run *run) {
    kstr s = kstr_from(run->cmds[run->step]);
    int sep = kstr_find(s, '|');
    if (sep < 0) {
        return 0;
    }
    char from[KAJI_PATH_MAX], to[KAJI_PATH_MAX];
    if (!kstr_copy(from, sizeof(from), kstr_slice(s, 0, (size_t)sep)) ||
        !kstr_copy(to, sizeof(to), kstr_slice(s, (size_t)sep + 1, s.len))) {
        return 0;
    }
    kaji_platform_make_dirs_for(to);
    return kaji_platform_copy_async(from, to,
                                    run->copy_step[run->step] == 2,
                                    &run->copy_thread);
}

static int kaji_advance(kaji_run *run) {
    if (run->step < run->step_count) {
        if (run->copy_step[run->step]) {
            return kaji_start_copy(run);
        }
        return kaji_platform_spawn(run->cmds[run->step],
                                   run->log_path[0] ? run->log_path : NULL,
                                   &run->proc);
    }
    if (run->publish_out[0]) {
        if (!kaji_platform_rename(run->publish_tmp, run->publish_out)) {
            return 0;
        }
        run->publish_out[0] = 0; /* publish once */
    }
    return 1;
}

int kaji_build_async(kaji *k, const char *target, kaji_run *run, int force) {
    if (run->active) {
        return 0;
    }
    kaji_target *t = kaji_find(k, kstr_from(target));
    if (!t) {
        return 0;
    }
    memset(run, 0, sizeof(*run));
    for (int i = 0; i < k->target_count; i++) {
        k->targets[i].visiting = 0;
        k->targets[i].visited = 0;
        k->targets[i].planned = 0;
    }
    snprintf(run->log_path, sizeof(run->log_path), "%s/kaji_%s.log",
             k->builddir, target);
    int forced = force ? 1 : 0;
    if (force) {
        /* plan deps normally, then plan the root unconditionally */
        int dep_forced = 0;
        t->visiting = 1;
        if (!kaji_plan_deps(k, t, run, &dep_forced)) {
            return 0;
        }
        t->visiting = 0;
        t->visited = 1;
        t->planned = 1;
        kaji_platform_make_dirs_for(t->out);
        char cmd[KAJI_RUN_CMD_MAX];
        if (t->kind == KAJI_KIND_COPY) {
            snprintf(cmd, sizeof(cmd), "%s|%s", t->ins[0], t->out);
            if (!kaji_add_step(run, cmd, 1)) {
                return 0;
            }
        } else {
            const char *out_path = t->out;
            if (t->kind == KAJI_KIND_DLL) {
                snprintf(run->publish_out, sizeof(run->publish_out), "%s", t->out);
                snprintf(run->publish_tmp, sizeof(run->publish_tmp), "%s.tmp", t->out);
                out_path = run->publish_tmp;
            }
            if (!kaji_command_for(k, t, cmd, sizeof(cmd), out_path)) {
                return 0;
            }
            if (!kaji_add_step(run, cmd, 0)) {
                return 0;
            }
            for (int i = 0; i < t->post_count; i++) {
                char post[KAJI_RUN_CMD_MAX];
                snprintf(post, sizeof(post), "%s|%s",
                         t->posts[i].from, t->posts[i].to);
                if (!kaji_add_step(run, post, t->posts[i].is_dir ? 2 : 1)) {
                    return 0;
                }
            }
        }
    } else if (!kaji_plan_target(k, t, run, &forced)) {
        return 0;
    }
    kaji_platform_make_dirs_for(run->log_path);
    kaji_platform_truncate(run->log_path);
    run->active = 1;
    if (!kaji_advance(run)) {
        run->active = 0;
        return 0;
    }
    return 1;
}

kaji_status kaji_run_poll(kaji *k, kaji_run *run) {
    (void)k;
    if (!run->active) {
        return KAJI_IDLE;
    }
    if (run->step >= run->step_count) {
        /* nothing was stale: instant success (publish ran in advance) */
        run->active = 0;
        run->exit_code = 0;
        return KAJI_DONE;
    }

    int step_ok;
    if (run->copy_step[run->step]) {
        int copy_ok = 0;
        if (!kaji_platform_copy_poll(run->copy_thread, &copy_ok)) {
            return KAJI_RUNNING;
        }
        kaji_platform_copy_close(run->copy_thread);
        run->copy_thread = NULL;
        step_ok = copy_ok;
        run->exit_code = copy_ok ? 0 : 1;
    } else {
        int exit_code = 0;
        if (!kaji_platform_poll(run->proc, &exit_code)) {
            return KAJI_RUNNING;
        }
        kaji_platform_close(run->proc);
        run->proc = NULL;
        step_ok = exit_code == 0;
        run->exit_code = exit_code;
    }
    if (!step_ok) {
        run->active = 0;
        return KAJI_FAILED;
    }

    run->step++;
    if (!kaji_advance(run)) {
        run->active = 0;
        run->exit_code = 1;
        return KAJI_FAILED;
    }
    if (run->step >= run->step_count) {
        run->active = 0;
        run->exit_code = 0;
        return KAJI_DONE;
    }
    return KAJI_RUNNING;
}

int kaji_build(kaji *k, const char *target, int force) {
    kaji_run run = { 0 };
    if (!kaji_build_async(k, target, &run, force)) {
        fprintf(stderr, "kaji: cannot plan/start target '%s'\n", target);
        return 1;
    }
    for (;;) {
        kaji_status s = kaji_run_poll(k, &run);
        if (s == KAJI_DONE) {
            return 0;
        }
        if (s == KAJI_FAILED || s == KAJI_IDLE) {
            fprintf(stderr, "kaji: target '%s' failed (exit %d), see %s\n",
                    target, run.exit_code, run.log_path);
            return 1;
        }
        kaji_platform_sleep_ms(5);
    }
}
