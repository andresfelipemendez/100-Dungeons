# meikyu (迷宮)

A small hot-reloading C game engine. The engine is the stable binary; a game
is a folder of data + C sources the engine **gathers like scripts** and forges
into a reloadable dll. Edit code or the hot-state header, save, and the running
process picks it up — no restart, game state survives (via [seni](lib/seni)
auto-migrating the persistent struct when its layout changes).

Built once per machine; everything after that — game dlls, shaders, ship
bundles, even rebuilding the engine exe — is a generated [kaji](lib/kaji)
target the running engine drives.

## Build & run

```sh
./bootstrap.sh            # macOS/Linux  (Windows: bootstrap.bat)
```

This is the only cmake step: it builds the engine exe into `build-<os>/meikyu`
(first run also fetches + builds SDL3 from source, a few minutes). Then open a
project any of three ways:

```sh
./build-mac/meikyu                      # no project beside the exe -> picker
./build-mac/meikyu --path <project>     # open a specific project
cd <project> && .../build-mac/meikyu    # cwd is a project -> open it
```

Headless builds (run from a project dir or with `--path`):

```sh
meikyu --build game     # the reloadable dll
meikyu --build ship     # standalone bundle (runtime host + engine + game)
meikyu --build host     # rebuild this engine exe, beside the running one
```

Requirements: gcc (MinGW on Windows — seni's layout embedding uses GNU
`.incbin`), CMake + Ninja (bootstrap only), Vulkan SDK (`glslc`; resolved at
generation time, falls back across `$VULKAN_SDK`, `/usr/local/bin`,
`/opt/homebrew/bin`). GPU is Vulkan/SPIR-V — MoltenVK on macOS.

## A project

A project is any folder holding a `project.meikyu` marker:

```
name dungeon1            # window title + ship exe name (default: folder name)
# assets ../assets       # dir bundled into ship builds (default ../assets)
# version 0.1            # warns if it mismatches the engine
```

plus `src/` (every `.c` under it is gathered into the dll — adding a file is
registration), `src/game_state.h` (the hot state; flat struct, scalars + fixed
arrays only — seni's constraint), and `src/shaders/*.{vert,frag}`. No build
scripts, cmake, or configs live in a project — the engine generates them.

On open, the engine writes `<project>/build-<os>/gen/`:
`game_unity.gen.c`, `kaji.gen.cfg`, `kansi.gen.cfg` (plain, inspectable). It
regenerates on every open and on each source-set change, so engine upgrades
propagate. Starting a new game = a sibling folder with a `project.meikyu` and a
`src/`.

## Layout

```
src/
  base/        layer zero: types (compiled into both exe and dll)
  abi/         frozen exe<->dll contract (PlatformMemory/Api, GpuContext).
               changing it means "restart", not "reload". no SDL types here.
  platform/    the exe: window/GPU/input via dodai, the dll reload loop, the
               seni migration driver, and project_gen (marker parse +
               build-input generation). host_main.c is the dev host;
               runtime_main.c is the shipped host. includes base/ + abi/ only.
  engine/      reloadable systems compiled into the dll: render (SDL_GPU/
               Vulkan backend behind a u32-handle seam), asset (cgltf/stb),
               ui (clay + stb_truetype, behind an ito-typed widget API).
  editor/      dev-only tooling, linked into the dll, never shipped.
               src/engine must never include src/editor.
  pch.h        vendor headers precompiled into the dll build
  linalg.h, camera.h   shared math

lib/           first-party libraries (own test.sh suites, usable standalone):
  ito (糸)     string views + bounded builder; the monorepo's string type
  michi (道)   path views + builder, layered on ito; the path type
  dodai (土台) the single OS layer (process/files/dl/watch/lock/time); SDL-free
  kansi (監視) source watcher: reports a debounced change edge
  kaji (鍛冶)  the forge: builds targets from a generated description
  seni         persistent-state layout differ + migration codegen (strict C89)
```

`vendor/` (repo root) is **third-party only**: SDL3, cgltf, stb, clay.

## How hot reload works

With the engine running, **just save a file.** kansi reports the change; kaji
forges the dll (snapshots, shaders, cached objects, atomic publish) as
non-blocking child processes; the host swaps the dll in. On a `game_state.h`
layout change, seni diffs the old layout (embedded in the running dll) against
the header, generates + compiles a migration, and moves the memory before the
swap — new fields zeroed, removed fields dropped, arrays `min(old,new)`. Any
failure is logged and the old dll keeps running; state is untouched.

## Conventions that keep reload honest

- Hot memory holds no pointers, dll-static addresses, or string literals.
- All GPU/asset state is cold: rebuilt from scratch on every reload.
- The exe never interprets game memory; the dll never stores platform
  pointers across frames (`PlatformApi` is re-supplied each call).
- The game dll must never depend on dodai; platform code must never include
  engine/ or game headers.
