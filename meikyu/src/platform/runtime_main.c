/* Shipping host: the engine subset players get. Window, GPU device, memory,
   input, one statically linked game -- nothing else. No dll loading, no
   file watcher, no kansi, no seni, no editor. If host_main.c is the
   workshop, this is the product.

   The game entry is linked statically; the bundle layout mirrors the dev
   tree so game code paths work unchanged: the exe sits at the bundle root
   next to build/ (compiled shaders) and assets/. */

#include <stdlib.h>
#include "dodai.h"
#include "dodai_video.h"

#include "base/base_types.h"
#include "abi/abi_platform.h"
#include "abi/abi_gpu.h"

#define HOT_SIZE       (1u << 20)   /* 1 MB  */
#define TRANSIENT_SIZE (64u << 20)  /* 64 MB */

/* ship builds inject the project name: define GAME_TITLE=\"<name>\" */
#ifndef GAME_TITLE
#define GAME_TITLE "Dungeon"
#endif

/* statically linked from the game's unity build */
GAME_UPDATE_AND_RENDER(game_update_and_render);

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* the exe sits at the bundle root; anchor all relative paths there */
    {
        michi_buf bb;
        michi_buf_reset(&bb);
        if (dodai_exe_dir(&bb) && !dodai_chdir(michi_view(&bb))) {
            dodai_log("warning: cannot chdir to bundle root '%s'", michi_cstr(&bb));
        }
    }

    DodaiVideo video = { 0 };
    if (!dodai_video_open(ito_from(GAME_TITLE), 1280, 720, 0, &video)) {
        return 1; /* dodai_video_open logged the detail */
    }

    GpuContext gpu_context = { video.window, video.device };
    PlatformApi api = { &gpu_context, dodai_log };

    PlatformMemory memory = { 0 };
    memory.hot_size = HOT_SIZE;
    memory.hot = calloc(1, HOT_SIZE);
    memory.transient_size = TRANSIENT_SIZE;
    memory.transient = calloc(1, TRANSIENT_SIZE);
    if (!memory.hot || !memory.transient) {
        dodai_log("memory allocation failed");
        return 1;
    }
    memory.reloaded = 1; /* first frame builds the cold state, exactly once */

    b32 running = 1;
    u64 last = dodai_ticks_us();

    while (running) {
        u64 now = dodai_ticks_us();
        GameInput input = { 0 };
        input.dt = (f32)(now - last) / 1.0e6f;
        last = now;

        DodaiInput in;
        dodai_video_poll(&in);
        if (in.quit) {
            running = 0; /* ship ignores escape -- window close only */
        }
        input.mouse_x = in.mouse_x;
        input.mouse_y = in.mouse_y;
        input.mouse_left = in.mouse_left;

        game_update_and_render(&memory, &api, &input);
        memory.reloaded = 0;
    }

    dodai_video_close(&video);
    return 0;
}
