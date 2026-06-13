/* dodai_video: SDL3-backed implementation. The only dodai file that may
   include SDL. See dodai_video.h for the link rule. */

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <unistd.h>  /* access(), F_OK */
#endif

#include "dodai_video.h"

int dodai_video_init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("dodai_video_init: SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    return 1;
}

int dodai_video_open(ito title, int w, int h, int debug_gpu,
                     DodaiVideo *out) {
    char title_c[256];
    if (!ito_copy(title_c, sizeof(title_c), title)) {
        SDL_Log("dodai_video_open: window title truncated to %zu bytes",
                sizeof(title_c) - 1);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) { /* refcounts if already up */
        SDL_Log("dodai_video_open: SDL_Init failed: %s", SDL_GetError());
        return 0;
    }
    SDL_Window *window = SDL_CreateWindow(title_c, w, h, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("dodai_video_open: SDL_CreateWindow failed: %s", SDL_GetError());
        return 0;
    }
#ifdef __APPLE__
    /* modern dyld won't find the Vulkan SDK's loader (/usr/local/lib) from
       a bare dlopen name; point SDL at it explicitly. MoltenVK translates
       to Metal underneath -- the SPIR-V pipeline stays unchanged. */
    if (!SDL_GetHint(SDL_HINT_VULKAN_LIBRARY) &&
        access("/usr/local/lib/libvulkan.dylib", F_OK) == 0) {
        SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "/usr/local/lib/libvulkan.dylib");
    }
#endif
    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,
                                                debug_gpu != 0, "vulkan");
    if (!device) {
        SDL_Log("dodai_video_open: SDL_CreateGPUDevice (vulkan) failed: %s",
                SDL_GetError());
        SDL_DestroyWindow(window);
        return 0;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("dodai_video_open: SDL_ClaimWindowForGPUDevice failed: %s",
                SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        return 0;
    }
    out->window = window;
    out->device = device;
    return 1;
}

void dodai_gpu_wait_idle(void *device) {
    SDL_WaitForGPUIdle((SDL_GPUDevice *)device);
}

void dodai_video_close(DodaiVideo *v) {
    SDL_GPUDevice *d = (SDL_GPUDevice *)v->device;
    SDL_Window *w = (SDL_Window *)v->window;
    if (d) {
        SDL_WaitForGPUIdle(d);
        if (w) {
            SDL_ReleaseWindowFromGPUDevice(d, w);
        }
        SDL_DestroyGPUDevice(d);
    }
    if (w) {
        SDL_DestroyWindow(w);
    }
    SDL_Quit();
}

void dodai_video_poll(DodaiInput *out) {
    memset(out, 0, sizeof(*out));
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            out->quit = 1;
        } else if (event.type == SDL_EVENT_KEY_DOWN &&
                   event.key.scancode == SDL_SCANCODE_ESCAPE) {
            out->key_escape = 1;
        }
    }
    float mx = 0, my = 0;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
    out->mouse_x = mx;
    out->mouse_y = my;
    out->mouse_left = (buttons & SDL_BUTTON_LMASK) != 0;
}

unsigned long long dodai_ticks_us(void) {
    /* split to avoid c * 1e6 overflowing u64 (mac's counter is ns since
       boot: naive multiply wraps after ~5h uptime) */
    unsigned long long c = SDL_GetPerformanceCounter();
    unsigned long long f = SDL_GetPerformanceFrequency();
    return (c / f) * 1000000ull + (c % f) * 1000000ull / f;
}

void dodai_log(const char *fmt, ...) {
    /* render through ito's formatter so %S (an ito by value) works at every
       log site, then hand SDL one finished line */
    char line[2048];
    ito_buf b;
    ito_buf_init(&b, line, sizeof(line));
    va_list args;
    va_start(args, fmt);
    ito_buf_vappendf(&b, fmt, args);
    va_end(args);
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO,
                   "%s", ito_buf_cstr(&b));
}

int dodai_pref_path(ito org, ito app, michi_buf *out) {
    char orgc[128], appc[128];
    if (!ito_copy(orgc, sizeof(orgc), org) ||
        !ito_copy(appc, sizeof(appc), app)) {
        SDL_Log("dodai_pref_path: org/app too long, settings dir may be wrong");
    }
    char *pref = SDL_GetPrefPath(orgc, appc);
    if (!pref) {
        return 0;
    }
    ito_buf_append_c(&out->b, pref);
    SDL_free(pref);
    return !out->b.overflow;
}

int dodai_exe_dir(michi_buf *out) {
    const char *base = SDL_GetBasePath();
    if (!base) {
        return 0;
    }
    ito_buf_append_c(&out->b, base);
    return !out->b.overflow;
}

#define DODAI_MSGBOX_MAX_BUTTONS 16

int dodai_message_box(ito title, ito msg,
                      const char *const *buttons, int n,
                      int default_idx, int cancel_idx) {
    char title_c[256], msg_c[5120];
    if (!ito_copy(title_c, sizeof(title_c), title) ||
        !ito_copy(msg_c, sizeof(msg_c), msg)) {
        SDL_Log("dodai_message_box: title/message truncated");
    }
    SDL_MessageBoxButtonData btns[DODAI_MSGBOX_MAX_BUTTONS];
    if (n > DODAI_MSGBOX_MAX_BUTTONS) {
        /* never clamp silently: a dropped button whose index is default_idx
           or cancel_idx would leave Return/Escape unmapped while its index
           can still be returned -- the caller must shrink the list */
        SDL_Log("dodai_message_box: %d buttons exceeds cap %d, clamping "
                "(default/cancel beyond the cap won't be shown)",
                n, DODAI_MSGBOX_MAX_BUTTONS);
        n = DODAI_MSGBOX_MAX_BUTTONS;
    }
    for (int i = 0; i < n; i++) {
        btns[i].flags = 0;
        if (i == default_idx) {
            btns[i].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        }
        if (i == cancel_idx) {
            btns[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        }
        btns[i].buttonID = i;
        btns[i].text = buttons[i];
    }
    SDL_MessageBoxData mb = { SDL_MESSAGEBOX_INFORMATION, NULL, title_c, msg_c,
                              n, btns, NULL };
    int hit = -1;
    if (!SDL_ShowMessageBox(&mb, &hit) || hit < 0) {
        return cancel_idx;
    }
    return hit;
}

/* The dialog callback may arrive on another thread (SDL_CreateThread on
   windows, D-Bus on linux). `done` is the release/acquire handoff: the
   callback writes dir[] first, then sets done; the main thread only reads
   dir[] after seeing done. Plain ints would race (hoisted load = hang). */
typedef struct {
    SDL_AtomicInt done;
    char          dir[1024];
} FolderPick;

static void SDLCALL folder_pick_cb(void *userdata,
                                   const char * const *filelist, int filter) {
    (void)filter;
    FolderPick *p = (FolderPick *)userdata;
    if (filelist && filelist[0]) {
        snprintf(p->dir, sizeof(p->dir), "%s", filelist[0]);
    }
    SDL_SetAtomicInt(&p->done, 1);
}

int dodai_folder_dialog(michi_buf *out) {
    FolderPick pick = { { 0 }, { 0 } };
    SDL_ShowOpenFolderDialog(folder_pick_cb, &pick, NULL, NULL, false);
    while (!SDL_GetAtomicInt(&pick.done)) {
        SDL_PumpEvents();
        SDL_Delay(16);
    }
    if (!pick.dir[0]) {
        return 0;
    }
    ito_buf_append_c(&out->b, pick.dir);
    return !out->b.overflow;
}
