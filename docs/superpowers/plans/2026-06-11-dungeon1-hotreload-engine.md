# dungeon1 Hot-Reloading Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split dungeon1 into platform exe + reloadable game DLL with seni-driven state migration, compiling and hot-reloading on Windows/MinGW.

**Architecture:** Three link units per spec (`docs/superpowers/specs/2026-06-11-dungeon1-hotreload-engine-design.md`). Exe owns SDL/window/GPU-device/memory/reload-loop and links seni; DLL is a unity build holding game logic + render backend; flat `game_state.h` is the seni-migrated hot state. Shaders are GLSL compiled to SPIR-V via glslc (Vulkan SDK); SDL GPU device forced to Vulkan. stb_image replaces SDL3_image.

**Tech Stack:** C99, MinGW gcc 15, CMake+Ninja (exe only), SDL3 prebuilt mingw, cgltf, stb_image, seni (path: `C:/Users/andres/Development/seni`), glslc.

**User constraint: NO git commits.** Verification is manual/visual per task.

---

### Task 1: Vendor Windows deps

**Files:** Create `vendor/SDL3-mingw/` (extracted devel pkg), `vendor/stb/stb_image.h`.

- [ ] Download `SDL3-devel-3.4.10-mingw.tar.gz` from libsdl-org/SDL releases, extract; keep `x86_64-w64-mingw32/{include,lib,bin}` under `vendor/SDL3-mingw/`.
- [ ] Download raw `stb_image.h` (nothings/stb master) to `vendor/stb/stb_image.h`.
- [ ] Verify: `ls vendor/SDL3-mingw/x86_64-w64-mingw32/lib/libSDL3.dll.a` exists.

### Task 2: base + abi headers

**Files:** Create `dungeon1/src/base/base_types.h`, `dungeon1/src/abi/abi_platform.h`, `dungeon1/src/abi/abi_gpu.h`.

- [ ] `base_types.h`: stdint-based u8..u64, s32, f32, f64, b32.
- [ ] `abi_platform.h`: `PlatformMemory{hot_size,hot,transient_size,transient,reloaded}`, `PlatformApi{gpu_context, log}`, `GameInput{dt}`, `GAME_UPDATE_AND_RENDER(name)` macro + typedef. No SDL includes.
- [ ] `abi_gpu.h`: `GpuContext{void *window; void *device;}` (opaque contract between exe and render backend).

### Task 3: hot state + GLSL shaders

**Files:** Create `dungeon1/src/game/game_state.h`, `dungeon1/src/game/shaders/model.vert`, `model.frag`. 

- [ ] `game_state.h`: flat struct, seni-parseable (int/float only, fixed arrays allowed):
```c
typedef struct {
    int initialized;
    float cam_target_x, cam_target_y, cam_target_z;
    float cam_radius, cam_angle, cam_pitch;
    float clear_r, clear_g, clear_b;
} game_state;
```
- [ ] Port HLSL from `shaders.c` to GLSL 450: vertex UBO `layout(set=1,binding=0)` with mvp+model (row-major handled by transposing on CPU or column-major mats — linalg.h mats are column-vector style; verify mul order), fragment `layout(set=2,binding=0) uniform sampler2D`.
- [ ] Verify: `glslc src/game/shaders/model.vert -o build/model.vert.spv` exits 0 (both stages).

### Task 4: render seam

**Files:** Create `dungeon1/src/game/render/render.h`, `render_sdlgpu.c` (port of gpu.c + pipeline parts of shaders.c).

- [ ] `render.h` (no SDL types): u32-id handles `RndBuffer/RndTexture/RndPipeline`; fns: `rnd_init(void* gpu_context)`, `rnd_buffer_create_vertex/index(const void*,u64)`, `rnd_texture_create_rgba8(const void*,u32,u32)`, `rnd_pipeline_create(const char* vs_spv_path, const char* fs_spv_path)`, `rnd_frame_begin(f32 r,g,b)`, `rnd_draw_model(RndPipeline,RndBuffer vb,RndBuffer ib,RndTexture,u32 index_count,mat4 mvp,mat4 model)`, `rnd_frame_end()`, `rnd_swapchain_size(u32*,u32*)`.
- [ ] `render_sdlgpu.c`: static backend state (tables of SDL handles, sampler, depth texture w/ lazy resize, window+device from GpuContext). Pipeline: load .spv bytes via SDL_LoadFile, `SDL_CreateGPUShader` (SPIRV), vertex layout = existing interleaved pos/normal/uv. Port `gpu_draw` into begin/draw/end split. Statics OK: rebuilt every reload, never persisted.

### Task 5: model retrofit

**Files:** Modify `dungeon1/src/model.c` → move to `dungeon1/src/game/model.c`, `model.h`; texture via stb_image; GPU upload via render.h only.

- [ ] `Model{RndBuffer vertex_buffer,index_buffer; RndTexture texture; u32 index_count; vec3 bounds_min,max}`.
- [ ] Replace SDL_GPU transfer-buffer upload code with `rnd_buffer_create_*`/`rnd_texture_create_rgba8` (backend owns upload mechanics). Replace IMG_Load with stb_image (force 4 channels).

### Task 6: game dll entry + unity build

**Files:** Create `dungeon1/src/game/game.c`, `dungeon1/src/game/game_unity.c`. Move `camera.h`, `linalg.h` stay in `src/` (shared headers).

- [ ] `game.c`: `SENI_EMBED_LAYOUT("src/game/game_state.h")`; exported `game_update_and_render`: on `memory->reloaded` → `rnd_init`, rebuild pipeline + reload model into cold `EngineState` (cast of transient block); on `!gs->initialized` → seed camera from model bounds, clear color, `initialized=1`; per-frame: advance `cam_angle`, build mvp via camera_view/perspective, `rnd_frame_begin/draw/end`.
- [ ] `game_unity.c`: includes cgltf_impl, stb_image impl define, model.c, render_sdlgpu.c, game.c.

### Task 7: reload.bat (dll build)

**Files:** Create `dungeon1/reload.bat`.

- [ ] glslc both shaders → `build/*.spv`; `gcc -shared src/game/game_unity.c -o build/game_tmp.dll` with `-Isrc -I../vendor/cgltf -I../vendor/stb -Ivendor SDL include -IC:/Users/andres/Development/seni` + `-L...SDL3 lib -lSDL3`; then `move /y build\game_tmp.dll build\game_new.dll` (atomic-ish swap so watcher never loads a half-written dll).
- [ ] Verify: `cmd /c reload.bat` exits 0, `build/game_new.dll` exists, exports `game_update_and_render` + `seni_layout` (`objdump -p | grep`).

### Task 8: platform exe + CMake (no hot reload yet)

**Files:** Create `dungeon1/src/platform/win32_main.c`; rewrite `dungeon1/cmakelists.txt`; create `dungeon1/build.bat`. Delete `src/main.c`, `src/gpu.c/.h`, `src/shaders.c/.h`, old `src/model.c/.h`, `src/cgltf_impl.c`.

- [ ] `win32_main.c`: SDL init, window, `SDL_CreateGPUDevice(SPIRV, debug, "vulkan")`, claim window; alloc hot (1MB) + transient (256MB) zeroed; load `build/game_loaded.dll` (copied from game_new.dll), resolve entry; loop: events→quit/escape, dt, call entry with `reloaded` on first frame.
- [ ] `cmakelists.txt`: exe target only (win32_main.c + seni.c + arena.c from SENI_DIR), SDL3 from `vendor/SDL3-mingw` (CMAKE_PREFIX_PATH), copy SDL3.dll beside exe.
- [ ] `build.bat`: cmake configure+build (Ninja, gcc), then `reload.bat` for first dll.
- [ ] Verify: exe runs, barrel renders via Vulkan, camera orbits.

### Task 9: hot reload loop

**Files:** Modify `win32_main.c`.

- [ ] Per-frame (or 250ms throttle) stat `build/game_new.dll` mtime; on change: free old dll, `CopyFileA` → `build/game_loaded.dll` (retry briefly if locked), load, resolve, set `reloaded=1` for next frame. On any failure log + keep old dll.
- [ ] Verify: run exe; change clear color or orbit speed in `game.c`; `reload.bat`; change appears without restart; camera angle continuous (state survived).

### Task 10: seni migration driver

**Files:** Modify `win32_main.c` (+ link already-present seni).

- [ ] Before unloading old dll on reload: GetProcAddress `seni_layout` (cast `const char**`), read `src/game/game_state.h` from disk; if strings differ: `diff_structs` → `generate_migration` → write `build/mig.c` → `system("gcc -shared build/mig.c -o build/mig.dll")` → load → `migrate_game_state(hot, transient_scratch, 1)` → memcpy back → unload mig. Errors → log, abort reload, keep old dll.
- [ ] Verify end-to-end: run; add `float spin_speed;` to `game_state.h`, use it in game.c; `reload.bat`; process keeps running, camera state persists, new field zeroed then settable. Then remove field → reload again, still alive.

### Task 11: cleanup + docs

- [ ] Delete `dungeon1/build.sh` (macOS path, superseded) or guard with note; ensure `.gitignore` covers `build/`, `vendor/SDL3-mingw` decision (binaries — gitignore them, document download in README).
- [ ] Update spec doc amendments (GLSL/glslc, stb_image) — already reflected here.
- [ ] Write `dungeon1/README.md`: prerequisites (MinGW, Vulkan SDK, CMake/Ninja), build.bat, reload.bat workflow, seni migration explanation.

## Self-review

Spec coverage: all sections mapped (deps→T1, ABI→T2, hot state→T3, render seam→T4/5, dll→T6, build hybrid→T7/8, reload→T9, migration→T10, docs→T11). Placeholders: none blocking; exact code lives in executor context. Naming: `game_update_and_render`, `seni_layout`, `migrate_game_state`, `rnd_*` consistent.
