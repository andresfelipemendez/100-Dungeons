# 監視 kansi

source watcher + dll rebuilder for hot-reloading hosts. sibling library to
[seni](../seni): the engine consumes it, it is not the engine.

watches source trees from inside the host process; when something changes
(debounced), runs the build pipeline as non-blocking child processes:

1. `pre` commands (shader compiles, header snapshots, ...)
2. one `gcc -shared` invocation assembled from the config
3. atomic rename of the temp dll onto the output path

the host's own dll watcher / migration logic (seni) takes over from there —
kansi ends where the reload loop begins. `kansi_update()` is called once per
frame and never blocks; compiles run as child processes polled for exit.

a config with no `out`/`pre` puts kansi in **watch-only** mode: `kansi_update`
reports `KANSI_CHANGED` edges and builds nothing. that is how meikyu uses it —
kansi reports the change, [kaji](../kaji) forges. kansi never compiles; kaji
never watches.

## usage

```c
#include "kansi.h"

char err[256];
kansi *k = kansi_start("kansi.cfg", err, sizeof(err)); /* NULL on error */

/* per frame: */
switch (kansi_update(k)) {
case KANSI_BUILT: /* new dll published at cfg `out` */ break;
case KANSI_ERROR: /* see cfg `log` */ break;
default: break;
}
```

all build knowledge lives in the config file next to the project, not in the
host binary — the host stays generic, the config changes with the game.

## config format

line-based, `#` comments, keys repeatable:

```
watch   src                 # dir tree to watch, recursive
watch   ../engine/src
ext     .c .h .vert .frag   # extensions that trigger rebuilds (none = all)
pre     copy /y a b         # raw command, run before compiling
source  src/game_unity.c    # the single (unity) C file to compile
include build               # -I        (first include wins, order kept)
libdir  ../vendor/lib       # -L
lib     SDL3                # -l
flag    -g                  # extra gcc flag
define  FOO=1               # -D
out     build/game_new.dll  # final dll path (renamed onto, atomic)
tmp     build/game_tmp.dll  # gcc writes here first (default: <out>.tmp)
log     build/kansi.log     # step output appended here, truncated per build
debounce_ms 300
```

relative paths resolve against the host process cwd.

## change detection

event-based when the OS provides it: ReadDirectoryChangesW (windows) /
inotify (linux), drained non-blocking each `kansi_update` through the
`dodai_notify_fn` callback in `../dodai/dodai.h`. detection latency is one frame.
an event-buffer overflow is reported as a generic change so nothing is
missed; on linux, subdirectories created after start are not auto-watched.

when `dodai_watch_begin` fails, falls back to polling (50ms):
commutative hash fold of (path, mtime, size) over watched files with
matching extensions. fallback consequence: a rewrite with identical size
inside the filesystem's mtime quantum is invisible; real editor saves
always move the mtime.

`pre_newer` steps skip when output is newer than input (mtime), so shader
compiles / header snapshots cost nothing on unrelated saves. `obj` files
let heavy never-changing code (vendor implementations) be compiled once and
linked per rebuild.

## tests

`meikyu --test kansi` builds + runs the suite through kaji (configured by the
`lib_test kansi` row in `build.manifest`): unit tests (parser, ext filter,
command assembly, stamps) + e2e (watch a real dir, rebuild a real dll via
gcc, assert BUILT/ERROR edges). OS specifics live in `../dodai`
(`dodai_posix.c` / `dodai_windows.c`).
