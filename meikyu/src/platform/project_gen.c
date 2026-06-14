/* project_gen: see project_gen.h. All paths the engine contributes are
   absolute (derived from the exe location) and double-quoted in the
   generated configs -- kaji's tokenizer strips the quotes, its command
   builder re-quotes for the shell, so clone paths with spaces survive
   end to end. Project paths stay relative to the project root, which is
   also kaji's spawn cwd. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/base_types.h"
#include "abi/abi_platform.h"   /* PlatformMemory/Api, GAME_EXPORT */
#include "dodai.h"
#include "dodai_video.h"        /* dodai_exe_dir, dodai_log */
#include "project_gen.h"
#include "build_manifest.h"

/* The build dir and the generated-file paths are derived at runtime from the
   manifest's per-OS row (see gen_paths_init); they are no longer compile-time
   macros so the dir name lives only in build.manifest. */
#define SDL_MINGW  "/SDL3-mingw/x86_64-w64-mingw32"

#define MAX_SOURCES 256
#define MAX_SHADERS 64
#define PATH_CAP    1024

/* engine/vendor roots + discovery results; one project per process */
static char g_engine[PATH_CAP];   /* <repo>/meikyu, absolute */
static char g_vendor[PATH_CAP];   /* <repo>/vendor, absolute */
static char g_glslc[PATH_CAP];    /* resolved shader compiler */
static char g_cc[PATH_CAP];       /* resolved C compiler (kaji's cc tool) */
static char g_sources[MAX_SOURCES][PATH_CAP]; /* src/... relative */
static int  g_source_count;
static char g_shaders[MAX_SHADERS][PATH_CAP]; /* src/shaders/... relative */
static int  g_shader_count;
/* extra headers prepended to the game_state.h snapshot (engine-relative), so a
   lib's nested hot-state struct is in the layout seni diffs. From the marker's
   `layout_include` keys. */
#define MAX_LAYOUT_INC 8
static char g_layout_inc[MAX_LAYOUT_INC][PATH_CAP];
static int  g_layout_inc_count;

/* ---- text emitter -------------------------------------------------------- */

/* one ito_buf builder for the file under construction: sticky overflow and
   NUL-termination come from ito, so the emitter is just init + appendf */
static char    g_storage[128 * 1024];
static ito_buf g_emit;

static void emit_reset(void) { ito_buf_init(&g_emit, g_storage, sizeof(g_storage)); }

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ito_buf_vappendf(&g_emit, fmt, args);
    va_end(args);
}

/* write only when content changed: the generated file is a kaji input --
   rewriting it with identical bytes would force pointless rebuilds. A
   truncated buffer (overflow) silently builds the wrong thing, so refuse.
   *changed (optional) reports an actual write. */
static b32 write_if_changed(const char *path, b32 *changed) {
    if (changed) {
        *changed = 0;
    }
    if (g_emit.overflow) {
        dodai_log("project_gen: generated text for %s exceeds %d bytes, "
                  "refusing to write a truncated file",
                  path, (int)sizeof(g_storage));
        return 0;
    }
    size_t old_len = 0;
    char *old = dodai_read_file(michi_from_cstr(path), &old_len);
    b32 same = old && old_len == g_emit.len &&
               memcmp(old, g_emit.buf, g_emit.len) == 0;
    free(old);
    if (same) {
        return 1;
    }
    dodai_make_dirs_for(michi_from_cstr(path));
    if (!dodai_write_file(michi_from_cstr(path), g_emit.buf, g_emit.len)) {
        dodai_log("project_gen: cannot write %s", path);
        return 0;
    }
    if (changed) {
        *changed = 1;
    }
    return 1;
}

/* ---- discovery ----------------------------------------------------------- */

typedef struct {
    char (*items)[PATH_CAP];
    int  count, cap;
    const char *ext_a, *ext_b; /* ".c" / ".vert"+".frag" */
} Collect;

static void collect_cb(michi path_v, unsigned long long mtime,
                       unsigned long long size, void *user) {
    (void)mtime; (void)size;
    Collect *c = (Collect *)user;
    char path[PATH_CAP];
    if (!ito_copy(path, sizeof(path), path_v.s)) {
        return;
    }
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return;
    }
    if (strcmp(dot, c->ext_a) != 0 && (!c->ext_b || strcmp(dot, c->ext_b) != 0)) {
        return;
    }
    if (c->count == c->cap) {
        dodai_log("project_gen: more than %d files, '%s' ignored", c->cap, path);
        return;
    }
    snprintf(c->items[c->count++], PATH_CAP, "%s", path);
}

static int cmp_path(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void discover(void) {
    Collect src = { g_sources, 0, MAX_SOURCES, ".c", NULL };
    dodai_walk(PATH("src"), collect_cb, &src);
    qsort(g_sources, (size_t)src.count, PATH_CAP, cmp_path);
    g_source_count = src.count;

    Collect sh = { g_shaders, 0, MAX_SHADERS, ".vert", ".frag" };
    dodai_walk(PATH("src/shaders"), collect_cb, &sh);
    qsort(g_shaders, (size_t)sh.count, PATH_CAP, cmp_path);
    g_shader_count = sh.count;
}

/* ---- marker -------------------------------------------------------------- */

static void marker_parse(Project *p) {
    p->name[0] = 0;
    snprintf(p->assets, sizeof(p->assets), "../assets");

    size_t len = 0;
    char *text = dodai_read_file(PATH(PROJECT_MARKER), &len);
    ito rest = { text, text ? len : 0 };
    while (rest.len) {
        ito line = ito_trim(ito_next_line(&rest));
        if (!line.len || line.ptr[0] == '#') {
            continue;
        }
        ito key = ito_next_token(&line);   /* `key value...` */
        ito val = ito_trim(line);          /* rest of line, spaces kept */
        if (ito_eq_c(key, "name")) {
            ito_copy(p->name, sizeof(p->name), val);
        } else if (ito_eq_c(key, "assets")) {
            ito_copy(p->assets, sizeof(p->assets), val);
        } else if (ito_eq_c(key, "layout_include")) {
            /* engine-relative header prepended to the seni layout snapshot, so a
               lib's nested hot-state struct resolves in the reload diff. */
            if (g_layout_inc_count < MAX_LAYOUT_INC) {
                ito_copy(g_layout_inc[g_layout_inc_count++],
                         sizeof(g_layout_inc[0]), val);
            } else {
                dodai_log("project_gen: too many layout_include keys (max %d)",
                          MAX_LAYOUT_INC);
            }
        } else if (ito_eq_c(key, "version")) {
            if (!ito_eq_c(val, MEIKYU_VERSION)) {
                dodai_log("project_gen: project version '" ITO_FMT "' != "
                          "engine version '" MEIKYU_VERSION "' -- generated "
                          "recipes may differ across the team", ITO_ARG(val));
            }
        } else {
            dodai_log("project_gen: unknown marker key '" ITO_FMT "'",
                      ITO_ARG(key));
        }
    }
    free(text);

    if (!p->name[0]) {
        /* fall back to the folder's basename */
        michi_buf ab;
        michi_buf_reset(&ab);
        ito base = michi_base(michi_view(&ab)).s; /* default: empty */
        if (dodai_absolute_path(PATH("."), &ab) == 0) {
            base = michi_base(michi_view(&ab)).s;
        }
        if (base.len) {
            ito_copy(p->name, sizeof(p->name), base);
        } else {
            snprintf(p->name, sizeof(p->name), "game");
        }
        dodai_log("project_gen: no 'name' in " PROJECT_MARKER ", using '%s'",
                  p->name);
    }
    /* the name lands in exe paths and -D defines where embedded whitespace
       cannot be quoted away; sanitize rather than refuse */
    b32 sanitized = 0;
    for (char *c = p->name; *c; c++) {
        if (*c == ' ' || *c == '\t') {
            *c = '-';
            sanitized = 1;
        }
    }
    if (sanitized) {
        dodai_log("project_gen: whitespace in 'name' replaced: '%s'", p->name);
    }
}

/* ---- roots + tools ------------------------------------------------------- */

static b32 roots_resolve(void) {
    char up[PATH_CAP + 16];
    michi_buf mb;

    michi_buf_reset(&mb);
    if (!dodai_exe_dir(&mb)) {
        dodai_log("project_gen: cannot locate the exe");
        return 0;
    }
    snprintf(up, sizeof(up), "%s..", michi_cstr(&mb)); /* dir ends with sep */
    michi_buf_reset(&mb);
    if (dodai_absolute_path(michi_from_cstr(up), &mb) != 0) {
        dodai_log("project_gen: cannot resolve engine root from '%s'", up);
        return 0;
    }
    snprintf(g_engine, sizeof(g_engine), "%s", michi_cstr(&mb));

    snprintf(up, sizeof(up), "%s/../vendor", g_engine);
    michi_buf_reset(&mb);
    if (dodai_absolute_path(michi_from_cstr(up), &mb) != 0) {
        dodai_log("project_gen: no vendor/ beside the engine ('%s')", up);
        return 0;
    }
    snprintf(g_vendor, sizeof(g_vendor), "%s", michi_cstr(&mb));
    return 1;
}

static b32 file_exists(const char *path) {
    unsigned long long m;
    return dodai_mtime_ns(michi_from_cstr(path), &m) != 0;
}

/* ---- build.manifest: single source of truth ----------------------------- */

/* Process-lifetime parse of <engine>/build.manifest. The arena holds the
   manifest text; g_manifest and g_plat are ito views INTO that text, so the
   storage must outlive them -- hence file scope, not a local. */
static char         g_manifest_storage[1u << 16];
static arena        g_manifest_arena;
static BuildManifest g_manifest;
static BuildPlatform g_plat;          /* the row for the running OS */
static b32          g_manifest_loaded;

static ito current_os(void) {
    if (dodai_is_macos()) return ITO("mac");
    if (dodai_is_linux()) return ITO("linux");
    return ITO("windows");
}

/* getenv with the const-qualified signature build_manifest_resolve_glslc wants */
static const char *env_get(const char *name) { return getenv(name); }

/* Last resort when no manifest candidate exists: find a bare tool (`glslc`,
   `gcc`) on PATH and resolve it to an ABSOLUTE path, so the logged tool path
   makes machine-to-machine divergence visible rather than silent. On Windows
   the `.exe` suffix is added. 0 = not found. */
static b32 tool_from_path(const char *name, char *out, size_t cap) {
    const char *path = getenv("PATH");
#ifdef _WIN32
    const char sep = ';';
    const char *suffix = ".exe";
#else
    const char sep = ':';
    const char *suffix = "";
#endif
    if (!path) {
        return 0;
    }
    while (*path) {
        const char *end = path;
        char probe[PATH_CAP];
        size_t dirlen;
        while (*end && *end != sep) {
            end++;
        }
        dirlen = (size_t)(end - path);
        if (dirlen && dirlen + 2 + strlen(name) + strlen(suffix) < sizeof(probe)) {
            snprintf(probe, sizeof(probe), "%.*s/%s%s",
                     (int)dirlen, path, name, suffix);
            if (file_exists(probe)) {
                snprintf(out, cap, "%s", probe);
                return 1;
            }
        }
        path = *end ? end + 1 : end;
    }
    return 0;
}

/* Parses build.manifest once, selects the running-OS row into g_plat, and
   resolves g_glslc from the ordered candidate list (then PATH). Requires
   roots_resolve() to have set g_engine. 0 = load/parse/select failure (logged);
   an empty g_glslc is left for project_open to refuse loudly. */
static b32 manifest_load_once(void) {
    char mpath[PATH_CAP];
    size_t len = 0;
    char *txt;
    ito text, err;

    if (g_manifest_loaded) {
        return 1;
    }
    create_arena(&g_manifest_arena, g_manifest_storage, sizeof(g_manifest_storage));
    snprintf(mpath, sizeof(mpath), "%s/build.manifest", g_engine);
    txt = dodai_read_file(michi_from_cstr(mpath), &len);
    if (!txt) {
        dodai_log("project_gen: cannot read %s", mpath);
        return 0;
    }
    text.ptr = arena_copy_string(&g_manifest_arena, txt, len);
    text.len = text.ptr ? len : 0;
    free(txt);
    err.ptr = 0; err.len = 0;
    if (!build_manifest_parse(&g_manifest_arena, text, &g_manifest, &err)) {
        dodai_log("project_gen: build.manifest parse error: " ITO_FMT, ITO_ARG(err));
        return 0;
    }
    if (!build_manifest_select(&g_manifest, current_os(), &g_plat, &err)) {
        dodai_log("project_gen: build.manifest: " ITO_FMT, ITO_ARG(err));
        return 0;
    }
    /* The host/engine binary bakes PLATFORM_BUILD_DIR (abi_platform.h) at
       compile time -- it loads the game dll and shaders from there at runtime.
       project_gen emits every recipe path from this manifest row. If the two
       name different dirs the host looks where nothing was built, so enforce
       the invariant here rather than let it drift silently (the macro is the
       one per-OS fact that legitimately cannot read the manifest at compile
       time; this check is its leash). */
    if (!ito_eq_c(g_plat.builddir, PLATFORM_BUILD_DIR)) {
        dodai_log("project_gen: build.manifest builddir '" ITO_FMT "' != compiled "
                  "PLATFORM_BUILD_DIR '%s' -- manifest and abi_platform.h disagree",
                  ITO_ARG(g_plat.builddir), PLATFORM_BUILD_DIR);
        return 0;
    }
    if (!build_manifest_resolve_glslc(&g_manifest, env_get, file_exists,
                                      g_glslc, sizeof(g_glslc))) {
        if (!tool_from_path("glslc", g_glslc, sizeof(g_glslc))) {
            g_glslc[0] = 0;
        }
    }
    if (!build_manifest_resolve_cc(&g_manifest, env_get, file_exists,
                                   g_cc, sizeof(g_cc))) {
        if (!tool_from_path("gcc", g_cc, sizeof(g_cc))) {
            g_cc[0] = 0;
        }
    }
    g_manifest_loaded = 1;
    return 1;
}

/* Runtime equivalents of the old GEN_DIR/UNITY_GEN/... macros, built once from
   g_plat.builddir after the manifest loads. Relative to the project root. */
static char g_builddir[256];   /* "build-mac" / "build-linux" / "build" */
static char g_gen_dir[300];    /* "<builddir>/gen" */
static char g_unity_gen[320];  /* "<builddir>/gen/game_unity.gen.c" */
static char g_kaji_gen[320];   /* "<builddir>/gen/kaji.gen.cfg" */
static char g_kansi_gen[320];  /* "<builddir>/gen/kansi.gen.cfg" */

static void gen_paths_init(void) {
    ito_copy(g_builddir, sizeof(g_builddir), g_plat.builddir);
    snprintf(g_gen_dir,   sizeof(g_gen_dir),   "%s/gen", g_builddir);
    snprintf(g_unity_gen, sizeof(g_unity_gen), "%s/game_unity.gen.c", g_gen_dir);
    snprintf(g_kaji_gen,  sizeof(g_kaji_gen),  "%s/kaji.gen.cfg", g_gen_dir);
    snprintf(g_kansi_gen, sizeof(g_kansi_gen), "%s/kansi.gen.cfg", g_gen_dir);
}

/* ---- generated unity ----------------------------------------------------- */

static b32 unity_emit_and_write(void) {
    emit_reset();
    emit("/* generated by meikyu " MEIKYU_VERSION " -- do not edit;\n"
         "   regenerated on every project open and source-set change */\n\n");
    emit("#include \"%s/src/engine/engine_unity.c\"\n\n", g_engine);
    emit("#include \"%s/lib/seni/seni_panel.c\" "
         "/* seni's reload-question panel */\n\n", g_engine);
    for (int i = 0; i < g_source_count; i++) {
        michi_buf ab;
        michi_buf_reset(&ab);
        if (dodai_absolute_path(michi_from_cstr(g_sources[i]), &ab) != 0) {
            dodai_log("project_gen: cannot resolve '%s'", g_sources[i]);
            return 0;
        }
        emit("#include \"%s\"\n", michi_cstr(&ab));
    }
    return write_if_changed(g_unity_gen, NULL);
}

/* ---- generated kansi cfg ------------------------------------------------- */

static b32 kansi_emit_and_write(void) {
    emit_reset();
    emit("# generated by meikyu " MEIKYU_VERSION " -- do not edit\n");
    emit("watch src\n");
    emit("watch %s/src\n", g_engine); /* kansi values run to end of line --
                                         spaces are fine unquoted */
    emit("ext .c .h .vert .frag\n");
    emit("debounce_ms 30\n");
    return write_if_changed(g_kansi_gen, NULL);
}

/* ---- generated kaji cfg -------------------------------------------------- */

/* "src/shaders/model.vert" -> target name "model_vert"; the spv keeps the
   full basename ("model.vert.spv") because game code loads it by name */
static const char *shader_base(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void shader_target_name(const char *path, char *out, size_t cap) {
    snprintf(out, cap, "%s", shader_base(path));
    for (char *c = out; *c; c++) {
        if (*c == '.' || *c == ' ') {
            *c = '_';
        }
    }
}

static b32 kaji_emit_and_write(const Project *p, b32 *changed) {
    const char *B = g_builddir;
    /* per-OS link/copy lines are emitted with _win/_posix/_mac/_linux key
       suffixes and resolved by kaji at parse -- no OS branches here. */

    /* shared include list for pch + game (MUST be identical: the .gch is
       flag-sensitive). Absolute entries are quoted -- kaji strips the
       quotes at parse and re-quotes for the shell. */
    char includes[2560];
    /* lib/horu, lib/tsumami, lib/henshu: a project may pull in these game-side
       libs (e.g. the henshu editor) which include each other's headers by bare
       name. lib/seni covers seni_annotations.h (henshu's EditorState defaults). */
    snprintf(includes, sizeof(includes),
             "%s src \"%s/src\" \"%s/src/editor\" \"%s/lib/ito\" \"%s/cgltf\" "
             "\"%s/stb\" \"%s/clay\" \"%s/lib/seni\" \"%s/lib/horu\" "
             "\"%s/lib/tsumami\" \"%s/lib/henshu\" \"%s" SDL_MINGW "/include\"",
             B, g_engine, g_engine, g_engine, g_vendor, g_vendor, g_vendor,
             g_engine, g_engine, g_engine, g_engine, g_vendor);

    emit_reset();
    emit("# generated by meikyu " MEIKYU_VERSION " for project '%s' -- "
         "do not edit\n", p->name);
    emit("builddir %s\n\n", B);

    /* boundary rules: kaji rejects the build if any source under these dirs
       #includes the forbidden substring. mechanical layering enforcement. */
    emit("deny_include \"%s/src/engine\"   editor.h\n", g_engine);
    emit("deny_include \"%s/src/engine\"   dodai\n", g_engine);
    emit("deny_include \"%s/src/editor\"   dodai\n", g_engine);
    emit("deny_include \"%s/src/platform\" engine/\n", g_engine);
    emit("deny_include \"%s/src/abi\"      dodai\n", g_engine);
    emit("deny_include \"%s/src/abi\"      SDL3\n", g_engine);
    emit("deny_include \"%s/lib/seni\"     ito.h\n", g_engine);
    emit("deny_include src dodai\n\n");

    /* tool value runs to end of line in kaji's parser: no quotes here,
       the command builder quotes it for the shell. cc is the resolved C
       compiler -- the game dll build and the migration both use it. */
    emit("tool glslc %s\n", g_glslc);
    emit("tool cc %s\n\n", g_cc);

    /* hot-state snapshot: #include and seni's .incbin must read the same bytes
       even if src/game_state.h is saved mid-compile. A copy target with >1 `in`
       concatenates -- any `layout_include` headers (a lib's nested hot-state
       struct, e.g. henshu's EditorState) come first so seni's layout diff can
       resolve the fields game_state.h nests. */
    emit("target snapshot copy\n");
    for (int i = 0; i < g_layout_inc_count; i++) {
        emit("  in \"%s/%s\"\n", g_engine, g_layout_inc[i]);
    }
    emit("  in src/game_state.h\n"
         "  out %s/game_state.h\n\n", B);

    /* project shaders (discovered) + the engine's ui shaders */
    for (int i = 0; i < g_shader_count; i++) {
        char tname[256];
        shader_target_name(g_shaders[i], tname, sizeof(tname));
        emit("target %s shader\n"
             "  in \"%s\"\n"
             "  out \"%s/%s.spv\"\n\n", tname, g_shaders[i], B,
             shader_base(g_shaders[i]));
    }
    emit("target ui_vert shader\n"
         "  in \"%s/src/engine/render/shaders/ui.vert\"\n"
         "  out %s/ui.vert.spv\n\n", g_engine, B);
    emit("target ui_frag shader\n"
         "  in \"%s/src/engine/render/shaders/ui.frag\"\n"
         "  out %s/ui.frag.spv\n\n", g_engine, B);

    emit("target vendor_impl object\n"
         "  in \"%s/src/engine/vendor_impl.c\"\n"
         "  out %s/vendor_impl.o\n"
         "  flag -O1\n"
         "  include \"%s/cgltf\" \"%s/stb\" \"%s/clay\"\n\n",
         g_engine, B, g_vendor, g_vendor, g_vendor);

    emit("target ui object\n"
         "  in \"%s/src/engine/ui/ui.c\"\n"
         "  also \"%s/src/engine/ui/ui.h\" \"%s/clay/clay.h\"\n"
         "  out %s/ui.o\n"
         "  flag -g0 -O0 -pipe -std=c99\n"
         "  include \"%s/src\" \"%s/lib/ito\" \"%s/clay\" \"%s/stb\" "
              "\"%s" SDL_MINGW "/include\"\n\n",
         g_engine, g_engine, g_vendor, B, g_engine, g_engine, g_vendor,
         g_vendor, g_vendor);

    emit("target editor object\n"
         "  in \"%s/src/editor/editor_unity.c\"\n"
         "  also \"%s/src/editor/editor.c\" \"%s/src/editor/editor.h\"\n"
         "  also \"%s/src/editor/panels/inspector.c\" "
              "\"%s/src/editor/panels/build_panel.c\"\n"
         "  also \"%s/lib/seni/seni.c\" \"%s/lib/seni/seni.h\" "
              "\"%s/lib/seni/arena.c\"\n"
         "  out %s/editor.o\n"
         "  flag -g0 -O0 -pipe -std=c99\n"
         "  include \"%s/src/editor\" \"%s/src\" \"%s/lib/ito\" \"%s/lib/seni\" "
              "\"%s" SDL_MINGW "/include\"\n\n",
         g_engine, g_engine, g_engine, g_engine, g_engine,
         g_engine, g_engine, g_engine, B,
         g_engine, g_engine, g_engine, g_engine, g_vendor);

    emit("target pch_snapshot copy\n"
         "  in \"%s/src/pch.h\"\n"
         "  out %s/pch.h\n\n", g_engine, B);
    emit("target pch pch\n"
         "  dep pch_snapshot\n"
         "  in %s/pch.h\n"
         "  out %s/pch.h.gch\n"
         "  flag -g0 -O0 -pipe -std=c99\n"
         "  include %s\n\n", B, B, includes);

    /* the reloadable unit; the host force-builds it on kansi's edge */
    emit("target game dll\n"
         "  dep snapshot ui_vert ui_frag");
    for (int i = 0; i < g_shader_count; i++) {
        char tname[256];
        shader_target_name(g_shaders[i], tname, sizeof(tname));
        emit(" %s", tname);
    }
    emit(" vendor_impl ui editor pch\n");
    emit("  in %s\n"
         "  out %s/game_new" ITO_FMT "\n"
         "  obj %s/vendor_impl.o %s/ui.o %s/editor.o\n"
         "  flag -g0 -O0 -pipe -Wall -Wextra -std=c99\n"
         "  flag -include %s/pch.h\n"
         "  define EDITOR_BUILD\n"
         "  include %s\n",
         g_unity_gen, B, ITO_ARG(g_plat.dll_suffix), B, B, B, B, includes);
    /* link: OS-tagged keys, resolved per-OS by kaji's _win/_posix/_mac parser
       (two-level namespace on mac lets SDL symbols resolve at dlopen from the
       already-loaded libSDL3; linux gets it free via the flat namespace). */
    emit("  libdir_win \"%s" SDL_MINGW "/lib\"\n", g_vendor);
    emit("  lib_win SDL3\n");
    emit("  lib_posix m\n");
    emit("  flag_mac -Wl,-undefined,dynamic_lookup\n");
    emit("\n");

    /* the engine exe rebuilds itself; lands beside the running exe */
    emit("target host exe\n"
         "  in \"%s/src/platform/host_main.c\" "
              "\"%s/src/platform/project_gen.c\" "
              "\"%s/src/platform/seni_answers.c\"\n"
         "  in \"%s/lib/seni/seni.c\" \"%s/lib/seni/arena.c\"\n"
         "  in \"%s/lib/kansi/kansi.c\" \"%s/lib/kaji/kaji.c\"\n"
         "  in \"%s/lib/dodai/dodai_video_sdl.c\"\n"
         "  in \"%s/lib/dodai/" ITO_FMT "\"\n"
         "  out \"%s/%s/meikyu_new" ITO_FMT "\"\n"
         "  flag -g -O0 -Wall -Wextra -std=c99\n"
         "  include \"%s/src\" \"%s/lib/seni\" \"%s/lib/kansi\" \"%s/lib/kaji\" "
              "\"%s/lib/dodai\" \"%s" SDL_MINGW "/include\"\n",
         g_engine, g_engine, g_engine, g_engine, g_engine, g_engine, g_engine,
         g_engine, g_engine, ITO_ARG(g_plat.dodai_src),
         g_engine, B, ITO_ARG(g_plat.exe_suffix),
         g_engine, g_engine, g_engine, g_engine, g_engine, g_vendor);
    /* link: OS-tagged, resolved per-OS by kaji */
    emit("  libdir_win \"%s" SDL_MINGW "/lib\"\n", g_vendor);
    emit("  lib_win SDL3\n");
    emit("  libdir_posix \"%s/%s/_deps/sdl3-build\"\n", g_engine, B);
    emit("  lib_posix SDL3 m pthread\n");
    emit("  flag_mac -Wl,-rpath,@loader_path/_deps/sdl3-build\n");
    emit("  flag_linux -Wl,-rpath,'$ORIGIN/_deps/sdl3-build'\n");
    emit("\n");

    /* standalone bundle: runtime host + engine + game statically */
    emit("target ship exe\n"
         "  dep snapshot ui_vert ui_frag");
    for (int i = 0; i < g_shader_count; i++) {
        char tname[256];
        shader_target_name(g_shaders[i], tname, sizeof(tname));
        emit(" %s", tname);
    }
    emit(" vendor_impl ui\n");
    emit("  in \"%s/src/platform/runtime_main.c\" %s\n"
         "  in \"%s/lib/dodai/dodai_video_sdl.c\"\n"
         "  in \"%s/lib/dodai/" ITO_FMT "\"\n"
         "  out %s/ship/%s" ITO_FMT "\n"
         "  obj %s/vendor_impl.o %s/ui.o\n"
         "  flag -O2 -std=c99\n"
         "  define NDEBUG ASSETS_DIR=\\\"assets\\\" GAME_TITLE=\\\"%s\\\"\n"
         "  include %s \"%s/lib/dodai\"\n",
         g_engine, g_unity_gen, g_engine, g_engine,
         ITO_ARG(g_plat.dodai_src),
         B, p->name, ITO_ARG(g_plat.exe_suffix), B, B, p->name, includes, g_engine);
    /* link: OS-tagged, resolved per-OS by kaji */
    emit("  libdir_win \"%s" SDL_MINGW "/lib\"\n", g_vendor);
    emit("  lib_win SDL3\n");
    emit("  libdir_posix \"%s/%s/_deps/sdl3-build\"\n", g_engine, B);
    emit("  lib_posix SDL3 m pthread\n");
    emit("  flag_mac -Wl,-rpath,@loader_path\n");
    emit("  flag_linux -Wl,-rpath,'$ORIGIN'\n");
    for (int i = 0; i < g_shader_count; i++) {
        emit("  post copy \"%s/%s.spv\" \"%s/ship/%s/%s.spv\"\n",
             B, shader_base(g_shaders[i]), B, B, shader_base(g_shaders[i]));
    }
    emit("  post copy %s/ui.vert.spv %s/ship/%s/ui.vert.spv\n", B, B, B);
    emit("  post copy %s/ui.frag.spv %s/ship/%s/ui.frag.spv\n", B, B, B);
    /* the SDL runtime alongside the ship exe -- OS-tagged copy, kaji resolves */
    emit("  post_win copy \"%s" SDL_MINGW "/bin/SDL3.dll\" %s/ship/SDL3.dll\n",
         g_vendor, B);
    emit("  post_mac copy \"%s/%s/_deps/sdl3-build/libSDL3.0.dylib\" "
         "%s/ship/libSDL3.0.dylib\n", g_engine, B, B);
    emit("  post_linux copy \"%s/%s/_deps/sdl3-build/libSDL3.so.0\" "
         "%s/ship/libSDL3.so.0\n", g_engine, B, B);
    emit("  post copydir \"%s\" %s/ship/assets\n", p->assets, B);

    return write_if_changed(g_kaji_gen, changed);
}

/* ---- public -------------------------------------------------------------- */

b32 project_open(Project *p) {
    memset(p, 0, sizeof(*p));

    if (!roots_resolve()) {
        return 0;
    }
    if (!manifest_load_once()) {
        return 0; /* manifest_load_once logged the cause */
    }
    gen_paths_init();
    snprintf(p->kaji_cfg, sizeof(p->kaji_cfg), "%s", g_kaji_gen);
    snprintf(p->kansi_cfg, sizeof(p->kansi_cfg), "%s", g_kansi_gen);
    if (!g_glslc[0] || !g_cc[0]) {
        dodai_log("project_gen: missing toolchain -- glslc:%s cc:%s. Install the "
                  "missing tool, set $VULKAN_SDK / $CC, or add a candidate to "
                  "build.manifest; refusing to open a project that cannot build "
                  "or hot-reload.",
                  g_glslc[0] ? "ok" : "MISSING", g_cc[0] ? "ok" : "MISSING");
        return 0;
    }
    marker_parse(p);
    discover();
    if (g_source_count == 0) {
        dodai_log("project_gen: no .c files under src/ -- nothing to build");
        return 0;
    }
    if (!unity_emit_and_write() || !kansi_emit_and_write() ||
        !kaji_emit_and_write(p, NULL)) {
        return 0;
    }
    dodai_log("project '%s': %d source file%s, %d shader%s (glslc: %s)",
              p->name, g_source_count, g_source_count == 1 ? "" : "s",
              g_shader_count, g_shader_count == 1 ? "" : "s", g_glslc);
    return 1;
}

b32 project_regen(Project *p, b32 *kaji_changed) {
    *kaji_changed = 0;
    discover();
    if (g_source_count == 0) {
        dodai_log("project_gen: src/ has no .c files anymore; keeping the "
                  "previous generated files");
        return 1;
    }
    /* unity follows the source set; the kaji cfg follows the shader set
       (each shader is its own target) -- regenerate both so a new .vert
       saved mid-session compiles without reopening the project */
    return unity_emit_and_write() && kaji_emit_and_write(p, kaji_changed);
}
