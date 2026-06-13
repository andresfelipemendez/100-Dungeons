#ifndef ABI_PLATFORM_H
#define ABI_PLATFORM_H

/* The frozen contract between the platform exe and the game dll. Everything
   here survives a hot reload, so changing this file means "restart, don't
   reload". No SDL (or any backend) types may appear here. */

#include "base/base_types.h"

/* Reload disambiguations pending from seni's layout diff. When the diff
   finds a same-type field removal + addition in one struct it cannot know
   whether that is a rename (data must move) or a removal (data must die);
   the platform refuses the reload and parks the questions here. The game's
   UI (seni_panel.c in the seni checkout) shows them with answer buttons.

   Two-way mailbox: the platform writes the questions; the UI writes
   `answer`; the platform consumes the answer by inserting the matching
   annotation (SENI_WAS / SENI_DROPPED) into the state header on disk --
   the same edit the developer would type by hand, so the header stays the
   single source of intent. Saving the header triggers the rebuild that
   retries the reload and clears the questions. Fixed buffers only: this
   struct crosses the exe/dll boundary. */
#define SENI_STATUS_MAX_QUESTIONS 16
#define SENI_STATUS_MSG_MAX 512
#define SENI_STATUS_NAME_MAX 64

enum {
    SENI_ANSWER_NONE = 0,
    SENI_ANSWER_RENAME = 1,  /* annotate: <added> SENI_WAS(<removed>) */
    SENI_ANSWER_DROPPED = 2, /* annotate: SENI_DROPPED(<removed>) */
    SENI_ANSWER_APPLIED = 3  /* platform wrote the annotation; rebuild pending */
};

typedef struct SeniQuestion {
    char message[SENI_STATUS_MSG_MAX];          /* preformatted, multi-line */
    char struct_name[SENI_STATUS_NAME_MAX];
    char removed[SENI_STATUS_NAME_MAX];
    char added[SENI_STATUS_NAME_MAX];
    u32  answer; /* SENI_ANSWER_*; UI writes, platform consumes */
} SeniQuestion;

typedef struct SeniReloadStatus {
    u32 question_count; /* 0 = nothing pending, reloads flow */
    SeniQuestion questions[SENI_STATUS_MAX_QUESTIONS];
} SeniReloadStatus;

typedef struct PlatformMemory {
    /* seni-migrated region; one game_state lives at offset 0 */
    u64   hot_size;
    void *hot;
    /* scratch; the dll rebuilds its contents from scratch on every reload */
    u64   transient_size;
    void *transient;
    /* true on the first update call after a dll (re)load */
    b32   reloaded;
    /* written by the platform's reload driver, read by the game's UI */
    SeniReloadStatus seni;
} PlatformMemory;

typedef struct PlatformApi {
    /* opaque backend context (window + device); only the render backend
       inside the dll knows its layout (abi_gpu.h) */
    void *gpu_context;
    void (*log)(const char *fmt, ...);
    /* Spawns the build profile script editor/build/<profile> as a detached
       child process (output to the build dir's log). OS specifics live in
       the platform (kaji job runner); reloadable code never calls system()
       for builds. NULL in the shipping host -- callers must check. */
    b32 (*run_build_profile)(const char *profile);
    /* Last spawned build: 0 idle/succeeded, 1 running, 2 failed.
       NULL in the shipping host. */
    int (*build_status)(void);
} PlatformApi;

typedef struct GameInput {
    f32 dt;
    f32 mouse_x, mouse_y; /* pixels, window-relative */
    b32 mouse_left;
} GameInput;

/* Where per-OS build artifacts (game dll, compiled shaders, migration
   scratch) live, relative to the project root. Windows and Linux must not
   share a directory: object formats, cmake caches and .gch files collide. */
#ifdef _WIN32
#define PLATFORM_BUILD_DIR "build"
#elif defined(__APPLE__)
#define PLATFORM_BUILD_DIR "build-mac"
#else
#define PLATFORM_BUILD_DIR "build-linux"
#endif

/* Export decoration for the dll's entry points. */
#ifdef _WIN32
#define GAME_EXPORT __declspec(dllexport)
#else
#define GAME_EXPORT __attribute__((visibility("default")))
#endif

#define GAME_UPDATE_AND_RENDER(name) \
    void name(PlatformMemory *memory, PlatformApi *api, GameInput *input)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFn);

#endif /* ABI_PLATFORM_H */
