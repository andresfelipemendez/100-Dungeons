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

#ifdef _WIN32
#define GAME_DLL_NEW    PLATFORM_BUILD_DIR "/game_new.dll"
#define GAME_DLL_LOADED PLATFORM_BUILD_DIR "/game_loaded.dll"
#define MIG_DLL         PLATFORM_BUILD_DIR "/mig.dll"
#define MIG_COMPILE     "gcc -shared " MIG_C " -o " MIG_DLL " 2> " PLATFORM_BUILD_DIR "/mig_errors.log"
#define KANSI_CFG       "kansi.cfg"
#define RELOAD_HINT     "run reload.bat first"
#else
#define GAME_DLL_NEW    PLATFORM_BUILD_DIR "/game_new.so"
#define GAME_DLL_LOADED PLATFORM_BUILD_DIR "/game_loaded.so"
#define MIG_DLL         PLATFORM_BUILD_DIR "/mig.so"
#define MIG_COMPILE     "gcc -shared -fPIC " MIG_C " -o " MIG_DLL " 2> " PLATFORM_BUILD_DIR "/mig_errors.log"
#define KANSI_CFG       "kansi.linux.cfg"
#define RELOAD_HINT     "run ./reload.sh first"
#endif
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
    if (!migrate) {
        SDL_Log("migrate: migrate_game_state not found: %s", SDL_GetError());
        SDL_UnloadObject(mig);
        return 0;
    }

    u64 copy_size = hot_size < scratch_size ? hot_size : scratch_size;
    memset(scratch, 0, copy_size);
    migrate(hot, scratch, 1);
    memcpy(hot, scratch, copy_size);
    SDL_UnloadObject(mig);
    SDL_Log("migrate: game_state migrated to new layout");
    return 1;
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

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
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
    PlatformApi api = { &gpu_context, platform_log };

    PlatformMemory memory = { 0 };
    memory.hot_size = HOT_SIZE;
    memory.hot = calloc(1, HOT_SIZE);
    memory.transient_size = TRANSIENT_SIZE;
    memory.transient = calloc(1, TRANSIENT_SIZE);
    if (!memory.hot || !memory.transient) {
        SDL_Log("memory allocation failed");
        return 1;
    }

    /* kansi: watch source trees, auto-rebuild the dll. Optional -- without
       a config the manual reload script still works. */
    char kansi_err[256];
    kansi *watcher = kansi_start(KANSI_CFG, kansi_err, sizeof(kansi_err));
    if (!watcher) {
        SDL_Log("kansi disabled: %s", kansi_err);
    }

    GameCode game = { 0 };
    if (!game_load(&game)) {
        SDL_Log("initial game dll load failed -- " RELOAD_HINT);
        return 1;
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

        if (watcher) {
            kansi_status ks = kansi_update(watcher);
            if (ks == KANSI_BUILT) {
                SDL_Log("kansi: dll rebuilt");
            } else if (ks == KANSI_ERROR) {
                SDL_Log("kansi: build failed, see %s", kansi_log_path(watcher));
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
