/* Platform layer: owns the process, window, GPU device, the persistent
   memory blocks, the dll reload loop, and the seni migration driver. Knows
   nothing about the game's types beyond the ABI; must never need recompiling
   while iterating on the game.

   One file for every SDL platform: beyond the handful of definitions below
   (chdir, shared-object names, the migration compile command), everything
   here is SDL and identical on Windows and Linux. */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define platform_chdir _chdir
#else
#include <unistd.h>
#define platform_chdir chdir
#endif

#include "base/base_types.h"
#include "abi/abi_platform.h"
#include "abi/abi_gpu.h"

#include "seni.h"
#include "kansi.h"
#include "kaji.h"

#ifdef _WIN32
#define GAME_DLL_NEW    PLATFORM_BUILD_DIR "/game_new.dll"
#define GAME_DLL_LOADED PLATFORM_BUILD_DIR "/game_loaded.dll"
#define MIG_DLL         PLATFORM_BUILD_DIR "/mig.dll"
#define MIG_COMPILE     "gcc -shared " MIG_C " -o " MIG_DLL " 2> " PLATFORM_BUILD_DIR "/mig_errors.log"
#else
#define GAME_DLL_NEW    PLATFORM_BUILD_DIR "/game_new.so"
#define GAME_DLL_LOADED PLATFORM_BUILD_DIR "/game_loaded.so"
#define MIG_DLL         PLATFORM_BUILD_DIR "/mig.so"
#define MIG_COMPILE     "gcc -shared -fPIC " MIG_C " -o " MIG_DLL " 2> " PLATFORM_BUILD_DIR "/mig_errors.log"
#endif
/* both configs are platform-neutral now: kansi only watches, kaji carries
   the per-OS build knowledge internally */
#define KANSI_CFG       "kansi.cfg"
#define KAJI_CFG        "kaji.cfg"
#define GAME_STATE_HDR  "src/game_state.h"
#define MIG_C           PLATFORM_BUILD_DIR "/mig.c"

#define HOT_SIZE       (1u << 20)   /* 1 MB  */
#define TRANSIENT_SIZE (64u << 20)  /* 64 MB */

typedef struct {
    SDL_SharedObject      *lib;
    GameUpdateAndRenderFn *update;
    char                  *layout;   /* heap copy of the dll's seni_layout */
    SDL_Time               src_mtime; /* mtime of GAME_DLL_NEW when loaded */
} GameCode;

static void platform_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args);
    va_end(args);
}

static SDL_Time file_mtime(const char *path) {
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info)) {
        return 0;
    }
    return info.modify_time;
}

static void game_unload(GameCode *code) {
    if (code->lib) {
        SDL_UnloadObject(code->lib);
    }
    SDL_free(code->layout);
    memset(code, 0, sizeof(*code));
}

static b32 game_load(GameCode *code) {
    memset(code, 0, sizeof(*code));
    SDL_Time mtime = file_mtime(GAME_DLL_NEW);

    /* Copy so the compiler can overwrite GAME_DLL_NEW while we run. */
    b32 copied = 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        if (SDL_CopyFile(GAME_DLL_NEW, GAME_DLL_LOADED)) {
            copied = 1;
            break;
        }
        SDL_Delay(100);
    }
    if (!copied) {
        SDL_Log("reload: cannot copy %s: %s", GAME_DLL_NEW, SDL_GetError());
        return 0;
    }

    code->lib = SDL_LoadObject(GAME_DLL_LOADED);
    if (!code->lib) {
        SDL_Log("reload: SDL_LoadObject failed: %s", SDL_GetError());
        return 0;
    }
    code->update = (GameUpdateAndRenderFn *)
        SDL_LoadFunction(code->lib, "game_update_and_render");
    const char **layout_p = (const char **)
        SDL_LoadFunction(code->lib, "seni_layout");
    if (!code->update || !layout_p || !*layout_p) {
        SDL_Log("reload: missing exports in game dll: %s", SDL_GetError());
        game_unload(code);
        return 0;
    }
    /* The layout string lives in the dll image; copy it out so it survives
       the unload that precedes the next load. */
    code->layout = SDL_strdup(*layout_p);
    code->src_mtime = mtime;
    return 1;
}

/* Runs the seni pipeline: diff old (embedded) layout against the header on
   disk, generate migration C, compile it with gcc, run it on the hot block.
   Returns 0 on any failure -- the caller then keeps the old dll running. */
static b32 migrate_hot_memory(const char *old_layout, const char *new_layout,
                              void *hot, u64 hot_size,
                              void *scratch, u64 scratch_size,
                              SeniReloadStatus *status) {
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
        SDL_Log("migrate: arena exhausted");
        return 0;
    }

    diff_result dr = diff_structs(&a, old_copy, new_copy);
    if (dr.err) {
        SDL_Log("migrate: diff failed: %s", dr.err);
        return 0;
    }
    if (dr.question_count > 0) {
        /* the diff is ambiguous (possible renames). seni's questions are
           advisory -- the ops would zero the new fields -- but acting on a
           guess silently loses data, so the platform refuses the reload and
           parks the questions for the seni UI panel. the developer answers
           in the state header; the rebuild retries the reload. */
        u64 n = dr.question_count < SENI_STATUS_MAX_QUESTIONS
              ? dr.question_count : SENI_STATUS_MAX_QUESTIONS;
        for (u64 q = 0; q < n; q++) {
            SeniQuestion *sq = &status->questions[q];
            SDL_strlcpy(sq->message, dr.questions[q].message, SENI_STATUS_MSG_MAX);
            SDL_strlcpy(sq->struct_name, dr.questions[q].struct_name, SENI_STATUS_NAME_MAX);
            SDL_strlcpy(sq->removed, dr.questions[q].removed, SENI_STATUS_NAME_MAX);
            SDL_strlcpy(sq->added, dr.questions[q].added, SENI_STATUS_NAME_MAX);
            sq->answer = SENI_ANSWER_NONE;
            SDL_Log("migrate: %s", dr.questions[q].message);
        }
        if (dr.question_count > n) {
            SDL_Log("migrate: %llu more questions not shown (panel cap %d)",
                    (unsigned long long)(dr.question_count - n),
                    SENI_STATUS_MAX_QUESTIONS);
        }
        status->question_count = (u32)n;
        SDL_Log("migrate: reload refused until questions are answered "
                "(annotate " GAME_STATE_HDR ")");
        return 0;
    }
    if (dr.value.struct_count == 0) {
        return 1; /* layouts differ textually but not structurally */
    }

    generate_result gr = generate_migration(&a, dr.value);
    if (gr.err) {
        SDL_Log("migrate: codegen failed: %s", gr.err);
        return 0;
    }

    if (!SDL_SaveFile(MIG_C, gr.code, strlen(gr.code))) {
        SDL_Log("migrate: cannot write %s: %s", MIG_C, SDL_GetError());
        return 0;
    }
    if (system(MIG_COMPILE) != 0) {
        SDL_Log("migrate: gcc failed, see build/mig_errors.log");
        return 0;
    }

    SDL_SharedObject *mig = SDL_LoadObject(MIG_DLL);
    if (!mig) {
        SDL_Log("migrate: cannot load %s: %s", MIG_DLL, SDL_GetError());
        return 0;
    }
    typedef void (*migrate_fn)(void *old_p, void *new_p, size_t count);
    migrate_fn migrate = (migrate_fn)SDL_LoadFunction(mig, "migrate_game_state");
    /* the generated module also exports the compiler-true size of the new
       struct -- copy exactly that, never the whole hot block: anything that
       might one day live above game_state in hot memory must survive */
    const size_t *new_size_p =
        (const size_t *)SDL_LoadFunction(mig, "migrate_game_state_new_size");
    if (!migrate || !new_size_p) {
        SDL_Log("migrate: missing exports in %s: %s", MIG_DLL, SDL_GetError());
        SDL_UnloadObject(mig);
        return 0;
    }
    u64 new_size = (u64)*new_size_p;
    if (new_size == 0 || new_size > hot_size || new_size > scratch_size) {
        SDL_Log("migrate: new game_state size %llu does not fit (hot %llu, scratch %llu)",
                (unsigned long long)new_size, (unsigned long long)hot_size,
                (unsigned long long)scratch_size);
        SDL_UnloadObject(mig);
        return 0;
    }

    memset(scratch, 0, new_size);
    migrate(hot, scratch, 1);
    memcpy(hot, scratch, new_size);
    SDL_UnloadObject(mig);
    SDL_Log("migrate: game_state migrated to new layout (%llu bytes)",
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
    char *hdr = SDL_LoadFile(GAME_STATE_HDR, &len);
    if (!hdr) {
        return;
    }
    create_arena(&a, arena_buf, sizeof(arena_buf));
    char *copy = arena_copy_string(&a, hdr, len);
    SDL_free(hdr);
    if (!copy) {
        return;
    }
    annotate_result r = strip_was(&a, copy);
    if (r.err || r.code == copy) {
        return; /* error or nothing to strip */
    }
    if (SDL_SaveFile(GAME_STATE_HDR, r.code, strlen(r.code))) {
        SDL_Log("seni: consumed SENI_WAS annotations stripped from " GAME_STATE_HDR);
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
        char *hdr = SDL_LoadFile(GAME_STATE_HDR, &hdr_len);
        if (!hdr) {
            SDL_Log("seni: cannot read %s: %s", GAME_STATE_HDR, SDL_GetError());
            continue;
        }
        char *copy = arena_copy_string(&a, hdr, hdr_len);
        SDL_free(hdr);
        if (!copy) {
            SDL_Log("seni: arena exhausted reading %s", GAME_STATE_HDR);
            continue;
        }

        annotate_result an;
        if (answer == SENI_ANSWER_RENAME) {
            an = annotate_rename(&a, copy, sq->struct_name, sq->removed, sq->added);
        } else {
            an = annotate_dropped(&a, copy, sq->struct_name, sq->removed);
        }
        if (an.err) {
            SDL_Log("seni: %s", an.err);
            continue;
        }
        if (!SDL_SaveFile(GAME_STATE_HDR, an.code, strlen(an.code))) {
            SDL_Log("seni: cannot write %s: %s", GAME_STATE_HDR, SDL_GetError());
            continue;
        }
        if (answer == SENI_ANSWER_RENAME) {
            SDL_Log("seni: answered rename %s.%s <- %s; %s annotated, rebuilding",
                    sq->struct_name, sq->added, sq->removed, GAME_STATE_HDR);
        } else {
            SDL_Log("seni: answered removal of %s.%s; %s annotated, rebuilding",
                    sq->struct_name, sq->removed, GAME_STATE_HDR);
        }
    }
}

/* The forge: every build (the dev dll, ship bundles, anything in kaji.cfg)
   goes through kaji. Two run slots: the dev dll rebuild that kansi's change
   edge triggers, and whatever profile the editor/CLI asked for -- a ship
   build never blocks the gameplay rebuild loop. */
static kaji *g_forge;
static kaji_run g_game_run;   /* dev dll rebuilds (kansi edge) */
static kaji_run g_profile_run; /* editor/CLI requested targets */
static int g_build_state; /* 0 idle/ok, 1 running, 2 failed */

/* --build [target]: headless CLI mode. Builds the named kaji target
   (default: ship), forwards its exit status, never opens a window:
       dungeon --build            # ship
       dungeon --build game       # the reloadable dll */
static int run_build_cli(int argc, char *argv[]) {
    const char *target = argc >= 3 ? argv[2] : "ship";
    if (!g_forge) {
        fprintf(stderr, "build: no forge (missing/broken " KAJI_CFG ")\n");
        return 1;
    }
    fprintf(stderr, "build: target '%s'\n", target);
    return kaji_build(g_forge, target, 1);
}

static b32 platform_run_build_profile(const char *profile) {
    if (!g_forge) {
        SDL_Log("build: no forge (missing/broken " KAJI_CFG ")");
        return 0;
    }
    if (!kaji_build_async(g_forge, profile, &g_profile_run, 1)) {
        SDL_Log("build: cannot start '%s' (unknown target or build running?)",
                profile ? profile : "(null)");
        return 0;
    }
    g_build_state = 1;
    SDL_Log("build: target '%s' started (log: %s)", profile, g_profile_run.log_path);
    return 1;
}

static int platform_build_status(void) {
    return g_build_state;
}

static void platform_build_poll(void) {
    switch (kaji_run_poll(g_forge, &g_profile_run)) {
    case KAJI_DONE:
        g_build_state = 0;
        SDL_Log("build: done");
        break;
    case KAJI_FAILED:
        g_build_state = 2;
        SDL_Log("build: FAILED (exit %d), see %s",
                g_profile_run.exit_code, g_profile_run.log_path);
        break;
    default:
        break;
    }
}

int main(int argc, char *argv[]) {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);

    /* All paths (dlls, shaders, assets, gcc spawn) are relative to the
       project root. The exe lives in build/, so anchor the cwd to the exe's
       parent once -- running via double-click or from any directory then
       behaves identically to running from dungeon1/. */
    {
        const char *base = SDL_GetBasePath(); /* ...\dungeon1\build\ */
        if (base) {
            char root[1024];
            SDL_snprintf(root, sizeof(root), "%s..", base);
            if (platform_chdir(root) != 0) {
                SDL_Log("warning: cannot chdir to project root '%s'", root);
            }
        }
    }

    {
        char kaji_err[256];
        g_forge = kaji_load(KAJI_CFG, kaji_err, sizeof(kaji_err));
        if (!g_forge) {
            SDL_Log("kaji disabled: %s", kaji_err);
        }
    }

    if (argc >= 2 && strcmp(argv[1], "--build") == 0) {
        return run_build_cli(argc, argv);
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("Dungeon - hot reload", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice (vulkan) failed: %s", SDL_GetError());
        return 1;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return 1;
    }

    GpuContext gpu_context = { window, device };
    PlatformApi api = { &gpu_context, platform_log,
                        platform_run_build_profile, platform_build_status };

    PlatformMemory memory = { 0 };
    memory.hot_size = HOT_SIZE;
    memory.hot = calloc(1, HOT_SIZE);
    memory.transient_size = TRANSIENT_SIZE;
    memory.transient = calloc(1, TRANSIENT_SIZE);
    if (!memory.hot || !memory.transient) {
        SDL_Log("memory allocation failed");
        return 1;
    }

    /* kansi: watch the source trees. On its change edge the host asks the
       forge for a fresh dll. Optional -- `dungeon --build game` still works. */
    char kansi_err[256];
    kansi *watcher = kansi_start(KANSI_CFG, kansi_err, sizeof(kansi_err));
    if (!watcher) {
        SDL_Log("kansi disabled: %s", kansi_err);
    }

    GameCode game = { 0 };
    if (!game_load(&game)) {
        /* batteries included: forge the first dll ourselves */
        SDL_Log("no game dll yet, forging one...");
        if (!g_forge || kaji_build(g_forge, "game", 1) != 0 || !game_load(&game)) {
            SDL_Log("initial game build failed -- try: dungeon --build game");
            return 1;
        }
    }
    memory.reloaded = 1;

    b32 running = 1;
    u64 last = SDL_GetPerformanceCounter();
    u64 freq = SDL_GetPerformanceFrequency();
    u64 last_watch_ms = 0;

    while (running) {
        u64 now = SDL_GetPerformanceCounter();
        GameInput input = { 0 };
        input.dt = (f32)(now - last) / (f32)freq;
        last = now;
        {
            float mx = 0, my = 0;
            SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
            input.mouse_x = mx;
            input.mouse_y = my;
            input.mouse_left = (buttons & SDL_BUTTON_LMASK) != 0;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.scancode == SDL_SCANCODE_ESCAPE) {
                running = 0;
            }
        }

        platform_build_poll();

        /* the dev loop: kansi reports the change, kaji forges the dll, the
           dll watcher below swaps it in */
        if (watcher && g_forge) {
            if (kansi_update(watcher) == KANSI_CHANGED) {
                if (g_game_run.active) {
                    /* a rebuild is in flight; kansi will edge again on the
                       next save, and the in-flight result still publishes */
                    SDL_Log("forge: change during rebuild, finishing current");
                } else if (kaji_build_async(g_forge, "game", &g_game_run, 1)) {
                    SDL_Log("forge: sources changed, rebuilding dll");
                } else {
                    SDL_Log("forge: cannot start dll rebuild");
                }
            }
            switch (kaji_run_poll(g_forge, &g_game_run)) {
            case KAJI_DONE:
                SDL_Log("forge: dll rebuilt");
                break;
            case KAJI_FAILED:
                SDL_Log("forge: dll build FAILED (exit %d), see %s",
                        g_game_run.exit_code, g_game_run.log_path);
                break;
            default:
                break;
            }
        }

        /* Watch for a recompiled dll (throttled). */
        u64 ticks = SDL_GetTicks();
        if (game.lib && ticks - last_watch_ms > 30) {
            last_watch_ms = ticks;
            SDL_Time mtime = file_mtime(GAME_DLL_NEW);
            if (mtime != 0 && mtime != game.src_mtime) {
                SDL_Log("reload: %s changed", GAME_DLL_NEW);

                size_t hdr_len = 0;
                char *new_layout = SDL_LoadFile(GAME_STATE_HDR, &hdr_len);
                b32 ok = new_layout != NULL;
                if (!ok) {
                    SDL_Log("reload: cannot read %s: %s", GAME_STATE_HDR,
                            SDL_GetError());
                }
                if (ok && strcmp(game.layout, new_layout) != 0) {
                    /* GPU may still read transient-built resources; the new
                       dll rebuilds them all anyway. Wait so the scratch use
                       below cannot race in-flight frames. */
                    SDL_WaitForGPUIdle(device);
                    ok = migrate_hot_memory(game.layout, new_layout,
                                            memory.hot, memory.hot_size,
                                            memory.transient,
                                            memory.transient_size,
                                            &memory.seni);
                    if (ok) {
                        seni_strip_consumed_was();
                    }
                } else if (ok) {
                    /* identical layouts: pending questions (if any) were
                       answered by reverting the header -- clear them */
                    memory.seni.question_count = 0;
                }
                SDL_free(new_layout);

                if (ok) {
                    game_unload(&game);
                    if (game_load(&game)) {
                        memory.reloaded = 1;
                    } else {
                        SDL_Log("reload: load failed, retrying next frame");
                    }
                } else {
                    SDL_Log("reload: aborted, keeping old dll (state intact)");
                    game.src_mtime = mtime; /* don't retry until next build */
                }
            }
        }
        if (!game.lib) {
            /* A previous reload failed after unload; keep trying. */
            if (game_load(&game)) {
                memory.reloaded = 1;
            } else {
                SDL_Delay(100);
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
    SDL_WaitForGPUIdle(device);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
