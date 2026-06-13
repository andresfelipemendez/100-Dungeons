# dodai (土台) — the single OS layer

One platform library for the whole monorepo. Consumers: kansi (監視, the
watcher), kaji (鍛冶, the forge), seni's test harness, and the engine hosts
(`sdl_main.c` dev host, `runtime_main.c` ship host). It replaced four
near-duplicate platform layers; the bodies here are those implementations,
consolidated.

## Shape

```
dodai.h          the whole API (~34 functions): process spawn/poll, files,
                 async copy, change watch, dynamic libraries, shared-lib
                 compile, lock file, time, OS switch
dodai_posix.c    linux + macOS
dodai_windows.c  windows
```

Pick the implementation file per build (cmake/kaji.cfg/test scripts list it
explicitly) — there is no `#ifdef _WIN32` switching inside function bodies.
SDL-free on purpose: the lib test suites link dodai without SDL; the engine
keeps SDL for window/GPU/input only.

## Rules worth knowing

- **Codesign vnode rule (macOS):** the kernel validates Mach-O signatures
  per vnode. Overwriting a previously-mapped dylib in place poisons it —
  the next dlopen is SIGKILLed with "code signature invalid". Therefore
  `dodai_copy_file` removes its destination first; a fresh vnode per
  publish (kaji applies the same rule when it compiles). Harmless on
  linux/windows.
- **Watch on macOS declines:** `dodai_watch_begin` returns 0 (no FSEvents
  backend yet); callers (kansi) fall back to throttled polling rescans.
- **`dodai_mtime_ns` units differ per OS** (POSIX epoch ns vs Windows
  FILETIME ticks). Values are only ever compared against same-OS values
  (staleness checks), never across machines.
- Conventions: 1 = success / 0 = failure unless documented otherwise
  (`dodai_make_dir`, `dodai_absolute_path` keep their inherited
  0-on-success); nothing blocks; callers log and continue.
