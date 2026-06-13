/* Unit + e2e tests. Includes kaji.c directly to reach internals. */
#include "kaji.c"
#include "utest.h"

#ifdef _WIN32
#include <direct.h>
#define test_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define test_mkdir(p) mkdir(p, 0755)
#endif

static void write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static void settle_ms(int ms) {
    /* filesystem mtime quanta: separate writes the stale-check must order */
    dodai_sleep_ms(ms);
}

/* ---- ito ---- */

UTEST(ito, views_and_compare) {
    ito s = ITO("hello world");
    ASSERT_EQ(11u, (unsigned)s.len);
    ASSERT_TRUE(ito_eq(ito_slice(s, 0, 5), ITO("hello")));
    ASSERT_TRUE(ito_eq_c(ito_slice(s, 6, 11), "world"));
    ASSERT_TRUE(ito_starts_with(s, ITO("hell")));
    ASSERT_TRUE(ito_ends_with(s, ITO("rld")));
    ASSERT_FALSE(ito_eq(s, ITO("hello")));
    ASSERT_TRUE(ito_eq(ito_trim(ITO("  x\t\r\n")), ITO("x")));
}

UTEST(ito, line_and_token_iterators) {
    ito text = ITO("a b\tc\r\nsecond line\n\nlast");
    ito line = ito_next_line(&text);
    ASSERT_TRUE(ito_eq(line, ITO("a b\tc"))); /* \r stripped */
    ito tok = ito_next_token(&line);
    ASSERT_TRUE(ito_eq(tok, ITO("a")));
    tok = ito_next_token(&line);
    ASSERT_TRUE(ito_eq(tok, ITO("b")));
    tok = ito_next_token(&line);
    ASSERT_TRUE(ito_eq(tok, ITO("c")));
    tok = ito_next_token(&line);
    ASSERT_TRUE(ito_is_empty(tok));

    ASSERT_TRUE(ito_eq(ito_next_line(&text), ITO("second line")));
    ASSERT_TRUE(ito_eq(ito_next_line(&text), ITO("")));
    ASSERT_TRUE(ito_eq(ito_next_line(&text), ITO("last")));
    ASSERT_EQ(0u, (unsigned)text.len);
}

UTEST(ito, copy_bounds_and_builder) {
    char small[4];
    ASSERT_FALSE(ito_copy(small, sizeof(small), ITO("toolong")));
    ASSERT_STREQ("too", small); /* truncated but terminated */
    ASSERT_TRUE(ito_copy(small, sizeof(small), ITO("ok")));
    ASSERT_STREQ("ok", small);

    char storage[16];
    ito_buf b;
    ito_buf_init(&b, storage, sizeof(storage));
    ito_buf_append(&b, ITO("gcc"));
    ito_buf_appendf(&b, " -I%s", "src");
    ASSERT_FALSE(b.overflow);
    ASSERT_STREQ("gcc -Isrc", storage);
    ito_buf_appendf(&b, " %s", "waaaay too long for this");
    ASSERT_TRUE(b.overflow); /* sticky */
}

/* ---- config parsing ---- */

static kaji *load_cfg(const char *content, char *err, int err_size) {
    test_mkdir("build");
    write_file("build/test_kaji.cfg", content);
    return kaji_load("build/test_kaji.cfg", err, err_size);
}

UTEST(parse, targets_vars_tools) {
    char err[256] = { 0 };
    kaji *k = load_cfg(
        "# comment\n"
        "builddir out_w\n"
        "builddir_linux out_l\n"
        "builddir_mac out_m\n"
        "tool glslc my_glslc\n"
        "target snap copy\n"
        "  in src.h\n"
        "  out ${B}/snap.h\n"
        "target game dll\n"
        "  dep snap\n"
        "  in unity.c\n"
        "  out ${B}/game${SO}\n"
        "  include inc1 inc2\n"
        "  flag -g0\n"
        "  define FOO=1\n"
        "  lib_win SDL3\n"
        "  lib_linux m\n"
        "  lib_mac z\n",
        err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);
    const char *bd = dodai_is_linux()   ? "out_l"
                   : dodai_is_macos()   ? "out_m"
                                                : "out_w";
    ASSERT_STREQ(bd, k->builddir);

    kaji_target *t = kaji_find(k, ITO("game"));
    ASSERT_TRUE(t != NULL);
    ASSERT_EQ(KAJI_KIND_DLL, (int)t->kind);
    ASSERT_EQ(1, t->dep_count);
    ASSERT_EQ(2, t->include_count);
    ASSERT_EQ(1, t->lib_count); /* only this OS's lib survived */
    ASSERT_STREQ(dodai_is_linux()  ? "m"
                 : dodai_is_macos() ? "z"
                                            : "SDL3", t->lib[0]);

    char expect[128];
    snprintf(expect, sizeof(expect), "%s/game%s", bd, kaji_so_ext());
    ASSERT_STREQ(expect, t->out);
    kaji_free(k);
}

UTEST(parse, errors_carry_line_numbers) {
    char err[256] = { 0 };
    ASSERT_TRUE(load_cfg("target a copy\n  in x\n  out y\n  bogus v\n",
                         err, sizeof(err)) == NULL);
    ASSERT_TRUE(strstr(err, "line 4") != NULL);
    ASSERT_TRUE(strstr(err, "bogus") != NULL);

    ASSERT_TRUE(load_cfg("target a dll\n  in x\n  out ${NOPE}/y\n",
                         err, sizeof(err)) == NULL);
    ASSERT_TRUE(strstr(err, "expansion") != NULL);

    ASSERT_TRUE(load_cfg("target a copy\n  in x\n  out y\n  dep ghost\n",
                         err, sizeof(err)) == NULL);
    ASSERT_TRUE(strstr(err, "ghost") != NULL);
}

UTEST(parse, command_assembly) {
    char err[256] = { 0 };
    kaji *k = load_cfg(
        "builddir bb\nbuilddir_linux bb\n"
        "target obj1 object\n  in a.c\n  out bb/a.o\n  flag -O1\n  include i1\n"
        "target game dll\n  in u.c\n  out bb/g${SO}\n  obj bb/a.o\n"
        "  flag -g0\n  define ED=1\n  libdir L1\n  lib z\n",
        err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    char cmd[1024];
    int posix = dodai_is_linux() || dodai_is_macos();
    kaji_target *o = kaji_find(k, ITO("obj1"));
    ASSERT_TRUE(kaji_command_for(k, o, cmd, sizeof(cmd), o->out));
    /* every path is double-quoted (clone dirs may hold spaces); flags,
       -D and -l are not paths and stay bare */
    if (posix) {
        ASSERT_STREQ("\"gcc\" -c -fPIC -O1 \"a.c\" -I\"i1\" -o \"bb/a.o\"", cmd);
    } else {
        ASSERT_STREQ("\"gcc\" -c -O1 \"a.c\" -I\"i1\" -o \"bb/a.o\"", cmd);
    }

    kaji_target *g = kaji_find(k, ITO("game"));
    ASSERT_TRUE(kaji_command_for(k, g, cmd, sizeof(cmd), "TMP"));
    if (posix) {
        ASSERT_STREQ("\"gcc\" -shared -fPIC -g0 -DED=1 \"u.c\" \"bb/a.o\" -L\"L1\" -lz -o \"TMP\"", cmd);
    } else {
        ASSERT_STREQ("\"gcc\" -shared -g0 -DED=1 \"u.c\" \"bb/a.o\" -L\"L1\" -lz -o \"TMP\"", cmd);
    }
    kaji_free(k);
}

/* ---- e2e: real gcc builds through the graph ---- */

static const char *E2E_CFG =
    "builddir build/forge\n"
    "builddir_linux build/forge\n"
    "target answer object\n"
    "  in build/answer.c\n"
    "  out ${B}/answer.o\n"
    "target mod dll\n"
    "  dep answer\n"
    "  in build/mod.c\n"
    "  out ${B}/mod${SO}\n"
    "  obj ${B}/answer.o\n";

UTEST(e2e, graph_build_skip_rebuild) {
    test_mkdir("build");
    write_file("build/answer.c", "int answer(void) { return 42; }\n");
    write_file("build/mod.c", "extern int answer(void);\n"
                              "int twice(void) { return answer() * 2; }\n");
    char err[256] = { 0 };
    kaji *k = load_cfg(E2E_CFG, err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    /* full build */
    ASSERT_EQ(0, kaji_build(k, "mod", 0));
    char so_path[256];
    snprintf(so_path, sizeof(so_path), "build/forge/mod%s", kaji_so_ext());
    unsigned long long t1;
    ASSERT_TRUE(dodai_mtime_ns(ito_from(so_path), &t1));

    /* nothing changed: everything skips, artifact untouched */
    settle_ms(30);
    ASSERT_EQ(0, kaji_build(k, "mod", 0));
    unsigned long long t2;
    ASSERT_TRUE(dodai_mtime_ns(ito_from(so_path), &t2));
    ASSERT_EQ(t1, t2);

    /* touching the object's source rebuilds object AND the dll above it */
    settle_ms(30);
    write_file("build/answer.c", "int answer(void) { return 43; }\n");
    ASSERT_EQ(0, kaji_build(k, "mod", 0));
    unsigned long long t3;
    ASSERT_TRUE(dodai_mtime_ns(ito_from(so_path), &t3));
    ASSERT_NE(t2, t3);
    kaji_free(k);
}

UTEST(e2e, async_polls_to_done) {
    test_mkdir("build");
    write_file("build/answer.c", "int answer(void) { return 1; }\n");
    write_file("build/mod.c", "extern int answer(void);\n"
                              "int once(void) { return answer(); }\n");
    char err[256] = { 0 };
    kaji *k = load_cfg(E2E_CFG, err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    settle_ms(30);
    write_file("build/mod.c", "extern int answer(void);\n"
                              "int thrice(void) { return answer() * 3; }\n");
    kaji_run run = { 0 };
    ASSERT_TRUE(kaji_build_async(k, "mod", &run, 0));
    /* busy slot refused */
    ASSERT_FALSE(kaji_build_async(k, "mod", &run, 0));

    kaji_status final = KAJI_RUNNING;
    for (int i = 0; i < 4000 && final == KAJI_RUNNING; i++) {
        final = kaji_run_poll(k, &run);
        dodai_sleep_ms(5);
    }
    ASSERT_EQ(KAJI_DONE, (int)final);
    ASSERT_EQ(KAJI_IDLE, (int)kaji_run_poll(k, &run)); /* edge-triggered */
    kaji_free(k);
}

UTEST(e2e, compile_error_fails_with_log) {
    test_mkdir("build");
    write_file("build/answer.c", "int answer(void) { return 1; }\n");
    write_file("build/mod.c", "this does not compile;\n");
    char err[256] = { 0 };
    kaji *k = load_cfg(E2E_CFG, err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    ASSERT_EQ(1, kaji_build(k, "mod", 0));

    FILE *log = fopen("build/forge/kaji_mod.log", "rb");
    ASSERT_TRUE(log != NULL);
    char buf[512] = { 0 };
    fread(buf, 1, sizeof(buf) - 1, log);
    fclose(log);
    ASSERT_TRUE(strstr(buf, "error") != NULL);
    /* heal for later runs */
    write_file("build/mod.c", "extern int answer(void);\n"
                              "int ok(void) { return answer(); }\n");
    kaji_free(k);
}

UTEST(e2e, exe_with_post_copy) {
    test_mkdir("build");
    test_mkdir("build/payload");
    write_file("build/payload/data.txt", "loot\n");
    write_file("build/main.c", "int main(void) { return 0; }\n");
    char err[256] = { 0 };
    kaji *k = load_cfg(
        "builddir build/forge\nbuilddir_linux build/forge\n"
        "target app exe\n"
        "  in build/main.c\n"
        "  out ${B}/bundle/app${EXE}\n"
        "  post copydir build/payload ${B}/bundle/payload\n"
        "  post copy build/main.c ${B}/bundle/main.c.txt\n",
        err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);
    ASSERT_EQ(0, kaji_build(k, "app", 0));

    FILE *f = fopen("build/forge/bundle/payload/data.txt", "rb");
    ASSERT_TRUE(f != NULL);
    if (f) fclose(f);
    f = fopen("build/forge/bundle/main.c.txt", "rb");
    ASSERT_TRUE(f != NULL);
    if (f) fclose(f);
    kaji_free(k);
}

UTEST_MAIN();
