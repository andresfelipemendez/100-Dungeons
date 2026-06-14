/* Unit tests for tests_gen: the lib_test table -> kaji cfg emitter. Pure, no
   filesystem; same harness shape as test_build_manifest. */
#include "../../lib/seni/utest.h"
#include "build_manifest.h"
#include "tests_gen.h"
#include "../../lib/seni/arena.c"
#include "build_manifest.c"
#include "tests_gen.c"

UTEST_MAIN()

static b32 setup(arena *a, const char *manifest, BuildManifest *m,
                 BuildPlatform *p, ito *err) {
    char *txt = arena_copy_string(a, manifest, strlen(manifest));
    ito text;
    text.ptr = txt;
    text.len = strlen(manifest);
    if (!build_manifest_parse(a, text, m, err)) return 0;
    return build_manifest_select(m, ITO("mac"), p, err);
}

static const char *MANIFEST =
    "platform mac builddir=build-mac exe= dll=.so dodai=dodai_posix.c"
    " rpath=@loader_path sdl=fetch\n"
    "lib_test horu std=c89 src=horu.c link=m\n";

UTEST(tests_gen, horu_target) {
    static char buf[1u << 16];
    char cfg[1u << 14];
    arena a; BuildManifest m; BuildPlatform p; ito err = {0,0};
    ito_buf out;
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(setup(&a, MANIFEST, &m, &p, &err));

    ito_buf_init(&out, cfg, sizeof(cfg));
    tests_gen_emit(&out, &m, &p, "/repo/meikyu", "gcc", 0);
    ASSERT_FALSE(out.overflow);

    ASSERT_TRUE(strstr(cfg, "builddir build-mac") != NULL);
    ASSERT_TRUE(strstr(cfg, "tool cc gcc") != NULL);
    ASSERT_TRUE(strstr(cfg, "target test_horu exe") != NULL);
    ASSERT_TRUE(strstr(cfg, "/repo/meikyu/lib/horu/test.c") != NULL);
    ASSERT_TRUE(strstr(cfg, "/repo/meikyu/lib/horu/horu.c") != NULL);
    ASSERT_TRUE(strstr(cfg, "std c89") != NULL);
    ASSERT_TRUE(strstr(cfg, "pedantic") != NULL); /* default on */
    ASSERT_TRUE(strstr(cfg, "lib m") != NULL);
    ASSERT_TRUE(strstr(cfg, "test_horu") != NULL); /* the out path */
    ASSERT_TRUE(strstr(cfg, "coverage") == NULL);  /* coverage off */
}

UTEST(tests_gen, dodai_substituted_per_os) {
    /* a dodai_*.c src is the per-OS source: tests_gen rewrites it to the
       selected platform's dodai= so one manifest row works on every OS. */
    static char buf[1u << 16];
    char cfg[1u << 14];
    arena a; BuildManifest m; BuildPlatform p; ito err = {0,0};
    ito_buf out;
    const char *MAN =
        "platform windows builddir=build exe=.exe dll=.dll"
        " dodai=dodai_windows.c rpath= sdl=mingw-prebuilt\n"
        "lib_test kansi std=c99 src=kansi.c,../dodai/dodai_posix.c"
        " include=lib/dodai link=pthread\n";
    char *txt;
    ito text;
    create_arena(&a, buf, sizeof(buf));
    txt = arena_copy_string(&a, MAN, strlen(MAN));
    text.ptr = txt; text.len = strlen(MAN);
    ASSERT_TRUE(build_manifest_parse(&a, text, &m, &err));
    ASSERT_TRUE(build_manifest_select(&m, ITO("windows"), &p, &err));

    ito_buf_init(&out, cfg, sizeof(cfg));
    tests_gen_emit(&out, &m, &p, "/r/meikyu", "gcc", 0);
    ASSERT_FALSE(out.overflow);

    ASSERT_TRUE(strstr(cfg, "dodai_windows.c") != NULL); /* substituted */
    ASSERT_TRUE(strstr(cfg, "dodai_posix.c") == NULL);   /* original gone */
    ASSERT_TRUE(strstr(cfg, "kansi.c") != NULL);         /* non-dodai untouched */
    ASSERT_TRUE(strstr(cfg, "test_kansi.exe") != NULL);  /* exe suffix applied */
}

UTEST(tests_gen, coverage_variant) {
    /* coverage on => `coverage` key on each target + outputs under gen/cov/,
       and the cc must be clang (the MC/DC backend). */
    static char buf[1u << 16];
    char cfg[1u << 14];
    arena a; BuildManifest m; BuildPlatform p; ito err = {0,0};
    ito_buf out;
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(setup(&a, MANIFEST, &m, &p, &err));

    ito_buf_init(&out, cfg, sizeof(cfg));
    tests_gen_emit(&out, &m, &p, "/repo/meikyu", "clang", 1);
    ASSERT_FALSE(out.overflow);

    ASSERT_TRUE(strstr(cfg, "tool cc clang") != NULL);
    ASSERT_TRUE(strstr(cfg, "\n  coverage\n") != NULL);      /* the key */
    ASSERT_TRUE(strstr(cfg, "${B}/gen/cov/test_horu") != NULL); /* cov outdir */
}
