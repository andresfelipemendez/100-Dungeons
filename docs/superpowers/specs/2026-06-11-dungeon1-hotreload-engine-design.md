# dungeon1 — Minimal Hot-Reloading Engine with seni Migrations

Date: 2026-06-11. Status: approved.

## Goal

Split dungeon1's single-exe SDL3 GPU app into a hot-reloading engine: platform
exe + reloadable game DLL + frozen ABI headers. Persistent game state survives
DLL reloads; when the state struct's layout changes, seni
(`C:\Users\andres\Development\seni`) auto-migrates the memory instead of
forcing a restart. Success criterion: builds on Windows (MinGW gcc), runs,
and a live edit to `game_state.h` + `reload.bat` migrates state without
restarting the process.

## Platform / toolchain

Windows 11, MinGW gcc 15 (scoop), CMake + Ninja. seni's `SENI_EMBED_LAYOUT`
uses GNU `.incbin` asm, so gcc/clang is required throughout — no MSVC.

Dependencies:
- SDL3 + SDL3_image: prebuilt `-devel-*-mingw` packages from GitHub releases,
  unpacked into `vendor/` (current xcframeworks are macOS-only and stay for a
  future mac build).
- SDL_shadercross: **dropped**. No binary releases exist and building its
  vendored DXC under MinGW is impractical. Instead, HLSL is precompiled to
  SPIR-V offline with `dxc` from the installed Vulkan SDK
  (`C:\VulkanSDK\1.4.341.1`), and the SDL GPU device is created with the
  Vulkan driver. Shaders move to `.hlsl` files; `reload.bat` recompiles them,
  so shaders stay hot-reloadable alongside code.
- cgltf: vendored header (unchanged).
- seni: referenced by path; `seni.c` + `arena.c` compile into the exe.

## Architecture — three link units

1. **Platform exe** (`platform/`): owns process, SDL init, window,
   `SDL_GPUDevice`, the persistent memory blocks, DLL file watching, the
   reload loop, and the seni migration driver. Includes only `base/` + `abi/`.
   Never needs recompiling during iteration.
2. **Game DLL** (`game/`): everything reloadable — game logic, render
   backend, model loading. Unity build (`game_unity.c`).
3. **ABI** (`abi/`): the frozen contract both sides include. Tiny, loud,
   changing it means "restart, don't reload".

## Folder layout

```
dungeon1/
  src/
    base/
      base_types.h        u8..u64, f32, b32; included by everything
    abi/
      abi_platform.h      PlatformMemory, PlatformApi, GameInput, entry typedefs
    platform/
      win32_main.c        SDL init, event pump, memory, reload+migrate loop
    game/
      game_state.h        FLAT hot state (scalars + fixed arrays ONLY) — the
                          seni-managed header, embedded via SENI_EMBED_LAYOUT
      game.c              exports game_update_and_render; owns cold EngineState
      render/
        render.h          backend-agnostic render API (no SDL types)
        render_sdlgpu.c   SDL_GPU implementation
      model.c/.h          glTF load (retrofitted; SDL types only via render seam — pragmatically may keep SDL internally, see Render seam)
      shaders/
        model.vert.hlsl   moved out of shaders.c string literals
        model.frag.hlsl
      game_unity.c        unity include manifest for the DLL
    linalg.h, camera.h    shared math (header-only, both sides may include)
  build/                  cmake out: exe, game.dll, .spv files, mig artifacts
  reload.bat              dxc shaders + one gcc call -> build/game_new.dll
  cmakelists.txt          exe + first dll + asset paths (Windows/MinGW)
```

## ABI contract (`abi_platform.h`)

```c
typedef struct PlatformMemory {
    u64   hot_size;        // seni-migrated region; holds one game_state at offset 0
    void *hot;
    u64   transient_size;  // scratch; dll rebuilds freely every reload
    void *transient;
    b32   reloaded;        // true on first call after a dll swap
} PlatformMemory;

typedef struct PlatformApi {
    void *gpu_context;     // opaque; only render_sdlgpu.c casts it
    void (*log)(const char *fmt, ...);
} PlatformApi;

typedef struct GameInput { /* keys, mouse, dt — our own types, no SDL */ } GameInput;

// DLL exports:
//   void game_update_and_render(PlatformMemory*, PlatformApi*, GameInput*);
//   const char *seni_layout;   // via SENI_EMBED_LAYOUT("src/game/game_state.h")
```

No SDL types cross the ABI. No schema version integer: seni's embedded layout
string replaces it (the full schema travels inside the DLL it describes —
cannot desync).

`gpu_context` carries `{SDL_Window*, SDL_GPUDevice*}` packed in a small
platform-side struct; only the render backend knows its shape. SDL handles
stay valid across reloads because SDL3.dll remains loaded in the exe.

## Memory model — two tiers

- **Hot state** (`game_state.h`): one flat C struct, scalars and fixed-size
  arrays only (seni's parser constraint; pointers are rejected by design).
  Initial contents: camera target/radius/angle/pitch, clear color, anything
  gameplay-persistent. Placed at offset 0 of the `hot` block. Survives
  reloads; migrated by seni when its layout changes.
- **Cold state** (DLL-internal `EngineState` in the transient block): GPU
  pipelines, buffers, loaded models, sampler, depth target. Pointers allowed.
  **Rebuilt from scratch whenever `reloaded` is true.** This sidesteps every
  stale-pointer / stale-rodata hazard and keeps `game_state.h` trivially
  parseable.

## Render seam (backend-swappable)

Two seams so SDL can later be swapped for sokol/glfw:

1. **OS seam**: exe owns window/input/device; ABI carries only our structs.
   Swapping windowing = rewrite `platform/` only.
2. **Render seam**: `render.h` is our own thin API — opaque u32-id handles
   (sokol-style index handles, not pointers), mirroring only the subset used:
   init from `gpu_context`, pipeline/buffer/texture create, frame
   begin/draw/end. `render_sdlgpu.c` is today's only backend; a future
   `render_sokol.c` swaps via one include in `game_unity.c`. The API grows on
   demand — it is not a full GPU abstraction.

`model.c` is retrofitted to produce vertex/index/texture data and create
resources through `render.h`. Image decode uses SDL3_image inside the
backend (or stb-style surface handoff) so no SDL type leaks above the seam.

## Reload + migration sequence (exe-driven)

The exe links seni statically and drives everything (matches seni's own e2e
pattern):

1. Each frame, stat `build/game_new.dll`. Timestamp changed → begin reload.
2. Read the `seni_layout` symbol from the **currently loaded** DLL (the old
   layout — the header on disk has already been overwritten).
3. Read `src/game/game_state.h` from disk (the new layout).
4. Byte-identical → skip to step 6.
5. Else: `diff_structs` → `generate_migration` → write `build/mig.c` → spawn
   gcc → `build/mig.dll` → load → `migrate_game_state(hot, scratch, 1)` →
   memcpy scratch→hot → unload mig.dll. Scratch = transient block (safe: dll
   is about to be reloaded and rebuilds transient anyway).
6. Unload old DLL, copy `game_new.dll` → `build/game_loaded.dll` (copy so the
   compiler can overwrite the original), load it, re-resolve entry points,
   set `reloaded = 1` for the next frame.
7. Any error (parse failure, type change int→float, gcc failure, missing
   symbol) → log and keep the old DLL running. Never crash the process.

## Build (hybrid)

- **CMake (MinGW/Ninja)**: builds `dungeon.exe` (win32_main.c + seni.c +
  arena.c + SDL3 link), the initial `game_new.dll`, and copies SDL3/SDL3_image
  runtime dlls next to the exe. Also runs dxc for the initial shader compile.
- **`reload.bat`**: the iteration loop. Runs dxc on changed `.hlsl` →
  `build/*.spv`, then one gcc invocation:
  `gcc -shared src/game/game_unity.c -o build/game_new.dll` (+ includes,
  SDL3/SDL3_image import libs, cgltf). The running exe picks it up.
- DLL loads `.spv` bytecode from `build/` at pipeline creation; SDL GPU device
  created with the Vulkan driver (`SDL_CreateGPUDevice(SPIRV, ..., "vulkan")`).

## Error handling

- Reload pipeline errors never kill the process (see step 7 above).
- seni rejects unsupported constructs (pointers, type changes) with messages;
  these are logged verbatim.
- gcc compile of mig.c captured to a log file; failure → reload aborted.

## Testing / verification

1. `cmake --build` succeeds; exe runs; barrel renders via Vulkan.
2. Edit a value in `game.c` (e.g. clear color), `reload.bat` → change appears
   without restart; camera state persists.
3. Add a field to `game_state.h`, `reload.bat` → migration runs, old fields
   keep values, new field zeroed, process never restarts.
4. Remove/resize a field → same, tail-zeroing per seni semantics.
seni's own suite covers parser/diff/codegen; no duplicate tests here.

## Out of scope (this pass)

- macOS/Linux platform files, sokol/glfw backends (seams exist, no impls).
- Audio, asset cooker, work queue, fixed-base-address allocation.
- Multiple seni-managed structs (one root `game_state` only for now).
