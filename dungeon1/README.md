# dungeon1 — game project on the shared hot-reloading engine

Game project consuming the shared engine at `../meikyu`. A project is just
this folder: a `project.meikyu` marker, `src/` with the game's C files, and
shaders. The engine gathers the sources like scripts — no build scripts, no
cmake, no kaji/kansi configs here. Edit code or `src/game_state.h`, save,
and the running process picks it up — no restart,
[seni](../meikyu/lib/seni) auto-migrates persistent game state when its
struct layout changes.

## Prerequisites

The first-party libraries (seni, kansi, kaji, ito, michi, dodai, horu,
tsumami, henshu) live in the engine at `../meikyu/lib/`, not here. Third-party
deps are vendored at the repo root (`../vendor/`): SDL3, cgltf, stb, clay.

Still needed on the machine:

- gcc (MinGW on windows) — required, seni's layout embedding uses GNU
  `.incbin`
- CMake + Ninja (engine bootstrap only)
- Vulkan SDK (glslc)

## Build & run

The ENGINE builds once per machine, in its own folder:

```sh
../meikyu/bootstrap.sh          # windows: ..\meikyu\bootstrap.bat
```

Then open this project any of three ways:

```sh
../meikyu/build-mac/meikyu                     # from inside this folder (cwd)
../meikyu/build-mac/meikyu --path <this dir>   # from anywhere
../meikyu/build-mac/meikyu                     # from anywhere else: picker
```

The engine generates every build input into `build-<os>/gen/`
(`kaji.gen.cfg`, `kansi.gen.cfg`, `game_unity.gen.c`) on each open and
forges its own first game dll on launch.

Headless builds: `meikyu --build game`, `--build ship`, `--build host`
(rebuilds the engine exe beside the running one) — run from the project dir
or with `--path`.

## Iterate (the whole point)

With the exe running: **just save a file.** kansi (監視) watches `src/` and
the engine tree and reports a change edge; kaji (鍛冶) forges the dll from
the generated build description — snapshots, shaders, cached objects,
atomic publish — as non-blocking child processes. Adding or deleting a
`src/**.c` file is registration: the engine regenerates the unity file on
the same edge. Build errors land in `build-<os>/kaji_game.log`; the old dll
keeps running.

- Changed game/engine dll code → dll hot-swapped, state intact.
- Changed `src/game_state.h` layout → seni reads the old layout embedded in
  the running dll (`seni_layout` symbol), diffs against the header on disk,
  generates + gcc-compiles the migration, migrates the memory block, then
  swaps dlls. New fields arrive zeroed; removed fields are dropped; array
  resizes copy `min(old,new)` and zero the tail.
- Any failure (parse error, type change, gcc error) → logged, old dll keeps
  running, state untouched.

## Layout

```
../meikyu/src/         shared engine (a game never edits its own copy)
  base/                layer zero: types. Compiled into both binaries.
  abi/                 the frozen contract (PlatformMemory, PlatformApi,
                       GameInput, GpuContext). Changing it = restart.
  platform/            exe only. dodai window/device/input, memory blocks,
                       reload loop, seni migration driver, project_gen
                       (marker parse + build-input generation). Includes
                       base/ + abi/ ONLY — never engine/ or game code.
  editor/              dev-only tooling, linked into the dll (never ship)
  engine/              reloadable engine systems (compiled into the game dll)
    render/            render seam: render.h is backend-agnostic (u32-id
                       handles); render_sdlgpu.c is the SDL_GPU/Vulkan backend.
    asset/             glTF via cgltf, textures via stb_image.
    ui/                editor UI: clay (layout) + stb_truetype (text).
  pch.h                vendor headers precompiled into the dll build
  linalg.h, camera.h   shared math

project.meikyu         the marker: name (+ optional assets dir, layout_include)
src/                   this game — every .c here is gathered into the dll
  game_state.h         HOT state: scalars, fixed arrays, and nested structs,
                       no pointers (seni constraint). nests the henshu editor's
                       EditorState (declared via the marker's layout_include).
  game.c               entry point: camera, the demo barrel, clear colour, and
                       wiring -- the CSG editor itself lives in lib/henshu.
                       cold (GPU) state rebuilt on every reload.
  shaders/             GLSL → SPIR-V via glslc (generated shader targets).
```

Rules that keep hot reload honest:

- Hot memory never holds pointers, dll-static addresses, or string literals.
- All GPU/asset state is cold: rebuilt from scratch on every reload.
- The exe never interprets game memory; the dll never stores platform
  pointers across frames (`PlatformApi` is re-supplied every call).
- platform/ must not include engine/ or game headers — if a diff shows it,
  the architecture has been violated.

## Starting another game (dungeon2, ...)

Make a sibling folder with a `project.meikyu` (`name dungeon2`) and
`src/{game_state.h, game.c, shaders/}`. That's it — the engine generates
the rest.
