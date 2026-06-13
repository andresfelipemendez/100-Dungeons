/* stress suite: scale ceilings, exhaustion sweeps, throughput, and long
   migration chains. complements test_fuzz.c (randomized inputs) with
   deterministic worst-cases. */

#include "../dodai/dodai.h"
#ifdef _WIN32
#include "../dodai/dodai_windows.c"
#else
#include "../dodai/dodai_posix.c"
#endif
#include "utest.h"
#include "seni.h"
#include "arena.h"
#include "arena.c"
#include "seni.c"
#include "seni_dump.h"
#include <stdlib.h>
#include <time.h>

UTEST_MAIN()

/* migration TUs compile from the build/seni_out/migration_NNN.c audit log */
static void *compile_and_load_st(const char* code, const char* name) {
    char src_path[256];
    char lib_path[256];
    char err_path[256];
    if (seni_dump_migration(code, name, src_path, sizeof(src_path)) != 0) return NULL;
    sprintf(lib_path, "build/%s.%s", name, dodai_lib_extension());
    sprintf(err_path, "build/%s.err", name);
    if (dodai_compile_shared(ito_from(src_path), ito_from(lib_path), ito_from(err_path), "-std=c89 -pedantic") != 0) {
        fprintf(stderr, "gcc failed for %s, generated code:\n%s\n", src_path, code);
        return NULL;
    }
    return dodai_lib_open(ito_from(lib_path));
}

typedef void (*migrate_fn)(void* old_p, void* new_p, size_t count);

/* ---- ceiling: 256 structs x 16 fields, all names at the 64-char cap ----- */

#define BIG_STRUCTS 256

static char big_h1[524288];
static char big_h2[524288];
static char big_arena[8 * 1024 * 1024];

static void cap_name(char* out, const char* prefix, unsigned long idx) {
    int n = sprintf(out, "%s%02lu_", prefix, idx);
    while (n < 64) out[n++] = 'x';
    out[64] = '\0';
}

static void emit_big(char* buf, int extra_field_per_struct) {
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
        if (extra_field_per_struct) {
            char fname[65];
            cap_name(fname, "g", s);
            o += sprintf(&buf[o], "    double %s;\n", fname);
        }
        o += sprintf(&buf[o], "} %s;\n", name);
    }
    buf[o] = '\0';
}

UTEST(stress, max_structs_max_names) {
    arena a;
    diff_result d;
    generate_result g;
    char want[80];
    emit_big(big_h1, 0);
    emit_big(big_h2, 1); /* v2 adds one double per struct */
    create_arena(&a, big_arena, sizeof(big_arena));
    d = diff_structs(&a, big_h1, big_h2);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)BIG_STRUCTS, d.value.struct_count);
    ASSERT_EQ((size_t)17, d.value.structs[BIG_STRUCTS - 1].ops_count);
    g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    cap_name(want + 8, "s", BIG_STRUCTS - 1);
    memcpy(want, "migrate_", 8);
    ASSERT_TRUE(strstr(g.code, want) != NULL);
    fprintf(stderr, "stress.max: header %lu bytes, generated %lu bytes, arena used %lu\n",
            (unsigned long)strlen(big_h1), (unsigned long)strlen(g.code), (unsigned long)a.offset);
}

/* ---- exhaustion: every arena size from 0..4096 must succeed or error,
        never crash or corrupt. catches a missing OOM check at any single
        allocation site. ----------------------------------------------- */

UTEST(stress, arena_exhaustion_sweep) {
    static char sweep_buf[4096];
    char* old_header = "typedef struct { float x, y; int health; } enemy;";
    char* new_header = "typedef struct { float x, y; int health; float trail[2]; } enemy;";
    size_t size;
    int successes = 0;
    for (size = 0; size <= sizeof(sweep_buf); size++) {
        arena a;
        diff_result d;
        create_arena(&a, sweep_buf, size);
        d = diff_structs(&a, old_header, new_header);
        if (!d.err) {
            generate_result g = generate_migration(&a, d.value);
            if (!g.err) {
                ASSERT_TRUE(g.code != NULL);
                ASSERT_TRUE(strstr(g.code, "migrate_enemy") != NULL);
                successes++;
            }
        }
    }
    ASSERT_TRUE(successes > 0); /* the full-size run must have made it through */
}

/* ---- throughput: parse a realistic header many times ------------------- */

UTEST(stress, parse_throughput) {
    static char buf[16384];
    char* header =
        "typedef struct { float x, y, z; float vel[3]; int health; } enemy;\n"
        "struct player { char name[32]; double gold; int level; };\n"
        "typedef struct { int ids[64]; float weights[64]; } inventory;\n";
    int iter;
    clock_t t0 = clock();
    double secs;
    for (iter = 0; iter < 20000; iter++) {
        arena a;
        parse_result r;
        create_arena(&a, buf, sizeof(buf));
        r = parse_header(&a, header);
        ASSERT_FALSE(r.err);
        ASSERT_EQ((size_t)3, r.value.struct_count);
    }
    secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "stress.parse_throughput: 20000 parses in %.3fs (%.0f/s)\n",
            secs, secs > 0 ? 20000.0 / secs : 0.0);
}

/* ---- long dev session: 500 reload cycles migrating back and forth ------ */

static const char* H_A = "typedef struct { float x, y; } enemy;";
static const char* H_B = "typedef struct { float x, y; int health; float trail[2]; } enemy;";

typedef struct { float x, y; } enemy_a;
typedef struct { float x, y; int health; float trail[2]; } enemy_b;

static migrate_fn build_dir(arena* a, const char* from, const char* to,
                            const char* name, void **out_mod) {
    diff_result d = diff_structs(a, (char*)from, (char*)to);
    generate_result g;
    void *m;
    if (d.err) { fprintf(stderr, "diff: %s\n", d.err); return NULL; }
    g = generate_migration(a, d.value);
    if (g.err) { fprintf(stderr, "generate: %s\n", g.err); return NULL; }
    m = compile_and_load_st(g.code, name);
    if (!m) return NULL;
    *out_mod = m;
    return (migrate_fn)dodai_lib_symbol(m, "migrate_enemy");
}

UTEST(stress, migration_chain_500_cycles) {
    static char ab_arena[16384];
    static char ba_arena[16384];
    static enemy_a blk_a[256];
    static enemy_b blk_b[256];
    arena aa, ab;
    void *mod_ab = NULL, *mod_ba = NULL;
    migrate_fn a_to_b, b_to_a;
    int i, cycle;

    create_arena(&aa, ab_arena, sizeof(ab_arena));
    create_arena(&ab, ba_arena, sizeof(ba_arena));
    a_to_b = build_dir(&aa, H_A, H_B, "stress_ab", &mod_ab);
    b_to_a = build_dir(&ab, H_B, H_A, "stress_ba", &mod_ba);
    ASSERT_TRUE(a_to_b != NULL);
    ASSERT_TRUE(b_to_a != NULL);

    for (i = 0; i < 256; i++) {
        blk_a[i].x = (float)i * 1.5f;
        blk_a[i].y = (float)i * 2.5f;
    }
    for (cycle = 0; cycle < 500; cycle++) {
        memset(blk_b, 0xCD, sizeof(blk_b));
        a_to_b(blk_a, blk_b, 256);
        memset(blk_a, 0xCD, sizeof(blk_a));
        b_to_a(blk_b, blk_a, 256);
        /* spot-check every cycle, cheap */
        ASSERT_EQ(0.0f, blk_a[0].x);
        ASSERT_EQ(255.0f * 1.5f, blk_a[255].x);
        ASSERT_EQ(0, blk_b[128].health); /* zeroed on every a->b */
    }
    /* full integrity after 500 round trips */
    for (i = 0; i < 256; i++) {
        ASSERT_EQ((float)i * 1.5f, blk_a[i].x);
        ASSERT_EQ((float)i * 2.5f, blk_a[i].y);
    }
    dodai_lib_close(mod_ab);
    dodai_lib_close(mod_ba);
}

/* ---- repeated load/unload of the same dll ------------------------------ */

UTEST(stress, load_unload_100_cycles) {
    static char arena_buf[16384];
    arena a;
    void *first = NULL;
    migrate_fn fn;
    char lib_path[256];
    int i;
    create_arena(&a, arena_buf, sizeof(arena_buf));
    fn = build_dir(&a, H_A, H_B, "stress_reload", &first);
    ASSERT_TRUE(fn != NULL);
    dodai_lib_close(first);
    sprintf(lib_path, "build/stress_reload.%s", dodai_lib_extension());
    for (i = 0; i < 100; i++) {
        void *m = dodai_lib_open(ito_from(lib_path));
        ASSERT_TRUE(m != NULL);
        ASSERT_TRUE(dodai_lib_symbol(m, "migrate_enemy") != NULL);
        dodai_lib_close(m);
    }
}

/* ---- 200k-element block through a generated migration ------------------ */

UTEST(stress, migrate_200k_elements) {
    static char arena_buf[16384];
    arena a;
    void *mod = NULL;
    migrate_fn fn;
    size_t n = 200000;
    enemy_a* old_blk = (enemy_a*)malloc(n * sizeof(enemy_a));
    enemy_b* new_blk = (enemy_b*)malloc(n * sizeof(enemy_b));
    size_t i;
    clock_t t0;
    double secs;
    ASSERT_TRUE(old_blk != NULL);
    ASSERT_TRUE(new_blk != NULL);
    create_arena(&a, arena_buf, sizeof(arena_buf));
    fn = build_dir(&a, H_A, H_B, "stress_large", &mod);
    ASSERT_TRUE(fn != NULL);

    for (i = 0; i < n; i++) {
        old_blk[i].x = (float)(i % 8191);
        old_blk[i].y = (float)(i % 127);
    }
    memset(new_blk, 0xCD, n * sizeof(enemy_b));
    t0 = clock();
    fn(old_blk, new_blk, n);
    secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "stress.200k: migrated %lu elements (%.1f MB) in %.4fs\n",
            (unsigned long)n, (double)(n * sizeof(enemy_b)) / 1048576.0, secs);

    for (i = 0; i < n; i += 9973) { /* prime stride sampling */
        ASSERT_EQ((float)(i % 8191), new_blk[i].x);
        ASSERT_EQ((float)(i % 127), new_blk[i].y);
        ASSERT_EQ(0, new_blk[i].health);
    }
    ASSERT_EQ((float)((n - 1) % 8191), new_blk[n - 1].x); /* last element exact */
    dodai_lib_close(mod);
    free(old_blk);
    free(new_blk);
}
