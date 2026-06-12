/* Unit + e2e tests. Includes kansi.c directly to reach internals. */
#include "kansi.c"
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

/* Filesystem timestamps have coarse, lazily-flushed precision; a rewrite
   landing in the same mtime quantum with the same size is invisible to any
   stamp-based watcher. Tests separate writes in time and size. */
static void settle_ms(unsigned long long ms) {
    unsigned long long start = dodai_now_ms();
    while (dodai_now_ms() - start < ms) { /* spin */ }
}

/* ---- config parser ---- */

UTEST(parse, full_config) {
    kansi_cfg cfg;
    char err[256] = { 0 };
    const char *text =
        "# comment\n"
        "watch src\n"
        "watch ../engine/src\n"
        "ext .c .h .vert\n"
        "pre copy /y a b\n"
        "source src/unity.c\n"
        "include build\n"
        "include src\n"
        "libdir ../vendor/lib\n"
        "lib SDL3\n"
        "flag -g\n"
        "flag -O0\n"
        "define ASSETS_DIR=\"../assets\"\n"
        "out build/game_new.dll\n"
        "tmp build/game_tmp.dll\n"
        "log build/kansi.log\n"
        "debounce_ms 500\n";
    ASSERT_TRUE(kansi_parse_config(&cfg, text, err, sizeof(err)));
    ASSERT_EQ(2, cfg.watch_count);
    ASSERT_STREQ("../engine/src", cfg.watch[1]);
    ASSERT_EQ(3, cfg.ext_count);
    ASSERT_STREQ(".vert", cfg.ext[2]);
    ASSERT_EQ(1, cfg.pre_count);
    ASSERT_STREQ("copy /y a b", cfg.pre[0]);
    ASSERT_STREQ("src/unity.c", cfg.source);
    ASSERT_EQ(2, cfg.include_count);
    ASSERT_EQ(1, cfg.libdir_count);
    ASSERT_EQ(1, cfg.lib_count);
    ASSERT_EQ(2, cfg.flag_count);
    ASSERT_EQ(1, cfg.define_count);
    ASSERT_STREQ("build/game_new.dll", cfg.out);
    ASSERT_STREQ("build/game_tmp.dll", cfg.tmp);
    ASSERT_STREQ("build/kansi.log", cfg.log);
    ASSERT_EQ(500, cfg.debounce_ms);
}

UTEST(parse, defaults_and_required) {
    kansi_cfg cfg;
    char err[256] = { 0 };
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch src\nsource a.c\nout x.dll\n", err, sizeof(err)));
    ASSERT_STREQ("x.dll.tmp", cfg.tmp);     /* derived */
    ASSERT_EQ(300, cfg.debounce_ms);        /* default */

    ASSERT_FALSE(kansi_parse_config(&cfg, "watch src\nout x.dll\n",
                                    err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "source") != NULL);
    ASSERT_FALSE(kansi_parse_config(&cfg, "watch src\nsource a.c\n",
                                    err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "out") != NULL);
    ASSERT_FALSE(kansi_parse_config(&cfg, "source a.c\nout x.dll\n",
                                    err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "watch") != NULL);
}

UTEST(parse, unknown_key_reports_line) {
    kansi_cfg cfg;
    char err[256] = { 0 };
    ASSERT_FALSE(kansi_parse_config(&cfg, "watch src\nbogus x\n",
                                    err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "line 2") != NULL);
    ASSERT_TRUE(strstr(err, "bogus") != NULL);
}

/* ---- extension filter ---- */

UTEST(ext, match_case_insensitive) {
    kansi_cfg cfg;
    char err[64];
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\next .c .h\n", err, sizeof(err)));
    ASSERT_TRUE(kansi_has_ext(&cfg, "src\\game.c"));
    ASSERT_TRUE(kansi_has_ext(&cfg, "SRC\\GAME.C"));
    ASSERT_TRUE(kansi_has_ext(&cfg, "a/b/types.h"));
    ASSERT_FALSE(kansi_has_ext(&cfg, "build/game.dll"));
    ASSERT_FALSE(kansi_has_ext(&cfg, "notes.txt"));
}

UTEST(ext, no_filter_matches_all) {
    kansi_cfg cfg;
    char err[64];
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\n", err, sizeof(err)));
    ASSERT_TRUE(kansi_has_ext(&cfg, "anything.xyz"));
}

/* ---- compile command ---- */

UTEST(compile_cmd, assembly) {
    kansi_cfg cfg;
    char err[64];
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\n"
        "source src/unity.c\n"
        "include build\n"
        "include src\n"
        "libdir vlib\n"
        "lib SDL3\n"
        "flag -g\n"
        "define FOO=1\n"
        "out out.dll\n"
        "tmp tmp.dll\n", err, sizeof(err)));
    char cmd[KANSI_MAX_CMD];
    ASSERT_TRUE(kansi_build_compile_cmd(&cfg, cmd, sizeof(cmd)));
    ASSERT_STREQ("gcc -shared -g -DFOO=1 src/unity.c -Ibuild -Isrc"
                 " -Lvlib -lSDL3 -o tmp.dll", cmd);
}

UTEST(compile_cmd, objects_linked_after_source) {
    kansi_cfg cfg;
    char err[64];
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\n"
        "source src/unity.c\n"
        "obj build/vendor.o\n"
        "obj build/extra.o\n"
        "lib SDL3\n"
        "out out.dll\n"
        "tmp tmp.dll\n", err, sizeof(err)));
    char cmd[KANSI_MAX_CMD];
    ASSERT_TRUE(kansi_build_compile_cmd(&cfg, cmd, sizeof(cmd)));
    ASSERT_STREQ("gcc -shared src/unity.c build/vendor.o build/extra.o"
                 " -lSDL3 -o tmp.dll", cmd);
}

/* ---- pre_newer ---- */

UTEST(pre_newer, parse_and_skip_logic) {
    kansi_cfg cfg;
    char err[128] = { 0 };
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\n"
        "pre_newer build/in.txt build/out.txt echo rebuild\n",
        err, sizeof(err)));
    ASSERT_EQ(1, cfg.pre_count);
    ASSERT_STREQ("build/in.txt", cfg.pre_in[0]);
    ASSERT_STREQ("build/out.txt", cfg.pre_out[0]);
    ASSERT_STREQ("echo rebuild", cfg.pre[0]);

    /* malformed */
    ASSERT_FALSE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\npre_newer onlyinput\n",
        err, sizeof(err)));
    ASSERT_TRUE(strstr(err, "pre_newer") != NULL);

    /* skip logic against real files */
    test_mkdir("build");
    test_mkdir("build/newer");
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\n"
        "pre_newer build/newer/in.txt build/newer/out.txt echo x\n",
        err, sizeof(err)));

    remove("build/newer/in.txt");
    remove("build/newer/out.txt");
    write_file("build/newer/in.txt", "1");
    /* output missing: must run */
    ASSERT_FALSE(kansi_step_skippable(&cfg, 0));
    write_file("build/newer/out.txt", "1");
    settle_ms(30);
    /* output newer or equal: skip */
    ASSERT_TRUE(kansi_step_skippable(&cfg, 0));
    /* input touched after output: must run again */
    settle_ms(30);
    write_file("build/newer/in.txt", "22");
    ASSERT_FALSE(kansi_step_skippable(&cfg, 0));
    /* unconditional pre steps never skip */
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch w\nsource s.c\nout o.dll\npre echo always\n",
        err, sizeof(err)));
    ASSERT_FALSE(kansi_step_skippable(&cfg, 0));
}

/* ---- watch-only mode ---- */

UTEST(watch_only, parse_and_changed_edge) {
    kansi_cfg cfg;
    char err[128] = { 0 };
    /* watch+ext+debounce only => watch-only; still requires watch */
    ASSERT_TRUE(kansi_parse_config(&cfg, "watch w\next .c\ndebounce_ms 40\n",
                                   err, sizeof(err)));
    ASSERT_TRUE(cfg.watch_only);
    ASSERT_FALSE(kansi_parse_config(&cfg, "ext .c\n", err, sizeof(err)));

    test_mkdir("build");
    test_mkdir("build/watch_only");
    write_file("build/watch_only/kansi.cfg",
        "watch build/watch_only\next .c\ndebounce_ms 40\n");
    write_file("build/watch_only/seed.c", "int a;\n");

    char err2[256] = { 0 };
    kansi *k = kansi_start("build/watch_only/kansi.cfg", err2, sizeof(err2));
    ASSERT_TRUE_MSG(k != NULL, err2);

    settle_ms(30);
    write_file("build/watch_only/seed.c", "int a; int b;\n");

    kansi_status final = KANSI_IDLE;
    unsigned long long start = dodai_now_ms();
    while (dodai_now_ms() - start < 10000) {
        kansi_status st = kansi_update(k);
        if (st == KANSI_CHANGED || st == KANSI_BUILT || st == KANSI_ERROR) {
            final = st;
            break;
        }
    }
    ASSERT_EQ(KANSI_CHANGED, (int)final);
    /* edge-triggered: back to idle until the next change */
    ASSERT_EQ(KANSI_IDLE, (int)kansi_update(k));
    kansi_stop(k);
}

/* ---- event-based watch ---- */

static char watch_seen[1024];
static void watch_capture(const char *path, void *user) {
    (void)user;
    snprintf(watch_seen, sizeof(watch_seen), "%s", path);
}

UTEST(watch, event_notification_delivers_filename) {
    test_mkdir("build");
    test_mkdir("build/watch_ev");
    char dirs[1][512];
    snprintf(dirs[0], sizeof(dirs[0]), "build/watch_ev");

    dodai_watch w = { 0 };
    int event_mode = dodai_watch_begin(&w, dirs, 1);
    if (!event_mode) {
        /* legitimately unavailable here (e.g. 9p/drvfs mount under WSL,
           where inotify is silently dead) -- polling fallback covers it */
        printf("        watch: event mode unavailable on this fs, skipping\n");
        return;
    }

    watch_seen[0] = 0;
    write_file("build/watch_ev/poked.c", "int x;\n");

    int delivered = 0;
    unsigned long long start = dodai_now_ms();
    while (dodai_now_ms() - start < 5000) {
        delivered += dodai_watch_poll(&w, watch_capture, NULL);
        if (delivered > 0) {
            break;
        }
    }
    ASSERT_GT(delivered, 0);
    ASSERT_TRUE(strstr(watch_seen, "poked.c") != NULL);
    dodai_watch_end(&w);
}

/* ---- stamp / change detection ---- */

UTEST(stamp, changes_on_touch_and_filtered) {
    test_mkdir("build");
    test_mkdir("build/watchtest");
    write_file("build/watchtest/a.c", "int a;\n");
    write_file("build/watchtest/skip.txt", "x\n");

    kansi_cfg cfg;
    char err[64];
    ASSERT_TRUE(kansi_parse_config(&cfg,
        "watch build/watchtest\nsource s.c\nout o.dll\next .c\n",
        err, sizeof(err)));

    unsigned long long s1 = kansi_compute_stamp(&cfg);
    unsigned long long s2 = kansi_compute_stamp(&cfg);
    ASSERT_EQ(s1, s2); /* stable when nothing changes */

    write_file("build/watchtest/a.c", "int a; int b;\n"); /* size change */
    unsigned long long s3 = kansi_compute_stamp(&cfg);
    ASSERT_NE(s1, s3);

    write_file("build/watchtest/skip.txt", "different content\n");
    unsigned long long s4 = kansi_compute_stamp(&cfg);
    ASSERT_EQ(s3, s4); /* filtered file: no trigger */

    write_file("build/watchtest/new.c", "int c;\n"); /* new file */
    unsigned long long s5 = kansi_compute_stamp(&cfg);
    ASSERT_NE(s4, s5);
    remove("build/watchtest/new.c");
}

/* ---- e2e: watch a real dir, rebuild a real dll ---- */

UTEST(e2e, change_triggers_pipeline_to_built) {
    test_mkdir("build");
    test_mkdir("build/e2e");
    write_file("build/e2e/mod.c",
        "__attribute__((dllexport)) int answer(void) { return 41; }\n");
#ifdef _WIN32
    write_file("build/e2e/kansi.cfg",
        "watch build/e2e\n"
        "ext .c\n"
        "pre echo prestep ran\n"
        "source build/e2e/mod.c\n"
        "out build/e2e/mod.dll\n"
        "tmp build/e2e/mod_tmp.dll\n"
        "log build/e2e/kansi.log\n"
        "debounce_ms 50\n");
#else
    write_file("build/e2e/kansi.cfg",
        "watch build/e2e\n"
        "ext .c\n"
        "pre echo prestep ran\n"
        "source build/e2e/mod.c\n"
        "flag -fPIC\n"
        "out build/e2e/mod.so\n"
        "tmp build/e2e/mod_tmp.so\n"
        "log build/e2e/kansi.log\n"
        "debounce_ms 50\n");
#endif

    char err[256] = { 0 };
    kansi *k = kansi_start("build/e2e/kansi.cfg", err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    /* no change yet: stays idle across several polls */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(KANSI_IDLE, kansi_update(k));
    }

    /* touch the source (different size, past the mtime quantum) -> expect
       BUILT within a few seconds */
    settle_ms(30);
    write_file("build/e2e/mod.c",
        "__attribute__((dllexport)) int answer(void) { return 42 + 0; }\n");

    kansi_status final = KANSI_IDLE;
    unsigned long long start = dodai_now_ms();
    while (dodai_now_ms() - start < 15000) {
        kansi_status s = kansi_update(k);
        if (s == KANSI_BUILT || s == KANSI_ERROR) {
            final = s;
            break;
        }
    }
    ASSERT_EQ(KANSI_BUILT, final);

#ifdef _WIN32
    FILE *dll = fopen("build/e2e/mod.dll", "rb");
#else
    FILE *dll = fopen("build/e2e/mod.so", "rb");
#endif
    ASSERT_TRUE(dll != NULL);
    fclose(dll);

    /* pre step output landed in the log */
    FILE *log = fopen("build/e2e/kansi.log", "rb");
    ASSERT_TRUE(log != NULL);
    char buf[256] = { 0 };
    fread(buf, 1, sizeof(buf) - 1, log);
    fclose(log);
    ASSERT_TRUE(strstr(buf, "prestep ran") != NULL);

    kansi_stop(k);
}

UTEST(e2e, compile_failure_reports_error_once) {
    test_mkdir("build");
    test_mkdir("build/e2e_bad");
    write_file("build/e2e_bad/bad.c", "int ok;\n");
    write_file("build/e2e_bad/kansi.cfg",
        "watch build/e2e_bad\n"
        "ext .c\n"
        "source build/e2e_bad/bad.c\n"
        "out build/e2e_bad/bad.dll\n"
        "log build/e2e_bad/kansi.log\n"
        "debounce_ms 50\n");

    char err[256] = { 0 };
    kansi *k = kansi_start("build/e2e_bad/kansi.cfg", err, sizeof(err));
    ASSERT_TRUE_MSG(k != NULL, err);

    settle_ms(30);
    write_file("build/e2e_bad/bad.c", "this does not compile at all;\n");

    kansi_status final = KANSI_IDLE;
    unsigned long long start = dodai_now_ms();
    while (dodai_now_ms() - start < 15000) {
        kansi_status s = kansi_update(k);
        if (s == KANSI_BUILT || s == KANSI_ERROR) {
            final = s;
            break;
        }
    }
    ASSERT_EQ(KANSI_ERROR, final);

    /* edge-triggered: next update is back to idle, no error loop */
    ASSERT_EQ(KANSI_IDLE, kansi_update(k));
    kansi_stop(k);
}

UTEST_MAIN();
