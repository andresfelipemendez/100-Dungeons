/* cache behavior benchmark, run under valgrind --tool=cachegrind by perf.sh.
   two hot paths:
   - parse_header over a 256-struct header (parser locality)
   - a generated migration over 1M elements (the memory-bound reload path)
   the gcc child process spawned to build the migration dll is not
   instrumented (cachegrind does not follow children by default). */

#include "../dodai/dodai.h"
#ifdef _WIN32
#include "../dodai/dodai_windows.c"
#else
#include "../dodai/dodai_posix.c"
#endif
#include "seni.h"
#include "arena.h"
#include "arena.c"
#include "seni.c"
#include "seni_dump.h"
#include <stdlib.h>
#include <stdio.h>

#define BIG_STRUCTS 256
#define MIGRATE_N 1000000
#define PARSE_ITERS 20
#define MIGRATE_PASSES 5

static char big_h[524288];
static char big_arena[8 * 1024 * 1024];

static void cap_name(char* out, const char* prefix, unsigned long idx) {
    int n = sprintf(out, "%s%02lu_", prefix, idx);
    while (n < 64) out[n++] = 'x';
    out[64] = '\0';
}

static void emit_big(char* buf) {
    int o = 0;
    unsigned long s, f;
    char name[65];
    for (s = 0; s < BIG_STRUCTS; s++) {
        cap_name(name, "s", s);
        o += sprintf(&buf[o], "typedef struct {\n");
        for (f = 0; f < 16; f++) {
            char fname[65];
            cap_name(fname, "f", f);
            o += sprintf(&buf[o], "    int %s[%d];\n", fname, 64);
        }
        o += sprintf(&buf[o], "} %s;\n", name);
    }
    buf[o] = '\0';
}

typedef struct { float x, y; } enemy_a;
typedef struct { float x, y; int health; float trail[2]; } enemy_b;
typedef void (*migrate_fn)(void* old_p, void* new_p, size_t count);

int main(void) {
    int iter;
    size_t i;
    arena a;
    diff_result d;
    generate_result g;
    void *mod;
    migrate_fn fn;
    enemy_a* old_blk;
    enemy_b* new_blk;
    char src_path[256];
    char lib_path[128];
    char err_path[128];
    volatile float sink = 0.0f;

    /* ---- hot path 1: parser over a big header ---- */
    emit_big(big_h);
    for (iter = 0; iter < PARSE_ITERS; iter++) {
        parse_result r;
        create_arena(&a, big_arena, sizeof(big_arena));
        r = parse_header(&a, big_h);
        if (r.err) { fprintf(stderr, "parse: %s\n", r.err); return 1; }
        sink += (float)r.value.struct_count;
    }

    /* ---- build the migration dll (gcc child: not instrumented) ---- */
    create_arena(&a, big_arena, sizeof(big_arena));
    d = diff_structs(&a,
        "typedef struct { float x, y; } enemy;",
        "typedef struct { float x, y; int health; float trail[2]; } enemy;");
    if (d.err) { fprintf(stderr, "diff: %s\n", d.err); return 1; }
    g = generate_migration(&a, d.value);
    if (g.err) { fprintf(stderr, "generate: %s\n", g.err); return 1; }
    /* migration TU compiles from the build/seni_out audit log */
    if (seni_dump_migration(g.code, "bench_cache", src_path, sizeof(src_path)) != 0) return 1;
    sprintf(lib_path, "build/bench_cache.%s", dodai_lib_extension());
    sprintf(err_path, "build/bench_cache.err");
    if (dodai_compile_shared(michi_from_cstr(src_path), michi_from_cstr(lib_path), michi_from_cstr(err_path), "-std=c89 -pedantic") != 0) return 1;
    mod = dodai_lib_open(michi_from_cstr(lib_path));
    if (!mod) return 1;
    fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_enemy");
    if (!fn) return 1;

    /* ---- hot path 2: migrate 1M elements, several passes ---- */
    old_blk = (enemy_a*)malloc(MIGRATE_N * sizeof(enemy_a));
    new_blk = (enemy_b*)malloc(MIGRATE_N * sizeof(enemy_b));
    if (!old_blk || !new_blk) return 1;
    for (i = 0; i < MIGRATE_N; i++) {
        old_blk[i].x = (float)(i & 0xffff);
        old_blk[i].y = (float)(i & 0xff);
    }
    for (iter = 0; iter < MIGRATE_PASSES; iter++) {
        fn(old_blk, new_blk, MIGRATE_N);
        sink += new_blk[MIGRATE_N - 1].x;
    }

    dodai_lib_close(mod);
    free(old_blk);
    free(new_blk);
    fprintf(stderr, "bench done (sink %f)\n", sink);
    return 0;
}
