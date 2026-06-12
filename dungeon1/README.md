# dungeon1 — game project on the shared hot-reloading engine

Game project consuming the shared engine at `../engine`. Platform exe +
reloadable game dll + frozen ABI, with
[seni](C:/Users/andres/Development/seni) auto-migrating persistent game state
when its struct layout changes. Edit code or `src/game_state.h`, run
`reload.bat`, the running process picks it up — no restart, state survives.

## Prerequisites (Windows)

Vendored in the repo (batteries included): SDL3 prebuilt MinGW binaries
(`../vendor/SDL3-mingw/`, x86_64 only), cgltf, stb, clay, and the seni +
kansi libraries as git submodules (`../vendor/seni`, `../vendor/kansi`) —
clone with `git clone --recurse-submodules` (or run
`git submodule update --init` after a plain clone).

Still needed on the machine:

- MinGW gcc (on PATH) — required, seni's layout embedding uses GNU `.incbin`
- CMake + Ninja
- Vulkan SDK (glslc; path set in `reload.bat` + `kansi.cfg`)

To build against a development checkout of seni/kansi instead of the
submodules, override `-DSENI_DIR=`/`-DKANSI_DIR=` (exe) and the include
paths in `kansi.cfg` (dll).

## Build & run

```bat
build.bat          rem exe (cmake) + first game dll
build\dungeon.exe  rem runs from anywhere (exe anchors cwd to project root)
```

## Iterate (the whole point)

With the exe running: **just save a file.** The exe embeds
[kansi](C:/Users/andres/Development/kansi), which watches `src/` and
`../engine/src/` (config in `kansi.cfg`) and rebuilds the dll automatically —
pre-steps (header snapshot, glslc) then gcc, as non-blocking child processes,
publishing via atomic rename. Build errors go to `build/kansi.log`; the old
dll keeps running. `reload.bat` remains as the manual fallback.

- Changed game/engine dll code → dll hot-swapped, state intact.
- Changed `src/game_state.h` layout → seni reads the old layout embedded in
  the running dll (`seni_layout` symbol), diffs against the header on disk,
  generates + gcc-compiles `build/mig.c`, migrates the memory block, then
  swaps dlls. New fields arrive zeroed; removed fields are dropped; array
  resizes copy `min(old,new)` and zero the tail.
- Any failure (parse error, type change, gcc error) → logged, old dll keeps
  running, state untouched.

## Layout

```
../engine/src/         shared engine (a game never edits its own copy)
  base/                layer zero: types. Compiled into both binaries.
  abi/                 the frozen contract (PlatformMemory, PlatformApi,
                       GameInput, GpuContext). Changing it = restart.
  platform/            exe only. SDL window/device/input, memory blocks,
                       reload loop, seni migration driver. Includes base/ +
                       abi/ ONLY — never engine/ or game code.
  engine/              reloadable engine systems (compiled into the game dll)
    render/            render seam: render.h is backend-agnostic (u32-id
                       handles); render_sdlgpu.c is the SDL_GPU/Vulkan backend.
                       Includes a batched 2D overlay (rnd_ui_*) drawn in a
                       second render pass for editor UI.
    asset/             glTF via cgltf, textures via stb_image.
    ui/                editor UI: clay (layout) + stb_truetype (text) over
                       the rnd_ui_* overlay. clay is private to ui.c (own
                       object, build/ui.o); game code uses the plain widget
                       API in ui.h (ui_panel_begin / ui_button / ui_label)
                       between ui_frame_begin/ui_frame_end. Clay's context
                       lives in cold memory, rebuilt every reload.
    engine_unity.c     engine half of the dll unity build
  linalg.h, camera.h   shared math

src/                   this game
  game_state.h         HOT state: flat struct, scalars + fixed arrays only,
                       no pointers (seni constraint, enforced by its parser).
  game.c               entry point; cold state rebuilt on every reload.
  game_unity.c         includes engine_unity.c, then game.c — one gcc call
                       produces the whole reloadable dll.
  shaders/             GLSL → SPIR-V via glslc (recompiled by reload.bat).
```

Rules that keep hot reload honest:

- Hot memory never holds pointers, dll-static addresses, or string literals.
- All GPU/asset state is cold: rebuilt from scratch on every reload.
- The exe never interprets game memory; the dll never stores platform
  pointers across frames (`PlatformApi` is re-supplied every call).
- platform/ must not include engine/ or game headers — if a diff shows it,
  the architecture has been violated.

## Starting another game (dungeon2, ...)

Copy `cmakelists.txt`, `build.bat`, `reload.bat`, and `src/{game_state.h,
game.c, game_unity.c, shaders/}` into a sibling folder. The engine is
consumed as source via `-I../engine/src`; nothing else to wire up.
