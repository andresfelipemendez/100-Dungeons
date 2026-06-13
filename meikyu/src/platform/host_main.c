/* Platform layer: owns the process, window, GPU device, the persistent
   memory blocks, the dll reload loop, and the seni migration driver. Knows
   nothing about the game's types beyond the ABI; must never need recompiling
   while iterating on the game.

   One file for every platform: all OS services come from dodai (core +
   dodai_video). The only per-OS text left is path_basename's backslash
   case -- string parsing, not an OS service. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <sys/stat.h>   /* chmod: restore the bundle exe's exec bit (--install) */
#endif

#include "base/base_types.h"
#include "abi/abi_platform.h"
#include "abi/abi_gpu.h"

#include "seni.h"
#include "platform/seni_answers.h"
#include "platform/build_manifest.h"
#include "platform/migrate_core.h"
#include "kansi.h"
#include "kaji.h"
#include "dodai.h"
#include "dodai_video.h"
#include "project_gen.h"

#define GAME_DLL_NEW PLATFORM_BUILD_DIR "/game_new" DODAI_DLL_SUFFIX
#define GAME_STATE_HDR  "src/game_state.h"

/* the forge, defined below; migrate_hot_memory (above its definition) compiles
   the migration through it so kaji owns all compilation. */
static kaji *g_forge;

#define HOT_SIZE       (1u << 20)   /* 1 MB  */
#define TRANSIENT_SIZE (64u << 20)  /* 64 MB */

/* Several instances may run against one project dir (multi-window testing).
   Everything an instance WRITES privately is suffixed with its pid: the
   loaded-dll copy (Windows locks loaded images) and the migration scratch
   (each instance migrates its own hot memory). Shared work -- watching the
   sources and forging build/game_new -- is done by exactly ONE instance,
   elected by the forge lock below; followers only watch the published dll. */
static char g_dll_loaded_path[256];
static char g_mig_c_path[256];
static char g_mig_dll_path[256];
static char g_mig_err_path[256];

static void instance_paths_init(void) {
    unsigned long pid = dodai_pid();
    snprintf(g_dll_loaded_path, sizeof(g_dll_loaded_path),
                 PLATFORM_BUILD_DIR "/game_loaded_%lu" DODAI_DLL_SUFFIX, pid);
    snprintf(g_mig_c_path, sizeof(g_mig_c_path),
                 PLATFORM_BUILD_DIR "/mig_%lu.c", pid);
    snprintf(g_mig_dll_path, sizeof(g_mig_dll_path),
                 PLATFORM_BUILD_DIR "/mig_%lu" DODAI_DLL_SUFFIX, pid);
    snprintf(g_mig_err_path, sizeof(g_mig_err_path),
                 PLATFORM_BUILD_DIR "/mig_errors_%lu.log", pid);
}

/* Crashed/killed instances leave their per-pid files behind (a graceful
   quit removes its own). Sweep what is deletable; files a live instance
   still holds just refuse and stay. */
static void instance_litter_sweep(void) {
    dodai_remove_prefixed(PATH(PLATFORM_BUILD_DIR), ITO("game_loaded_"));
    dodai_remove_prefixed(PATH(PLATFORM_BUILD_DIR), ITO("mig_"));
}

/* The forge lock: held for the holder's lifetime, vanishes with the process
   (delete-on-close / flock), so a crashed builder never wedges the project.
   Followers retry periodically and take over when the builder exits. */
static void *g_forge_lock;
static b32 forge_lock_try(void) {
    if (g_forge_lock) {
        return 1;
    }
    return dodai_lockfile_try(PATH(PLATFORM_BUILD_DIR "/.forge.lock"), &g_forge_lock);
}

typedef struct {
    void                  *lib;
    GameUpdateAndRenderFn *update;
    char                  *layout;     /* heap copy of the dll's seni_layout */
    unsigned long long     src_mtime;  /* mtime of GAME_DLL_NEW when loaded */
} GameCode;

static void game_unload(GameCode *code) {
    if (code->lib) {
        dodai_lib_close(code->lib);
    }
    free(code->layout);
    memset(code, 0, sizeof(*code));
}

static b32 game_load(GameCode *code) {
    memset(code, 0, sizeof(*code));
    unsigned long long mtime = 0;
    dodai_mtime_ns(PATH(GAME_DLL_NEW), &mtime);

    /* Copy so the compiler can overwrite GAME_DLL_NEW while we run. dodai
       removes the old copy first: macOS validates code signatures per vnode,
       and overwriting a previously-mapped dylib in place poisons it (dlopen
       is then SIGKILLed with "code signature invalid"). A fresh vnode per
       copy avoids that; harmless on the other platforms. */
    b32 copied = 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        if (dodai_copy_file(PATH(GAME_DLL_NEW), michi_from_cstr(g_dll_loaded_path))) {
            copied = 1;
            break;
        }
        dodai_sleep_ms(100);
    }
    if (!copied) {
        dodai_log("reload: cannot copy %s", GAME_DLL_NEW);
        return 0;
    }

    code->lib = dodai_lib_open(michi_from_cstr(g_dll_loaded_path));
    if (!code->lib) {
        dodai_log("reload: dodai_lib_open failed");
        return 0;
    }
    code->update = (GameUpdateAndRenderFn *)
        dodai_lib_symbol(code->lib, "game_update_and_render");
    const char **layout_p = (const char **)
        dodai_lib_symbol(code->lib, "seni_layout");
    if (!code->update || !layout_p || !*layout_p) {
        dodai_log("reload: missing exports in game dll");
        game_unload(code);
        return 0;
    }
    /* The layout string lives in the dll image; copy it out so it survives
       the unload that precedes the next load. */
    code->layout = strdup(*layout_p);
    code->src_mtime = mtime;
    return 1;
}

/* Runs the seni pipeline: diff old (embedded) layout against the header on
   disk, generate migration C, compile it with gcc, run it on the hot block.
   Returns 0 on any failure -- the caller then keeps the old dll running. */
static b32 migrate_hot_memory(const char *old_layout, const char *new_layout,
                              void *hot, u64 hot_size,
                              void *scratch, u64 scratch_size,
                              SeniReloadStatus *status, u32 override) {
    static char arena_buf[1u << 20];
    arena a;
    create_arena(&a, arena_buf, sizeof(arena_buf));

    /* every run starts clean: stale questions from a previous attempt must
       not outlive the diff that raised them */
    status->question_count = 0;

    /* seni's parser takes non-const buffers; hand it arena copies. */
    char *old_copy = arena_copy_string(&a, old_layout, strlen(old_layout));
    char *new_copy = arena_copy_string(&a, new_layout, strlen(new_layout));
    if (!old_copy || !new_copy) {
        dodai_log("migrate: arena exhausted");
        return 0;
    }

    diff_result dr = diff_structs(&a, old_copy, new_copy);
    if (dr.err) {
        dodai_log("migrate: diff failed: %s", dr.err);
        return 0;
    }
    switch (migrate_decide(&dr, override)) {
    case MIG_NOOP:
        return 1; /* layouts differ textually but not structurally */
    case MIG_REFUSE_QUESTIONS: {
        /* the diff is ambiguous (possible renames). seni's questions are
           advisory -- the ops would zero the new fields -- but acting on a
           guess silently loses data, so the platform refuses the reload and
           parks the questions for the seni UI panel. the developer answers
           in the state header (or uses the panel / --on-ambiguous escape). */
        u32 n = seni_fill_questions(status, dr.questions, dr.question_count);
        u32 q;
        for (q = 0; q < n; q++) {
            dodai_log("migrate: %s", status->questions[q].message);
        }
        if (dr.question_count > n) {
            dodai_log("migrate: %llu more questions not shown (panel cap %d)",
                    (unsigned long long)(dr.question_count - n),
                    SENI_STATUS_MAX_QUESTIONS);
        }
        dodai_log("migrate: reload refused until questions are answered "
                "(annotate " GAME_STATE_HDR ", or use the panel / --on-ambiguous)");
        return 0;
    }
    case MIG_RELOAD_COLD:
        memset(hot, 0, hot_size);
        status->question_count = 0;
        dodai_log("migrate: ambiguous reload -- hot state discarded, loading "
                  "new layout cold (escape hatch)");
        return 1;
    case MIG_DROP_ALL: {
        /* answer every question as a drop on a scratch copy of the new header
           (never the on-disk file -- a teammate's escape must not rewrite
           shared source). Iterate the full diff question list, not the
           16-clamped mailbox, so truncation cannot hide a dropped field. */
        char *dropped = new_copy;
        size_t i;
        for (i = 0; i < dr.question_count; i++) {
            annotate_result an = seni_answer_annotate(&a, SENI_ANSWER_DROPPED,
                    dr.questions[i].struct_name, dr.questions[i].removed, "",
                    dropped);
            if (an.err) {
                dodai_log("migrate: drop-all annotate failed: %s", an.err);
                return 0;
            }
            dropped = an.code;
        }
        dr = diff_structs(&a, old_copy, dropped);
        if (dr.err) {
            dodai_log("migrate: drop-all re-diff failed: %s", dr.err);
            return 0;
        }
        if (dr.question_count > 0) {
            dodai_log("migrate: drop-all still ambiguous (%llu) -- aborting",
                    (unsigned long long)dr.question_count);
            return 0;
        }
        status->question_count = 0;
        dodai_log("migrate: ambiguous reload -- dropped %llu ambiguous field(s), "
                  "migrating the rest (escape hatch)", (unsigned long long)i);
        if (dr.value.struct_count == 0) {
            return 1; /* everything dropped to a textual-only change */
        }
        break; /* fall through to generate + run with the re-diffed result */
    }
    case MIG_PROCEED:
    default:
        break; /* fall through to generate + run */
    }

    generate_result gr = generate_migration(&a, dr.value);
    if (gr.err) {
        dodai_log("migrate: codegen failed: %s", gr.err);
        return 0;
    }

    if (!dodai_write_file(michi_from_cstr(g_mig_c_path), gr.code, strlen(gr.code))) {
        dodai_log("migrate: cannot write %s", g_mig_c_path);
        return 0;
    }
    /* kaji owns compilation: it uses the project's resolved cc tool and
       removes the lib first (codesign-vnode rule). */
    if (!g_forge) {
        dodai_log("migrate: no forge -- cannot compile the migration");
        return 0;
    }
    if (kaji_compile_shared(g_forge, g_mig_c_path, g_mig_dll_path,
                            g_mig_err_path, NULL) != 0) {
        dodai_log("migrate: compile failed, see the per-instance mig_errors log");
        return 0;
    }

    void *mig = dodai_lib_open(michi_from_cstr(g_mig_dll_path));
    if (!mig) {
        dodai_log("migrate: cannot load %s", g_mig_dll_path);
        return 0;
    }
    typedef void (*migrate_fn)(void *old_p, void *new_p, size_t count);
    migrate_fn migrate = (migrate_fn)dodai_lib_symbol(mig, "migrate_game_state");
    /* the generated module also exports the compiler-true size of the new
       struct -- copy exactly that, never the whole hot block: anything that
       might one day live above game_state in hot memory must survive */
    const size_t *new_size_p =
        (const size_t *)dodai_lib_symbol(mig, "migrate_game_state_new_size");
    if (!migrate || !new_size_p) {
        dodai_log("migrate: missing exports in %s", g_mig_dll_path);
        dodai_lib_close(mig);
        return 0;
    }
    u64 new_size = (u64)*new_size_p;
    if (!migrate_size_fits(new_size, hot_size, scratch_size)) {
        dodai_log("migrate: new game_state size %llu does not fit (hot %llu, scratch %llu)",
                (unsigned long long)new_size, (unsigned long long)hot_size,
                (unsigned long long)scratch_size);
        dodai_lib_close(mig);
        return 0;
    }

    memset(scratch, 0, new_size);
    migrate(hot, scratch, 1);
    memcpy(hot, scratch, new_size);
    dodai_lib_close(mig);
    dodai_log("migrate: game_state migrated to new layout (%llu bytes)",
            (unsigned long long)new_size);
    return 1;
}

/* A successful migration makes every SENI_WAS in the header inert (the next
   diff's old layout IS this header; all fields match by name), so strip the
   served history -- the header keeps only live intent. The save triggers
   one more rebuild whose diff is structurally empty; it settles at once. */
static void seni_strip_consumed_was(void) {
    static char arena_buf[1u << 20];
    arena a;
    size_t len = 0;
    char *hdr = dodai_read_file(PATH(GAME_STATE_HDR), &len);
    if (!hdr) {
        return;
    }
    create_arena(&a, arena_buf, sizeof(arena_buf));
    char *copy = arena_copy_string(&a, hdr, len);
    free(hdr);
    if (!copy) {
        return;
    }
    annotate_result r = strip_was(&a, copy);
    if (r.err || r.code == copy) {
        return; /* error or nothing to strip */
    }
    if (dodai_write_file(PATH(GAME_STATE_HDR), r.code, strlen(r.code))) {
        dodai_log("seni: consumed SENI_WAS annotations stripped from " GAME_STATE_HDR);
    }
}

/* Consumes answers the seni UI panel wrote into the mailbox: inserts the
   matching annotation into GAME_STATE_HDR -- the exact edit the developer
   would have typed -- and saves it. The save triggers the watcher rebuild,
   which retries the reload; the fresh diff has no questions and the panel
   clears. */
static void seni_apply_answers(SeniReloadStatus *status) {
    for (u32 q = 0; q < status->question_count; q++) {
        SeniQuestion *sq = &status->questions[q];
        if (sq->answer != SENI_ANSWER_RENAME && sq->answer != SENI_ANSWER_DROPPED) {
            continue;
        }
        u32 answer = sq->answer;
        sq->answer = SENI_ANSWER_APPLIED; /* one attempt per click, even on failure */

        static char arena_buf[1u << 20];
        arena a;
        create_arena(&a, arena_buf, sizeof(arena_buf));

        size_t hdr_len = 0;
        char *hdr = dodai_read_file(PATH(GAME_STATE_HDR), &hdr_len);
        if (!hdr) {
            dodai_log("seni: cannot read %s", GAME_STATE_HDR);
            continue;
        }
        char *copy = arena_copy_string(&a, hdr, hdr_len);
        free(hdr);
        if (!copy) {
            dodai_log("seni: arena exhausted reading %s", GAME_STATE_HDR);
            continue;
        }

        annotate_result an = seni_answer_annotate(&a, answer, sq->struct_name,
                                                  sq->removed, sq->added, copy);
        if (an.err) {
            dodai_log("seni: %s", an.err);
            continue;
        }
        if (!dodai_write_file(PATH(GAME_STATE_HDR), an.code, strlen(an.code))) {
            dodai_log("seni: cannot write %s", GAME_STATE_HDR);
            continue;
        }
        if (answer == SENI_ANSWER_RENAME) {
            dodai_log("seni: answered rename %s.%s <- %s; %s annotated, rebuilding",
                    sq->struct_name, sq->added, sq->removed, GAME_STATE_HDR);
        } else {
            dodai_log("seni: answered removal of %s.%s; %s annotated, rebuilding",
                    sq->struct_name, sq->removed, GAME_STATE_HDR);
        }
    }
}

/* The forge: every build (the dev dll, ship bundles, any generated target)
   goes through kaji. Two run slots: the dev dll rebuild that kansi's change
   edge triggers, and whatever profile the editor/CLI asked for -- a ship
   build never blocks the gameplay rebuild loop. */
static Project g_project;
static kaji *g_forge;
static kaji_run g_game_run;   /* dev dll rebuilds (kansi edge) */
static kaji_run g_profile_run; /* editor/CLI requested targets */
static int g_build_state; /* 0 idle/ok, 1 running, 2 failed */
static u32 g_on_ambiguous = SENI_OVERRIDE_NONE; /* --on-ambiguous session policy */

/* --build [target]: headless CLI mode. Builds the named kaji target
   (default: ship), forwards its exit status, never opens a window:
       meikyu --build             # ship
       meikyu --build game        # the reloadable dll */
static int run_build_cli(const char *target) {
    if (!target) {
        target = "ship";
    }
    if (!g_forge) {
        fprintf(stderr, "build: no forge (missing/broken generated config)\n");
        return 1;
    }
    fprintf(stderr, "build: target '%s'\n", target);
    return kaji_build(g_forge, target, 1);
}

static b32 platform_run_build_profile(const char *profile) {
    if (!g_forge) {
        dodai_log("build: no forge (missing/broken generated config)");
        return 0;
    }
    if (!kaji_build_async(g_forge, profile, &g_profile_run, 1)) {
        dodai_log("build: cannot start '%s' (unknown target or build running?)",
                profile ? profile : "(null)");
        return 0;
    }
    g_build_state = 1;
    dodai_log("build: target '%s' started (log: %s)", profile, g_profile_run.log_path);
    return 1;
}

static int platform_build_status(void) {
    return g_build_state;
}

static void platform_build_poll(void) {
    switch (kaji_run_poll(g_forge, &g_profile_run)) {
    case KAJI_DONE:
        g_build_state = 0;
        dodai_log("build: done");
        break;
    case KAJI_FAILED:
        g_build_state = 2;
        dodai_log("build: FAILED (exit %d), see %s",
                g_profile_run.exit_code, g_profile_run.log_path);
        break;
    default:
        break;
    }
}

/* ---- project picker ----------------------------------------------------
   Shown only when no project was found (installed exe, bare .app launch).
   The host has no renderer -- the dll it would come from loads only after a
   project is open -- so the picker is native dialogs via dodai_video: a
   message box listing recent projects as buttons, plus Browse... (native
   folder dialog). Recents live in <pref dir>/recent_projects.txt, most
   recent first. */

#define PICKER_MAX_RECENTS   8
#define PICKER_SHOWN_RECENTS 4

static char g_recents[PICKER_MAX_RECENTS][1024];
static int  g_recent_count;

static const char *recents_file(void) {
    static char path[1024];
    if (!path[0]) {
        michi_buf pb;
        michi_buf_reset(&pb);
        if (!dodai_pref_path(ITO("andres"), ITO("meikyu"), &pb)) {
            return NULL;
        }
        snprintf(path, sizeof(path), "%srecent_projects.txt", michi_cstr(&pb));
    }
    return path;
}

static void recents_load(void) {
    const char *file = recents_file();
    g_recent_count = 0;
    if (!file) {
        return;
    }
    size_t len = 0;
    char *text = dodai_read_file(michi_from_cstr(file), &len);
    if (!text) {
        return;
    }
    ito rest = { text, len };
    while (rest.len && g_recent_count < PICKER_MAX_RECENTS) {
        ito line = ito_next_line(&rest);
        if (line.len) {
            ito_copy(g_recents[g_recent_count++], sizeof(g_recents[0]), line);
        }
    }
    free(text);
}

static void recents_save(void) {
    const char *file = recents_file();
    if (!file) {
        return;
    }
    char storage[PICKER_MAX_RECENTS * 1024];
    ito_buf b;
    ito_buf_init(&b, storage, sizeof(storage));
    for (int i = 0; i < g_recent_count; i++) {
        ito_buf_appendf(&b, ITO_FMT "\n", ITO_ARG(ito_from(g_recents[i])));
    }
    dodai_write_file(michi_from_cstr(file), b.buf, b.len);
}

static void recents_remove(int idx) {
    for (int i = idx; i < g_recent_count - 1; i++) {
        memcpy(g_recents[i], g_recents[i + 1], sizeof(g_recents[0]));
    }
    g_recent_count--;
}

/* Records the project at `dir` (any path; stored absolute) as most recent.
   Called on every successful open -- picker, --path, or exe anchor. */
static void recents_add(const char *dir) {
    michi_buf ab;
    michi_buf_reset(&ab);
    if (dodai_absolute_path(michi_from_cstr(dir), &ab) != 0) {
        return;
    }
    const char *abs = michi_cstr(&ab);
    for (int i = 0; i < g_recent_count; i++) {
        if (strcmp(g_recents[i], abs) == 0) {
            recents_remove(i);
            break;
        }
    }
    if (g_recent_count == PICKER_MAX_RECENTS) {
        g_recent_count--;
    }
    for (int i = g_recent_count; i > 0; i--) {
        memcpy(g_recents[i], g_recents[i - 1], sizeof(g_recents[0]));
    }
    snprintf(g_recents[0], sizeof(g_recents[0]), "%s", abs);
    g_recent_count++;
    recents_save();
}

static b32 project_dir_valid(const char *dir) {
    char marker[1100];
    unsigned long long m;
    snprintf(marker, sizeof(marker), "%s/" PROJECT_MARKER, dir);
    return dodai_mtime_ns(michi_from_cstr(marker), &m) != 0;
}

static const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) {
        slash = bslash;
    }
#endif
    return slash && slash[1] ? slash + 1 : path;
}

/* Returns 1 with the cwd inside the chosen project; 0 = user quit. */
static b32 picker_run(void) {
    const char *note = "";
    recents_load();
    for (;;) {
        /* drop recents that stopped being projects before offering them */
        for (int i = 0; i < g_recent_count;) {
            if (!project_dir_valid(g_recents[i])) {
                recents_remove(i);
            } else {
                i++;
            }
        }

        int shown = g_recent_count < PICKER_SHOWN_RECENTS
                  ? g_recent_count : PICKER_SHOWN_RECENTS;
        const char *btns[PICKER_SHOWN_RECENTS + 2];
        int nbtns = 0;
        for (int i = 0; i < shown; i++) {
            btns[nbtns++] = path_basename(g_recents[i]);
        }
        int browse_idx = nbtns;
        btns[nbtns++] = "Browse...";
        int quit_idx = nbtns;
        btns[nbtns++] = "Quit";

        /* fits up to PICKER_SHOWN_RECENTS absolute paths (~1KB each) plus
           the header; matches dodai_message_box's msg cap */
        char msg[5120];
        size_t off = (size_t)snprintf(msg, sizeof(msg),
            "%sA project is any folder holding a " PROJECT_MARKER ".\n", note);
        if (shown) {
            off += (size_t)snprintf(msg + off, sizeof(msg) - off,
                                    "\nRecent:\n");
            for (int i = 0; i < shown; i++) {
                off += (size_t)snprintf(msg + off, sizeof(msg) - off,
                                        "  %s\n", g_recents[i]);
            }
        }

        int hit = dodai_message_box(ITO("meikyu - open project"),
                                    ito_from(msg),
                                    btns, nbtns, browse_idx, quit_idx);
        if (hit == quit_idx) {
            return 0; /* closed or Quit */
        }

        const char *dir;
        char browse[1024];
        if (hit == browse_idx) {
            michi_buf bb;
            michi_buf_reset(&bb);
            if (!dodai_folder_dialog(&bb)) {
                continue; /* cancelled the folder dialog */
            }
            snprintf(browse, sizeof(browse), "%s", michi_cstr(&bb));
            dir = browse;
        } else {
            dir = g_recents[hit];
        }

        if (!project_dir_valid(dir)) {
            note = "Not a project: no " PROJECT_MARKER " there.\n\n";
            continue;
        }
        if (!dodai_chdir(michi_from_cstr(dir))) {
            note = "Cannot enter that folder.\n\n";
            continue;
        }
        return 1;
    }
}

/* --print-host-srcs: prints build.manifest's host_src entries, sorted, one per
   line. The drift guard (test_manifest_drift.sh) diffs this against cmake's
   compiled source list, catching the "added a .c to one but not the other"
   divergence this whole mechanism exists to prevent. */
static int print_host_srcs(void) {
    michi_buf mb;
    char up[1024];
    static char storage[1u << 16];
    arena a;
    char *txt;
    size_t len = 0;
    ito text, err;
    BuildManifest m;
    BuildPlatform bp;
    ito os;
    const char *items[BM_MAX_HOST_SRC + 1];
    int count, i, j;

    michi_buf_reset(&mb);
    if (!dodai_exe_dir(&mb)) {
        fprintf(stderr, "cannot locate exe\n");
        return 1;
    }
    snprintf(up, sizeof(up), "%s../build.manifest", michi_cstr(&mb));
    michi_buf_reset(&mb);
    if (dodai_absolute_path(michi_from_cstr(up), &mb) != 0) {
        fprintf(stderr, "cannot resolve %s\n", up);
        return 1;
    }
    txt = dodai_read_file(michi_view(&mb), &len);
    if (!txt) {
        fprintf(stderr, "cannot read build.manifest\n");
        return 1;
    }
    create_arena(&a, storage, sizeof(storage));
    text.ptr = arena_copy_string(&a, txt, len);
    text.len = text.ptr ? len : 0;
    free(txt);
    err.ptr = 0; err.len = 0;
    if (!build_manifest_parse(&a, text, &m, &err)) {
        fprintf(stderr, "manifest parse error: %.*s\n", (int)err.len, err.ptr);
        return 1;
    }
    count = 0;
    for (i = 0; i < m.host_src_count; i++) {
        items[count++] = arena_copy_string(&a, m.host_src[i].ptr, m.host_src[i].len);
    }
    /* the per-OS dodai source is appended (not a host_src line) -- cmake
       compiles it too, so the drift guard's lists must include it. */
    os = dodai_is_macos() ? ITO("mac")
       : dodai_is_linux() ? ITO("linux") : ITO("windows");
    if (build_manifest_select(&m, os, &bp, &err)) {
        items[count++] = arena_sprintf(&a, "lib/dodai/%.*s",
                                       (int)bp.dodai_src.len, bp.dodai_src.ptr);
    }
    for (i = 1; i < count; i++) { /* insertion sort, small N */
        const char *key = items[i];
        for (j = i - 1; j >= 0 && strcmp(items[j], key) > 0; j--) {
            items[j + 1] = items[j];
        }
        items[j + 1] = key;
    }
    for (i = 0; i < count; i++) {
        printf("%s\n", items[i]);
    }
    return 0;
}

/* --install: build the ship bundle, then assemble a launchable macOS .app in
   /Applications. The ship dir is already self-contained -- the ship exe chdirs
   to its own dir and loads assets + build/*.spv from there -- so the .app is
   that dir dropped under Contents/MacOS plus an Info.plist that makes Finder
   and Spotlight treat it as an app. */
static int run_install(void) {
#ifndef __APPLE__
    dodai_log("install: only macOS (.app to /Applications) is supported for now; "
              "ship bundle is at " PLATFORM_BUILD_DIR "/ship");
    return 1;
#else
    const char *name = g_project.name;
    char ship[512], app[600], macos[680], plist_path[760], seed[760], plist[1200];

    if (run_build_cli("ship") != 0) {
        return 1; /* build logged the cause */
    }
    snprintf(ship, sizeof(ship), "%s/ship", PLATFORM_BUILD_DIR);
    snprintf(app, sizeof(app), "/Applications/%s.app", name);
    snprintf(macos, sizeof(macos), "%s/Contents/MacOS", app);

    /* make Contents/MacOS (make_dirs_for builds the chain up to a file), then
       copy the whole self-contained ship dir into it. */
    snprintf(seed, sizeof(seed), "%s/.seed", macos);
    dodai_make_dirs_for(michi_from_cstr(seed));
    if (!dodai_copy_dir(michi_from_cstr(ship), michi_from_cstr(macos))) {
        dodai_log("install: cannot copy %s -> %s", ship, macos);
        return 1;
    }
    /* dodai's file copy does not preserve the exec bit; without it launchd
       refuses to spawn the bundle (Launch failed, errno 111). */
    {
        char exe[800];
        snprintf(exe, sizeof(exe), "%s/%s", macos, name);
        if (chmod(exe, 0755) != 0) {
            dodai_log("install: cannot chmod +x %s", exe);
            return 1;
        }
    }

    snprintf(plist_path, sizeof(plist_path), "%s/Contents/Info.plist", app);
    snprintf(plist, sizeof(plist),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n<dict>\n"
        "  <key>CFBundleExecutable</key><string>%s</string>\n"
        "  <key>CFBundleIdentifier</key><string>com.meikyu.%s</string>\n"
        "  <key>CFBundleName</key><string>%s</string>\n"
        "  <key>CFBundlePackageType</key><string>APPL</string>\n"
        "  <key>CFBundleVersion</key><string>" MEIKYU_VERSION "</string>\n"
        "  <key>NSHighResolutionCapable</key><true/>\n"
        "</dict>\n</plist>\n",
        name, name, name);
    if (!dodai_write_file(michi_from_cstr(plist_path), plist, strlen(plist))) {
        dodai_log("install: cannot write %s", plist_path);
        return 1;
    }
    dodai_log("install: %s", app);
    return 0;
#endif
}

int main(int argc, char *argv[]) {
    /* CLI: meikyu [--path <project>] [--build [target]] [--install] */
    const char *project_path = NULL;
    const char *build_target = NULL;
    b32 build_mode = 0;
    b32 install_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-host-srcs") == 0) {
            return print_host_srcs(); /* drift guard hook; no window, no project */
        } else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
            project_path = argv[++i];
        } else if (strcmp(argv[i], "--build") == 0) {
            build_mode = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                build_target = argv[++i];
            }
        } else if (strcmp(argv[i], "--install") == 0) {
            install_mode = 1; /* headless: build ship + assemble the .app */
        } else if (strncmp(argv[i], "--on-ambiguous=", 15) == 0) {
            const char *v = argv[i] + 15;
            if (strcmp(v, "cold") == 0) {
                g_on_ambiguous = SENI_OVERRIDE_RELOAD_COLD;
            } else if (strcmp(v, "drop") == 0) {
                g_on_ambiguous = SENI_OVERRIDE_DROP_ALL;
            } else {
                fprintf(stderr, "--on-ambiguous: expected 'cold' or 'drop', got '%s'\n", v);
                return 1;
            }
        }
    }

    /* All paths (dlls, shaders, assets, gcc spawn) are relative to the
       project root. A project is any directory holding a project.meikyu
       (the marker, godot's project.godot). Resolution: --path, then the
       cwd, then the picker. The exe lives in the engine's build dir,
       never inside a project. */
    unsigned long long marker;
    if (project_path) {
        if (!dodai_chdir(michi_from_cstr(project_path))) {
            dodai_log("cannot chdir to project '%s'", project_path);
            return 1;
        }
        if (!dodai_mtime_ns(PATH(PROJECT_MARKER), &marker)) {
            dodai_log("'%s' is not a project (no " PROJECT_MARKER ")",
                      project_path);
            return 1;
        }
    } else if (!dodai_mtime_ns(PATH(PROJECT_MARKER), &marker)) {
        if (build_mode || install_mode) {
            /* headless: no picker, fail fast */
            fprintf(stderr, "build: no project here (no " PROJECT_MARKER
                    "); run from a project dir or pass --path\n");
            return 1;
        }
        /* picker (godot-style). Init video first -- the folder dialog
           needs it on some platforms; the later open just refcounts. */
        dodai_log("no project in the cwd, opening picker");
        if (!dodai_video_init()) {
            return 1; /* dodai_video_init logged the detail */
        }
        if (!picker_run()) {
            return 0; /* user quit the picker */
        }
    }
    recents_add(".");

    instance_paths_init();

    if (!project_open(&g_project)) {
        return 1; /* project_open logged the cause */
    }

    {
        char kaji_err[256];
        g_forge = kaji_load(g_project.kaji_cfg, kaji_err, sizeof(kaji_err));
        if (!g_forge) {
            dodai_log("kaji disabled: %s", kaji_err);
        }
    }

    if (install_mode) {
        return run_install();
    }
    if (build_mode) {
        return run_build_cli(build_target);
    }

    char title[300];
    snprintf(title, sizeof(title), "meikyu - %s", g_project.name);
    DodaiVideo video = { 0 };
    if (!dodai_video_open(ito_from(title), 1280, 720, 1, &video)) {
        return 1; /* dodai_video_open logged the detail */
    }

    GpuContext gpu_context = { video.window, video.device };
    PlatformApi api = { &gpu_context, dodai_log,
                        platform_run_build_profile, platform_build_status };

    PlatformMemory memory = { 0 };
    memory.hot_size = HOT_SIZE;
    memory.hot = calloc(1, HOT_SIZE);
    memory.transient_size = TRANSIENT_SIZE;
    memory.transient = calloc(1, TRANSIENT_SIZE);
    if (!memory.hot || !memory.transient) {
        dodai_log("memory allocation failed");
        return 1;
    }

    /* Role election: exactly one instance watches the sources and forges
       build/game_new; the rest follow the published dll. Followers retry
       the lock and take over when the builder exits. */
    b32 is_builder = forge_lock_try();
    kansi *watcher = NULL;
    if (is_builder) {
        char kansi_err[256];
        watcher = kansi_start(g_project.kansi_cfg, kansi_err, sizeof(kansi_err));
        if (!watcher) {
            dodai_log("kansi disabled: %s", kansi_err);
        }
    }
    dodai_log("forge role: %s", is_builder ? "builder" : "follower (another instance builds)");
    instance_litter_sweep();

    GameCode game = { 0 };
    if (!game_load(&game)) {
        if (is_builder) {
            /* batteries included: forge the first dll ourselves */
            dodai_log("no game dll yet, forging one...");
            if (!g_forge || kaji_build(g_forge, "game", 1) != 0 || !game_load(&game)) {
                dodai_log("initial game build failed -- try: meikyu --build game");
                /* GUI launches never see the log -- say it in a window
                   (the usual culprit: minimal GUI PATH hides gcc/glslc) */
                const char *btn = "Quit";
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "Initial game build failed.\n\n"
                         "See " PLATFORM_BUILD_DIR "/kaji_game.log in the "
                         "project folder.\n\n"
                         "If launched from Spotlight/Finder: GUI apps get a "
                         "minimal PATH, so gcc or glslc may not be found -- "
                         "try once from a terminal.");
                dodai_message_box(ITO("meikyu - build failed"), ito_from(msg), &btn, 1, 0, 0);
                return 1;
            }
        } else {
            /* the builder is forging it; wait for the publish */
            int waited;
            dodai_log("no game dll yet, waiting for the builder...");
            for (waited = 0; waited < 150 && !game_load(&game); waited++) {
                dodai_sleep_ms(100);
            }
            if (!game.lib) {
                dodai_log("no dll appeared -- is the builder instance stuck?");
                return 1;
            }
        }
    }
    memory.reloaded = 1;

    b32 running = 1;
    u64 last = dodai_ticks_us();
    u64 last_watch_ms = 0;

    while (running) {
        u64 now = dodai_ticks_us();
        GameInput input = { 0 };
        input.dt = (f32)(now - last) / 1.0e6f;
        last = now;

        DodaiInput in;
        dodai_video_poll(&in);
        if (in.quit || in.key_escape) {
            running = 0;
        }
        input.mouse_x = in.mouse_x;
        input.mouse_y = in.mouse_y;
        input.mouse_left = in.mouse_left;

        platform_build_poll();

        /* follower -> builder takeover when the previous builder exits */
        u64 role_ticks = dodai_now_ms();
        static u64 last_lock_try_ms;
        if (!is_builder && role_ticks - last_lock_try_ms > 2000) {
            last_lock_try_ms = role_ticks;
            if (forge_lock_try()) {
                is_builder = 1;
                char kansi_err[256];
                watcher = kansi_start(g_project.kansi_cfg, kansi_err, sizeof(kansi_err));
                if (!watcher) {
                    dodai_log("kansi disabled: %s", kansi_err);
                }
                dodai_log("forge role: promoted to builder");
            }
        }

        /* the dev loop: kansi reports the change, kaji forges the dll, the
           dll watcher below swaps it in */
        if (is_builder && watcher && g_forge) {
            b32 kaji_changed = 0;
            if (kansi_update(watcher) == KANSI_CHANGED) {
                if (g_game_run.active) {
                    /* a rebuild is in flight; kansi will edge again on the
                       next save, and the in-flight result still publishes */
                    dodai_log("forge: change during rebuild, finishing current");
                } else if (!project_regen(&g_project, &kaji_changed)) {
                    dodai_log("forge: regen failed, skipping rebuild");
                } else {
                    if (kaji_changed && !g_profile_run.active) {
                        /* shader set changed: the forge must reload the
                           regenerated config to know the new targets.
                           Skipped while a profile build runs (its kaji_run
                           references the old forge); the cfg on disk is
                           already new, so the reload lands on the next
                           quiet edge. */
                        char kaji_err[256];
                        kaji *fresh = kaji_load(g_project.kaji_cfg, kaji_err,
                                                sizeof(kaji_err));
                        if (fresh) {
                            kaji_free(g_forge);
                            g_forge = fresh;
                            dodai_log("forge: shader set changed, config reloaded");
                        } else {
                            dodai_log("forge: regenerated config rejected: %s",
                                      kaji_err);
                        }
                    }
                    if (kaji_build_async(g_forge, "game", &g_game_run, 1)) {
                        dodai_log("forge: sources changed, rebuilding dll");
                    } else {
                        dodai_log("forge: cannot start dll rebuild");
                    }
                }
            }
            switch (kaji_run_poll(g_forge, &g_game_run)) {
            case KAJI_DONE:
                dodai_log("forge: dll rebuilt");
                break;
            case KAJI_FAILED:
                dodai_log("forge: dll build FAILED (exit %d), see %s",
                        g_game_run.exit_code, g_game_run.log_path);
                break;
            default:
                break;
            }
        }

        /* Watch for a recompiled dll (throttled). */
        u64 ticks = dodai_now_ms();
        if (game.lib && ticks - last_watch_ms > 30) {
            last_watch_ms = ticks;
            unsigned long long mtime = 0;
            dodai_mtime_ns(PATH(GAME_DLL_NEW), &mtime);
            if (mtime != 0 && mtime != game.src_mtime) {
                dodai_log("reload: %s changed", GAME_DLL_NEW);

                size_t hdr_len = 0;
                char *new_layout = dodai_read_file(PATH(GAME_STATE_HDR), &hdr_len);
                b32 ok = new_layout != NULL;
                if (!ok) {
                    dodai_log("reload: cannot read %s", GAME_STATE_HDR);
                }
                if (ok && strcmp(game.layout, new_layout) != 0) {
                    /* GPU may still read transient-built resources; the new
                       dll rebuilds them all anyway. Wait so the scratch use
                       below cannot race in-flight frames. */
                    dodai_gpu_wait_idle(video.device);
                    u32 eff = memory.seni.override ? memory.seni.override
                                                   : g_on_ambiguous;
                    ok = migrate_hot_memory(game.layout, new_layout,
                                            memory.hot, memory.hot_size,
                                            memory.transient,
                                            memory.transient_size,
                                            &memory.seni, eff);
                    memory.seni.override = SENI_OVERRIDE_NONE; /* one-shot */
                    if (ok && is_builder) {
                        /* shared-file edit: one writer only */
                        seni_strip_consumed_was();
                    }
                } else if (ok) {
                    /* identical layouts: pending questions (if any) were
                       answered by reverting the header -- clear them */
                    memory.seni.question_count = 0;
                }
                free(new_layout);

                if (ok) {
                    game_unload(&game);
                    if (game_load(&game)) {
                        memory.reloaded = 1;
                    } else {
                        dodai_log("reload: load failed, retrying next frame");
                    }
                } else {
                    dodai_log("reload: aborted, keeping old dll (state intact)");
                    game.src_mtime = mtime; /* don't retry until next build */
                }
            }
        }
        if (!game.lib) {
            /* A previous reload failed after unload; keep trying. */
            if (game_load(&game)) {
                memory.reloaded = 1;
            } else {
                dodai_sleep_ms(100);
                continue;
            }
        }

        /* apply any disambiguation answers clicked in the seni panel last
           frame: edits + saves the state header, which kicks the rebuild */
        seni_apply_answers(&memory.seni);

        game.update(&memory, &api, &input);
        memory.reloaded = 0;
    }

    kansi_stop(watcher);
    game_unload(&game);
    dodai_remove(michi_from_cstr(g_dll_loaded_path)); /* per-pid copy: ours to clean up */
    dodai_video_close(&video);
    return 0;
}
