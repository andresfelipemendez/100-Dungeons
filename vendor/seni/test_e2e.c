// platform impl first: on windows it pulls in windows.h, which must come
// before utest.h so utest uses windows.h's LARGE_INTEGER
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
#include "seni_registry.h"
#include "seni_reload.h"

UTEST_MAIN()

static char* read_file(arena* a, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "fixture not found: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = allocate(a, (size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "short read on %s\n", path);
        fclose(f);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

// compiles src_path to build/<name>.<dll|so>, loads it. on gcc failure prints
// the source (code) + gcc stderr and returns NULL.
static void *compile_src_and_load(const char* src_path, const char* code, const char* name) {
    char lib_path[256], err_path[256];
    snprintf(lib_path, sizeof(lib_path), "build/%s.%s", name, dodai_lib_extension());
    snprintf(err_path, sizeof(err_path), "build/%s.err", name);
    if (dodai_compile_shared(src_path, lib_path, err_path, "-std=c89 -pedantic") != 0) {
        fprintf(stderr, "gcc failed for %s\ngenerated code:\n%s\n", src_path, code);
        FILE* e = fopen(err_path, "rb");
        if (e) {
            char line[512];
            while (fgets(line, sizeof(line), e)) fputs(line, stderr);
            fclose(e);
        }
        return NULL;
    }
    return dodai_lib_open(lib_path);
}

// writes code to build/<name>.c, compiles, loads. for hand-written sources
// (game dlls); generated migrations go through compile_migration_and_load.
static void *compile_and_load(const char* code, const char* name) {
    char src_path[256];
    dodai_make_dir("build");
    snprintf(src_path, sizeof(src_path), "build/%s.c", name);
    FILE* f = fopen(src_path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", src_path); return NULL; }
    fputs(code, f);
    fclose(f);
    return compile_src_and_load(src_path, code, name);
}

// generated migration TUs compile from the build/seni_out/migration_NNN.c
// audit log, so the file on disk is exactly what gcc consumed.
static void *compile_migration_and_load(const char* code, const char* name) {
    char src_path[256];
    if (seni_dump_migration(code, name, src_path, sizeof(src_path)) != 0) return NULL;
    return compile_src_and_load(src_path, code, name);
}

static int copy_file(const char* src, const char* dst) {
    char buf[4096];
    size_t n;
    FILE* in = fopen(src, "rb");
    FILE* out;
    if (!in) { fprintf(stderr, "cannot read %s\n", src); return 1; }
    out = fopen(dst, "wb");
    if (!out) { fclose(in); fprintf(stderr, "cannot write %s\n", dst); return 1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 0;
}

// snapshot the layout header to build/<name>_layout.h and resolve to an
// absolute path. within one compile, the preprocessor reads the layout
// header (#include) and the assembler reads it again (.incbin) at different
// moments -- if the file changes in between, the dll's embedded layout
// disagrees with its compiled layout, which is exactly the desync the embed
// exists to prevent. pointing both readers at a snapshot nothing overwrites
// mid-compile closes that window. absolute path because .incbin resolves
// relative to the compiler's cwd, not the source file.
static int snapshot_layout(const char* header_path, const char* name, char* abs_out, size_t cap) {
    char copy_path[256];
    dodai_make_dir("build");
    snprintf(copy_path, sizeof(copy_path), "build/%s_layout.h", name);
    if (copy_file(header_path, copy_path) != 0) return 1;
    return dodai_absolute_path(copy_path, abs_out, cap);
}

typedef void (*migrate_fn)(void* old_p, void* new_p, size_t count);

// pipeline: read both fixtures, diff, generate, compile, load, return migrate_<struct_name>
static migrate_fn build_migration(arena* a, const char* old_path, const char* new_path,
                                  const char* test_name, const char* struct_name, void ** out_mod) {
    char* old_header = read_file(a, old_path);
    char* new_header = read_file(a, new_path);
    if (!old_header || !new_header) return NULL;
    diff_result d = diff_structs(a, old_header, new_header);
    if (d.err) { fprintf(stderr, "diff error: %s\n", d.err); return NULL; }
    generate_result g = generate_migration(a, d.value);
    if (g.err) { fprintf(stderr, "generate error: %s\n", g.err); return NULL; }
    void *m = compile_migration_and_load(g.code, test_name);
    if (!m) return NULL;
    *out_mod = m;
    char sym[128];
    snprintf(sym, sizeof(sym), "migrate_%s", struct_name);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(m, sym);
    if (!fn) fprintf(stderr, "symbol not found: %s\n", sym);
    return fn;
}

UTEST(e2e, add_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v1.h", "fixtures/enemy_v2.h",
                                         "add_field", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));  // poison: catches missed zero-init

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
        ASSERT_EQ(0, new_block[i].health);
    }
    dodai_lib_close(mod);
}

UTEST(e2e, remove_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v2.h", "fixtures/enemy_v1.h",
                                         "remove_field", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; int health; } enemy_v2;
    typedef struct { float x, y; } enemy_v1;

    enemy_v2 old_block[2] = { {1.0f, 2.0f, 99}, {3.0f, 4.0f, 50} };
    enemy_v1 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
    }
    dodai_lib_close(mod);
}

UTEST(e2e, reorder_fields) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/reorder_v1.h", "fixtures/reorder_v2.h",
                                         "reorder_fields", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x; int health; double speed; } enemy_v1;
    typedef struct { double speed; float x; int health; } enemy_v2;

    enemy_v1 old_block[2] = { {1.5f, 7, 2.25}, {3.5f, 9, 4.75} };
    enemy_v2 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].health, new_block[i].health);
        ASSERT_EQ(old_block[i].speed, new_block[i].speed);
    }
    dodai_lib_close(mod);
}

UTEST(e2e, multiple_structs) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = read_file(&a, "fixtures/world_v1.h");
    char* new_header = read_file(&a, "fixtures/world_v2.h");
    ASSERT_TRUE(old_header != NULL);
    ASSERT_TRUE(new_header != NULL);
    diff_result d = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "multiple_structs");
    ASSERT_TRUE(mod != NULL);

    migrate_fn migrate_enemy_fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_enemy");
    migrate_fn migrate_player_fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_player");
    ASSERT_TRUE(migrate_enemy_fn != NULL);
    ASSERT_TRUE(migrate_player_fn != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;
    typedef struct { int score; } player_v1;
    typedef struct { int score, level; } player_v2;

    enemy_v1 old_enemies[2] = { {1.0f, 2.0f}, {3.0f, 4.0f} };
    enemy_v2 new_enemies[2];
    memset(new_enemies, 0xCD, sizeof(new_enemies));
    migrate_enemy_fn(old_enemies, new_enemies, 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_enemies[i].x, new_enemies[i].x);
        ASSERT_EQ(old_enemies[i].y, new_enemies[i].y);
        ASSERT_EQ(0, new_enemies[i].health);
    }

    player_v1 old_players[1] = { {1234} };
    player_v2 new_players[1];
    memset(new_players, 0xCD, sizeof(new_players));
    migrate_player_fn(old_players, new_players, 1);
    ASSERT_EQ(1234, new_players[0].score);
    ASSERT_EQ(0, new_players[0].level);

    dodai_lib_close(mod);
}

// hot-reload scenario: the old header is not on disk anymore (the save
// overwrote it) — it lives embedded inside the currently-loaded game dll.
// build a fake game dll that embeds enemy_v1.h via seni_embed.h, pull the
// layout back out through the seni_layout symbol, diff against the new
// header file, migrate.
UTEST(e2e, layout_embedded_in_dll) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));

    char layout_abs[512];
    char game_src[1024];
    ASSERT_EQ(0, snapshot_layout("fixtures/enemy_v1.h", "game_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src),
        "#include \"../seni_embed.h\"\n"  // generated source lives in build/
        "SENI_EMBED_LAYOUT(\"%s\");\n", layout_abs);
    void *game = compile_and_load(game_src, "game_v1");
    ASSERT_TRUE(game != NULL);

    const char** layout_p = (const char**)dodai_lib_symbol(game, "seni_layout");
    ASSERT_TRUE(layout_p != NULL);
    const char* old_header = *layout_p;

    // embedded bytes must be identical to the file gcc compiled against
    char* file_header = read_file(&a, "fixtures/enemy_v1.h");
    ASSERT_TRUE(file_header != NULL);
    ASSERT_STREQ(file_header, old_header);

    char* new_header = read_file(&a, "fixtures/enemy_v2.h");
    ASSERT_TRUE(new_header != NULL);
    diff_result d = diff_structs(&a, (char*)old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "layout_embedded_migration");
    ASSERT_TRUE(mod != NULL);
    migrate_fn migrate = (migrate_fn)dodai_lib_symbol(mod, "migrate_enemy");
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
        ASSERT_EQ(0, new_block[i].health);
    }
    dodai_lib_close(mod);
    dodai_lib_close(game);
}

UTEST(e2e, array_resize) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/thing_v1.h", "fixtures/thing_v2.h",
                                         "array_resize", "thing", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float pos[4]; int id; } thing_v1;
    typedef struct { float pos[2]; int id; float vel[3]; } thing_v2;

    thing_v1 old_block[2] = {
        { {1.0f, 2.0f, 3.0f, 4.0f}, 10 },
        { {5.0f, 6.0f, 7.0f, 8.0f}, 20 },
    };
    thing_v2 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].pos[0], new_block[i].pos[0]);
        ASSERT_EQ(old_block[i].pos[1], new_block[i].pos[1]);
        ASSERT_EQ(old_block[i].id, new_block[i].id);
        for (int j = 0; j < 3; j++) {
            ASSERT_EQ(0.0f, new_block[i].vel[j]);
        }
    }
    dodai_lib_close(mod);
}

static int write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fputs(content, f);
    fclose(f);
    return 0;
}

typedef void (*game_fn)(void* state, size_t count);

/* full engine lifecycle: host owns the state memory, game code lives in a
   dll that gets rebuilt and hot-reloaded after the struct layout changes.

   1. working header build/game_current.h starts as game_v1.h
   2. game dll v1 (embeds layout, has game_init/game_update) loads, ticks twice
   3. the "save": game_current.h is OVERWRITTEN with the v2 layout --
      from here the old layout exists only inside the loaded v1 dll
   4. game dll v2 builds from the new header, with update logic that uses
      the new fields (health, trail[2])
   5. reload: old layout read from v1 dll's seni_layout symbol, new layout
      from v2 dll's, diff -> generate -> compile migration dll -> migrate
      host state into a new block, unload v1
   6. v2's game_update ticks the MIGRATED memory; correct values coming out
      of v2's view of the block is the layout proof */
UTEST(e2e, full_hot_reload) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));

    /* host's view of the two layouts */
    typedef struct { float x, y; float speed; } host_enemy_v1;
    typedef struct { float x, y; float speed; int health; float trail[2]; } host_enemy_v2;

    /* both the #include and the .incbin point at a per-dll snapshot of the
       working header (see snapshot_layout) -- the live game_current.h may be
       overwritten by a "save" while a compile is in flight */
    static const char* game_v1_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_init(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) {\n"
        "        e[i].x = (float)(i * 10);\n"
        "        e[i].y = 0.0f;\n"
        "        e[i].speed = (float)(1 + i);\n"
        "    }\n"
        "}\n"
        "GEXPORT void game_update(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) { e[i].x += e[i].speed; e[i].y += 1.0f; }\n"
        "}\n";

    static const char* game_v2_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_update(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) {\n"
        "        e[i].x += e[i].speed;\n"
        "        e[i].y += 1.0f;\n"
        "        e[i].health += 1;\n"
        "        e[i].trail[0] = e[i].x;\n"
        "        e[i].trail[1] = e[i].y;\n"
        "    }\n"
        "}\n";

    char layout_abs[512];
    char game_src[2048];

    /* 1. working header starts at v1 */
    char* v1_header = read_file(&a, "fixtures/game_v1.h");
    ASSERT_TRUE(v1_header != NULL);
    dodai_make_dir("build");
    ASSERT_EQ(0, write_file("build/game_current.h", v1_header));

    /* 2. build + load game v1, init, tick twice */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotgame_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), game_v1_fmt, layout_abs, layout_abs);
    void *game_v1 = compile_and_load(game_src, "hotgame_v1");
    ASSERT_TRUE(game_v1 != NULL);
    game_fn init_v1 = (game_fn)dodai_lib_symbol(game_v1, "game_init");
    game_fn update_v1 = (game_fn)dodai_lib_symbol(game_v1, "game_update");
    ASSERT_TRUE(init_v1 != NULL);
    ASSERT_TRUE(update_v1 != NULL);

    host_enemy_v1 state_v1[3];
    init_v1(state_v1, 3);
    update_v1(state_v1, 3);
    update_v1(state_v1, 3);
    /* after 2 ticks: x = 10*i + 2*(1+i), y = 2 */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), state_v1[i].x);
        ASSERT_EQ(2.0f, state_v1[i].y);
    }

    /* 3. the save: overwrite the working header with the v2 layout.
       old layout is now ONLY inside the loaded v1 dll. */
    char* v2_header = read_file(&a, "fixtures/game_v2.h");
    ASSERT_TRUE(v2_header != NULL);
    ASSERT_EQ(0, write_file("build/game_current.h", v2_header));

    /* 4. rebuild: game dll v2 embeds the new layout */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotgame_v2", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), game_v2_fmt, layout_abs, layout_abs);
    void *game_v2 = compile_and_load(game_src, "hotgame_v2");
    ASSERT_TRUE(game_v2 != NULL);
    game_fn update_v2 = (game_fn)dodai_lib_symbol(game_v2, "game_update");
    ASSERT_TRUE(update_v2 != NULL);

    /* 5. reload: diff the layouts the two dlls were actually built with */
    const char** old_layout_p = (const char**)dodai_lib_symbol(game_v1, "seni_layout");
    const char** new_layout_p = (const char**)dodai_lib_symbol(game_v2, "seni_layout");
    ASSERT_TRUE(old_layout_p != NULL);
    ASSERT_TRUE(new_layout_p != NULL);

    diff_result d = diff_structs(&a, (char*)*old_layout_p, (char*)*new_layout_p);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *migration = compile_migration_and_load(g.code, "hotgame_migration");
    ASSERT_TRUE(migration != NULL);
    migrate_fn migrate = (migrate_fn)dodai_lib_symbol(migration, "migrate_enemy");
    ASSERT_TRUE(migrate != NULL);

    host_enemy_v2 state_v2[3];
    memset(state_v2, 0xCD, sizeof(state_v2));
    migrate(state_v1, state_v2, 3);
    dodai_lib_close(game_v1); /* old game code gone, state survived */

    /* migrated: x/y/speed carried over, health zeroed, trail zeroed */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), state_v2[i].x);
        ASSERT_EQ(2.0f, state_v2[i].y);
        ASSERT_EQ((float)(1 + i), state_v2[i].speed);
        ASSERT_EQ(0, state_v2[i].health);
        ASSERT_EQ(0.0f, state_v2[i].trail[0]);
        ASSERT_EQ(0.0f, state_v2[i].trail[1]);
    }

    /* 6. v2 game code ticks the migrated memory */
    update_v2(state_v2, 3);
    for (int i = 0; i < 3; i++) {
        float expect_x = (float)(10 * i + 3 * (1 + i));
        ASSERT_EQ(expect_x, state_v2[i].x);
        ASSERT_EQ(3.0f, state_v2[i].y);
        ASSERT_EQ(1, state_v2[i].health);
        ASSERT_EQ(expect_x, state_v2[i].trail[0]);
        ASSERT_EQ(3.0f, state_v2[i].trail[1]);
    }

    dodai_lib_close(migration);
    dodai_lib_close(game_v2);
}

/* registry-driven reload: who calls migrate, and with which pointers.
   the exe compiles in ZERO struct layout knowledge -- the game dll carves
   its arrays through seni_carve (recording name/offset/count/stride in the
   registry at the block base), and on reload seni_migrate_block walks that
   registry: migrate fn + strides from the migration dll, locations from the
   data itself. host typedefs below exist only to assert values from the
   test; the reload path never touches them. */
UTEST(e2e, registry_driven_reload) {
    char buf[16384];
    arena a;
    char layout_abs[512];
    char game_src[4096];
    size_t cap = 8192;
    void* old_block = malloc(cap);
    void* new_block = malloc(cap);

    /* game code: carves through the registry, finds its array again by name.
       strict c89 (dodai_compile_shared) -- no // comments. */
    static const char* reg_game_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "#include \"../seni_registry.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_boot(void* block) {\n"
        "    enemy* e = (enemy*)seni_carve(block, \"enemy\", 5, sizeof(enemy));\n"
        "    size_t i;\n"
        "    if (!e) return;\n"
        "    for (i = 0; i < 5; i++) {\n"
        "        e[i].x = (float)(i * 10);\n"
        "        e[i].y = 0.0f;\n"
        "        e[i].speed = (float)(1 + i);\n"
        "    }\n"
        "}\n"
        "GEXPORT void game_update(void* block) {\n"
        "    seni_array_desc* d = seni_registry_find(block, \"enemy\");\n"
        "    enemy* e;\n"
        "    size_t i;\n"
        "    if (!d) return;\n"
        "    e = (enemy*)((char*)block + d->offset);\n"
        "    for (i = 0; i < d->count; i++) {%s}\n"
        "}\n";
    static const char* v1_tick =
        " e[i].x += e[i].speed; e[i].y += 1.0f; ";
    static const char* v2_tick =
        " e[i].x += e[i].speed; e[i].y += 1.0f;"
        " e[i].health += 1; e[i].trail[0] = e[i].x; e[i].trail[1] = e[i].y; ";

    /* host's view, for assertions only */
    typedef struct { float x, y; float speed; } host_enemy_v1;
    typedef struct { float x, y; float speed; int health; float trail[2]; } host_enemy_v2;
    typedef void (*block_fn)(void* block);

    ASSERT_TRUE(old_block != NULL);
    ASSERT_TRUE(new_block != NULL);
    create_arena(&a, buf, sizeof(buf));

    /* 1. working header at v1; host inits an empty self-describing block */
    char* v1_header = read_file(&a, "fixtures/game_v1.h");
    ASSERT_TRUE(v1_header != NULL);
    dodai_make_dir("build");
    ASSERT_EQ(0, write_file("build/game_current.h", v1_header));
    seni_registry_init(old_block, cap);

    /* 2. game v1 carves and runs */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotreg_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), reg_game_fmt, layout_abs, layout_abs, v1_tick);
    void *game_v1 = compile_and_load(game_src, "hotreg_v1");
    ASSERT_TRUE(game_v1 != NULL);
    block_fn boot_v1 = (block_fn)dodai_lib_symbol(game_v1, "game_boot");
    block_fn update_v1 = (block_fn)dodai_lib_symbol(game_v1, "game_update");
    ASSERT_TRUE(boot_v1 != NULL);
    ASSERT_TRUE(update_v1 != NULL);
    boot_v1(old_block);
    update_v1(old_block);
    update_v1(old_block);

    seni_array_desc* od = seni_registry_find(old_block, "enemy");
    ASSERT_TRUE(od != NULL);
    ASSERT_EQ((size_t)5, od->count);
    ASSERT_EQ(sizeof(host_enemy_v1), od->stride);
    host_enemy_v1* e1 = (host_enemy_v1*)((char*)old_block + od->offset);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), e1[i].x);
        ASSERT_EQ(2.0f, e1[i].y);
    }

    /* 3. the save, then game v2 */
    char* v2_header = read_file(&a, "fixtures/game_v2.h");
    ASSERT_TRUE(v2_header != NULL);
    ASSERT_EQ(0, write_file("build/game_current.h", v2_header));
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotreg_v2", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), reg_game_fmt, layout_abs, layout_abs, v2_tick);
    void *game_v2 = compile_and_load(game_src, "hotreg_v2");
    ASSERT_TRUE(game_v2 != NULL);
    block_fn update_v2 = (block_fn)dodai_lib_symbol(game_v2, "game_update");
    ASSERT_TRUE(update_v2 != NULL);

    /* 4. diff the embedded layouts, build the migration dll */
    const char** old_layout_p = (const char**)dodai_lib_symbol(game_v1, "seni_layout");
    const char** new_layout_p = (const char**)dodai_lib_symbol(game_v2, "seni_layout");
    ASSERT_TRUE(old_layout_p != NULL);
    ASSERT_TRUE(new_layout_p != NULL);
    diff_result d = diff_structs(&a, (char*)*old_layout_p, (char*)*new_layout_p);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *migration = compile_migration_and_load(g.code, "hotreg_migration");
    ASSERT_TRUE(migration != NULL);

    /* 5. the actual answer to "who calls migrate": the host, blind,
       driven entirely by the old block's registry + the migration dll */
    memset(new_block, 0xCD, cap);
    ASSERT_EQ(0, seni_migrate_block(old_block, new_block, cap, migration));
    dodai_lib_close(game_v1);

    seni_array_desc* nd = seni_registry_find(new_block, "enemy");
    ASSERT_TRUE(nd != NULL);
    ASSERT_EQ((size_t)5, nd->count);
    ASSERT_EQ(sizeof(host_enemy_v2), nd->stride);
    host_enemy_v2* e2 = (host_enemy_v2*)((char*)new_block + nd->offset);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), e2[i].x);
        ASSERT_EQ(2.0f, e2[i].y);
        ASSERT_EQ((float)(1 + i), e2[i].speed);
        ASSERT_EQ(0, e2[i].health);
        ASSERT_EQ(0.0f, e2[i].trail[0]);
        ASSERT_EQ(0.0f, e2[i].trail[1]);
    }

    /* 6. v2 finds the migrated array through the new registry and ticks it */
    update_v2(new_block);
    for (int i = 0; i < 5; i++) {
        float expect_x = (float)(10 * i + 3 * (1 + i));
        ASSERT_EQ(expect_x, e2[i].x);
        ASSERT_EQ(3.0f, e2[i].y);
        ASSERT_EQ(1, e2[i].health);
        ASSERT_EQ(expect_x, e2[i].trail[0]);
        ASSERT_EQ(3.0f, e2[i].trail[1]);
    }

    dodai_lib_close(migration);
    dodai_lib_close(game_v2);
    free(old_block);
    free(new_block);
}

/* rename through the annotation channel: the data actually moves */
UTEST(e2e, rename_via_annotation) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } lamp;",
        "typedef struct { float x; int light_count SENI_WAS(num_lights); } lamp;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "rename_migration");
    ASSERT_TRUE(mod != NULL);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_lamp");
    ASSERT_TRUE(fn != NULL);

    typedef struct { float x; int num_lights; } lamp_v1;
    typedef struct { float x; int light_count; } lamp_v2;
    lamp_v1 old_block[3] = { {1.0f, 7}, {2.0f, 8}, {3.0f, 9} };
    lamp_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));
    fn(old_block, new_block, 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].num_lights, new_block[i].light_count);
    }
    dodai_lib_close(mod);
}

UTEST(e2e, identical) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v1.h", "fixtures/enemy_v1.h",
                                         "identical", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v1 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
    }
    dodai_lib_close(mod);
}
