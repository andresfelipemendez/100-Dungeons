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
   UI (seni_panel.c in the seni checkout) shows them; the developer answers
   by annotating the state header (SENI_WAS / SENI_DROPPED), which triggers
   a rebuild and a clean reload. Fixed buffers: this struct crosses the
   exe/dll boundary, so no pointers into either side. */
#define SENI_STATUS_MAX_QUESTIONS 16
#define SENI_STATUS_MSG_MAX 512
typedef struct SeniReloadStatus {
    u32  question_count; /* 0 = nothing pending, reloads flow */
    char questions[SENI_STATUS_MAX_QUESTIONS][SENI_STATUS_MSG_MAX];
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
} PlatformApi;

typedef struct GameInput {
    f32 dt;
    f32 mouse_x, mouse_y; /* pixels, window-relative */
    b32 mouse_left;
} GameInput;

#define GAME_UPDATE_AND_RENDER(name) \
    void name(PlatformMemory *memory, PlatformApi *api, GameInput *input)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFn);

#endif /* ABI_PLATFORM_H */
