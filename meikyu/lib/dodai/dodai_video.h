#ifndef DODAI_VIDEO_H
#define DODAI_VIDEO_H

/* dodai_video: host-only window / GPU-device / input / dialog services.
   One SDL-backed implementation (dodai_video_sdl.c) for every OS -- SDL is
   the portability layer here, unlike core dodai's per-OS files.

   Link rule: core dodai = everyone; dodai_video = platform hosts ONLY
   (host_main.c, runtime_main.c). The lib test suites and the game dll must
   never link or include this -- core dodai stays SDL-free on purpose.

   Conventions follow core dodai: 1 = success / 0 = failure; the impl logs
   the SDL error detail itself, callers add context and exit/skip. String
   params are `ito` (first-party string lib); path out-params are `ito_buf`.
   dodai_log's format literal stays const char* (boundary rule) but gains a
   %S conversion that consumes an ito by value. */

#include <stddef.h>
#include "../ito/ito.h"
#include "../michi/michi.h"

/* ---- lifecycle --------------------------------------------------------- */
/* The opened window/device pair, passed as a unit so the two void*s can
   never be swapped at a call site. Fields are opaque (SDL_Window* /
   SDL_GPUDevice* underneath) -- they flow into GpuContext untouched. */
typedef struct {
    void *window;
    void *device;
} DodaiVideo;

/* Init the video subsystem only (needed before dodai_folder_dialog when no
   window exists yet, e.g. the project picker). Refcounted by SDL; calling
   it before dodai_video_open is allowed but not required. */
int  dodai_video_init(void);
/* Window + GPU device (SPIR-V shaders, vulkan driver) + claim the window
   for the device. debug_gpu enables the validation/debug device. The macOS
   /usr/local/lib/libvulkan.dylib loader hint lives inside. */
int  dodai_video_open(ito title, int w, int h, int debug_gpu,
                      DodaiVideo *out);
void dodai_gpu_wait_idle(void *device);
/* Wait idle, release the window from the device, destroy both, quit SDL. */
void dodai_video_close(DodaiVideo *v);

/* ---- per-frame input ---------------------------------------------------- */
typedef struct {
    int   quit;          /* window closed */
    int   key_escape;    /* escape pressed this frame */
    float mouse_x, mouse_y;
    int   mouse_left;
} DodaiInput;
/* Pumps the event queue and samples the mouse. Call once per frame. */
void dodai_video_poll(DodaiInput *out);

/* ---- services ----------------------------------------------------------- */
/* Monotonic high-resolution time in microseconds (perf counter normalized
   once -- callers never divide by a frequency). For frame dt; coarser
   throttles keep using core dodai_now_ms. */
unsigned long long dodai_ticks_us(void);
/* printf-style log line (SDL application log underneath). The signature
   matches PlatformApi.log, so hosts pass it through the ABI directly.
   Supports an extra %S conversion consuming an ito by value (see
   ito_buf_vappendf); standard conversions work as usual. */
void dodai_log(const char *fmt, ...);
/* Per-user writable settings dir for org/app (a path; trailing separator). */
int  dodai_pref_path(ito org, ito app, michi_buf *out);
/* Directory containing the running exe, trailing separator included. */
int  dodai_exe_dir(michi_buf *out);

/* ---- modal dialogs (project picker) -------------------------------------*/
/* Native message box with one button per buttons[i] (buttons stay C
   strings -- SDL_MessageBoxButtonData.text is const char*). Returns the
   hit index; default_idx maps to Return, cancel_idx to Escape and is also
   returned when the box is closed or fails. */
int  dodai_message_box(ito title, ito msg,
                       const char *const *buttons, int n,
                       int default_idx, int cancel_idx);
/* Native folder picker. Blocks, pumping events internally; 0 = cancelled. */
int  dodai_folder_dialog(michi_buf *out);

#endif /* DODAI_VIDEO_H */
