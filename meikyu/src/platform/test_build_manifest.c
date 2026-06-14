/* Unit tests for build_manifest: parsing, OS-row selection, glslc resolution
   with $VAR expansion and ordered fallback. No filesystem, no OS detection --
   exists/getenv are injected. Same harness shape as test_seni_answers. */
#include "../../lib/seni/utest.h"
#include "build_manifest.h"
#include "../../lib/seni/arena.c"
#include "build_manifest.c"

UTEST_MAIN()

static const char *MANIFEST =
    "# comment\n"
    "host_src  src/platform/host_main.c\n"
    "host_src  lib/seni/seni.c\n"
    "include_root  src\n"
    "include_root  lib/seni\n"
    "sdl_version  release-3.4.10\n"
    "vulkan_pin  1.4.341.1\n"
    "glslc_candidate  $VULKAN_SDK/bin/glslc\n"
    "glslc_candidate  /opt/homebrew/bin/glslc\n"
    "glslc_candidate  /usr/local/bin/glslc\n"
    "cc_candidate  $CC\n"
    "cc_candidate  /usr/bin/gcc\n"
    "platform mac    builddir=build-mac   exe=     dll=.so  dodai=dodai_posix.c   rpath=@loader_path sdl=fetch\n"
    "platform windows builddir=build       exe=.exe dll=.dll dodai=dodai_windows.c rpath=            sdl=mingw-prebuilt\n";

/* parse the shared MANIFEST; returns the parse result so callers ASSERT it. */
static b32 load(arena *a, BuildManifest *m, ito *err) {
    char *txt = arena_copy_string(a, MANIFEST, strlen(MANIFEST));
    ito text;
    text.ptr = txt;
    text.len = strlen(MANIFEST);
    return build_manifest_parse(a, text, m, err);
}

UTEST(manifest, parse_lists) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0};
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_EQ(2, m.host_src_count);
    ASSERT_EQ(2, m.include_root_count);
    ASSERT_EQ(3, m.glslc_count);
    ASSERT_EQ(2, m.platform_count);
    ASSERT_TRUE(ito_eq_c(m.sdl_version, "release-3.4.10"));
    ASSERT_TRUE(ito_eq_c(m.vulkan_pin, "1.4.341.1"));
    ASSERT_TRUE(ito_eq_c(m.host_src[0], "src/platform/host_main.c"));
}

/* lib_test table + coverage facts: rows mirror the platform row (name + k=v),
   with comma-list values for src/include/link. */
static const char *MANIFEST_LT =
    "platform mac builddir=build-mac exe= dll=.so dodai=dodai_posix.c"
    " rpath=@loader_path sdl=fetch\n"
    "coverage_min_mcdc 90\n"
    "llvmcov_candidate $TOOLCHAIN/llvm-cov\n"
    "llvmcov_candidate /usr/bin/llvm-cov\n"
    "llvmprofdata_candidate /usr/bin/llvm-profdata\n"
    "lib_test ito   std=c99\n"
    "lib_test seni  std=c89 src=seni.c,arena.c\n"
    "lib_test kansi std=c99 src=kansi.c,../dodai/dodai_posix.c"
    " include=lib/dodai link=pthread\n"
    "lib_test horu  std=c89 src=horu.c link=m pedantic=0\n";

static b32 load_txt(arena *a, const char *s, BuildManifest *m, ito *err) {
    char *txt = arena_copy_string(a, s, strlen(s));
    ito text;
    text.ptr = txt;
    text.len = strlen(s);
    return build_manifest_parse(a, text, m, err);
}

UTEST(manifest, parse_lib_test) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0};
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load_txt(&a, MANIFEST_LT, &m, &err));

    ASSERT_EQ(4, m.lib_test_count);
    ASSERT_TRUE(ito_eq_c(m.coverage_min_mcdc, "90"));
    ASSERT_EQ(2, m.llvmcov_count);
    ASSERT_EQ(1, m.llvmprofdata_count);

    ASSERT_TRUE(ito_eq_c(m.lib_test[0].name, "ito"));
    ASSERT_TRUE(ito_eq_c(m.lib_test[0].std, "c99"));
    ASSERT_TRUE(m.lib_test[0].pedantic);          /* default on */
    ASSERT_EQ(0, m.lib_test[0].src_count);

    ASSERT_TRUE(ito_eq_c(m.lib_test[1].name, "seni"));
    ASSERT_EQ(2, m.lib_test[1].src_count);
    ASSERT_TRUE(ito_eq_c(m.lib_test[1].src[1], "arena.c"));

    ASSERT_EQ(1, m.lib_test[2].include_count);
    ASSERT_TRUE(ito_eq_c(m.lib_test[2].include[0], "lib/dodai"));
    ASSERT_EQ(1, m.lib_test[2].link_count);
    ASSERT_TRUE(ito_eq_c(m.lib_test[2].link[0], "pthread"));

    ASSERT_TRUE(ito_eq_c(m.lib_test[3].name, "horu"));
    ASSERT_FALSE(m.lib_test[3].pedantic);         /* pedantic=0 */
    ASSERT_TRUE(ito_eq_c(m.lib_test[3].link[0], "m"));
}

UTEST(manifest, select_mac_row) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; BuildPlatform bp;
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_TRUE(build_manifest_select(&m, ITO("mac"), &bp, &err));
    ASSERT_TRUE(ito_eq_c(bp.builddir, "build-mac"));
    ASSERT_TRUE(ito_eq_c(bp.exe_suffix, ""));
    ASSERT_TRUE(ito_eq_c(bp.dll_suffix, ".so"));
    ASSERT_TRUE(ito_eq_c(bp.rpath, "@loader_path"));
    ASSERT_TRUE(ito_eq_c(bp.sdl_strategy, "fetch"));
    ASSERT_TRUE(ito_eq_c(bp.dodai_src, "dodai_posix.c"));
}

UTEST(manifest, select_windows_row) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; BuildPlatform bp;
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_TRUE(build_manifest_select(&m, ITO("windows"), &bp, &err));
    ASSERT_TRUE(ito_eq_c(bp.exe_suffix, ".exe"));
    ASSERT_TRUE(ito_eq_c(bp.dll_suffix, ".dll"));
    ASSERT_TRUE(ito_eq_c(bp.rpath, "")); /* empty value parses to empty */
    ASSERT_TRUE(ito_eq_c(bp.sdl_strategy, "mingw-prebuilt"));
    ASSERT_TRUE(ito_eq_c(bp.dodai_src, "dodai_windows.c"));
}

UTEST(manifest, select_unknown_os_errs) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; BuildPlatform bp;
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_FALSE(build_manifest_select(&m, ITO("plan9"), &bp, &err));
    ASSERT_TRUE(err.len > 0);
}

/* injected probes */
static const char *fake_getenv(const char *name) {
    if (strcmp(name, "VULKAN_SDK") == 0) return "/sdk";
    if (strcmp(name, "CC") == 0) return "/sdk";
    return NULL;
}
static b32 cc_is_sdk(const char *path) { return strcmp(path, "/sdk") == 0; }
static b32 only_homebrew(const char *path) {
    return strcmp(path, "/opt/homebrew/bin/glslc") == 0;
}
static b32 only_env(const char *path) {
    return strcmp(path, "/sdk/bin/glslc") == 0;
}
static b32 none_exist(const char *path) { (void)path; return 0; }

UTEST(manifest, glslc_env_wins_first) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; char out[512];
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_TRUE(build_manifest_resolve_glslc(&m, fake_getenv, only_env, out, sizeof(out)));
    ASSERT_STREQ("/sdk/bin/glslc", out);
}

UTEST(manifest, glslc_skips_missing_to_homebrew) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; char out[512];
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_TRUE(build_manifest_resolve_glslc(&m, fake_getenv, only_homebrew, out, sizeof(out)));
    ASSERT_STREQ("/opt/homebrew/bin/glslc", out);
}

UTEST(manifest, glslc_none_returns_false) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; char out[512];
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_FALSE(build_manifest_resolve_glslc(&m, fake_getenv, none_exist, out, sizeof(out)));
}

UTEST(manifest, parse_cc_candidates) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0};
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    ASSERT_EQ(2, m.cc_count);
}

UTEST(manifest, cc_env_wins_first) {
    static char buf[1u << 16];
    arena a; BuildManifest m; ito err = {0,0}; char out[512];
    create_arena(&a, buf, sizeof(buf));
    ASSERT_TRUE(load(&a, &m, &err));
    /* $CC expands to /sdk (fake_getenv), cc_is_sdk makes it the first hit */
    ASSERT_TRUE(build_manifest_resolve_cc(&m, fake_getenv, cc_is_sdk, out, sizeof(out)));
    ASSERT_STREQ("/sdk", out);
}
