# 鍛冶 kaji

the builder. a purpose-built, cross-platform build library for
hot-reloading hosts: one build description covers the dev rebuild (the dll
the watcher asks for), shipping executables, shaders, snapshots and cached
objects, on windows and linux -- replacing per-OS build scripts entirely.

sibling to [seni](../seni) (migrations) and [kansi](../kansi) (watching):
kansi reports that sources changed, kaji turns sources into binaries, the
host composes them. kaji never watches; kansi never compiles.

## usage

```c
#include "kaji.h"

kaji *forge = kaji_load("kaji.cfg", err, sizeof(err));

/* frame loop: watcher said sources changed -> forge the dll */
static kaji_run game_run;
kaji_build_async(forge, "game", &game_run, 1 /* force */);
switch (kaji_run_poll(forge, &game_run)) {
case KAJI_DONE:   /* exactly once; dll published atomically */ break;
case KAJI_FAILED: /* exactly once; see game_run.log_path */ break;
default: break;
}

/* CLI / CI: blocking, forwards exit status */
return kaji_build(forge, "ship", 1);
```

multiple `kaji_run` slots may be in flight at once -- a ship build never
blocks the gameplay rebuild loop.

## build description

```
builddir build              # ${B} on windows
builddir_linux build-linux  # ${B} on linux
tool glslc C:\VulkanSDK\...\glslc.exe
tool_linux glslc glslc

target vendor_impl object
  in ../engine/src/engine/vendor_impl.c
  out ${B}/vendor_impl.o
  flag -O1
  include ../vendor/cgltf ../vendor/stb

target game dll
  dep vendor_impl
  in src/game_unity.c
  also src/some_header.h          # staleness-only inputs
  out ${B}/game_new${SO}
  obj ${B}/vendor_impl.o
  lib_win SDL3                    # _win/_linux suffix = one OS only
  lib_linux m
```

kinds: `copy` (snapshot when newer), `shader` (${glslc}), `object`
(cc -c, auto -fPIC on linux), `pch` (cc -x c-header), `dll` (cc -shared
to a tmp, then ATOMIC rename -- a watching host never sees a half-written
dll), `exe` (+ `post copy|copydir` bundle steps).

staleness: out missing, or any in/also/obj newer. deps build first; a
rebuilt dependency forces dependents. `force` rebuilds the named target
regardless -- unity builds keep their staleness in headers no list can
enumerate, so the watcher's change edge is the truth.

## strings

parsing is built on `ito (../ito/ito.h)`: pointer+size string views (`ito`),
line/token iterators, and a bounded builder -- no strtok, no hidden
strlen, C strings only at OS boundaries.

## tests

`meikyu --test kaji` builds + runs the suite through kaji itself (configured by
the `lib_test kaji` row in `build.manifest`): unit tests, parser (vars, per-OS
keys, std/pedantic/coverage opts, line numbers in errors), command assembly,
and e2e graph builds with real gcc (skip-when-fresh, rebuild-on-touch, async
polling, failure logs, exe with post copies). OS specifics live in `../dodai`
(`dodai_posix.c` / `dodai_windows.c`).
