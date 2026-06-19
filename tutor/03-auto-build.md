# 03 — The Auto-Builder

The watcher tells us a `.c`/`.h` changed. Now we have to **recompile `game.dll` from inside the running engine** and report any errors. This is the conceptual heart of the system: a program that rebuilds its own plugin while running.

## The decision: direct compiler call

You chose **invoke the compiler directly** over **`cmake --build --target game`**. Worth knowing the trade you made:

| | Direct `cl`/`cc` | `cmake --build` |
|---|---|---|
| Speed per rebuild | faster (no CMake/Ninja layer) | ~100 ms CMake overhead |
| Flag/path duplication | **you** restate include dirs, libs, CRT | none — reuses the real build |
| Cross-compiler (cl vs gcc) | you branch on syntax | CMake hides it |
| Environment needs | inherits engine's env (see the `cl` catch) | same |

The duplication risk is the cost. We tame it by **not hardcoding anything**: CMake already knows the compiler path, the SDL include dir, and the import-lib path, so we have CMake *inject those as preprocessor defines* into the engine (doc 04). `build.c` only decides the **flag grammar** (MSVC `/I` vs GNU `-I`), never the actual paths.

## The interface (`build.h`)

```c
bool build_game(void);   // true = compiler exited 0. Prints diagnostics to the engine terminal.
```

One function, returns success. Deliberately dumb: it doesn't reload anything. Why? Because your `main.c` already reloads whenever `game.dll`'s mtime changes. A successful build writes a newer `game.dll`; the existing check does the swap. A failed build leaves `game.dll` untouched, so no reload happens and the old code keeps running. The builder and the reloader stay decoupled — each does one thing.

## Building the command (`build.c`)

```c
#if HOTBUILD_MSVC
    const char *argv[] = {
        HOTBUILD_CC, "/nologo", "/LD", "/Z7", HOTBUILD_MSVC_CRT,
        "/I", HOTBUILD_GAME_INC,
        "/I", HOTBUILD_SDL_INC,
        HOTBUILD_SRC,
        "/Fe:" HOTBUILD_OUT,
        "/Fo:" HOTBUILD_OBJ,
        "/link", HOTBUILD_SDL_LINK,
        NULL
    };
#else
    const char *argv[] = {
        HOTBUILD_CC, "-shared", "-fPIC", "-g", "-O0", "-fvisibility=hidden",
        "-I", HOTBUILD_GAME_INC,
        "-I", HOTBUILD_SDL_INC,
        HOTBUILD_SRC,
        "-o", HOTBUILD_OUT,
        HOTBUILD_SDL_LINK,
        NULL
    };
#endif
```

The `HOTBUILD_*` tokens are string-literal defines from CMake. Note `"/Fe:" HOTBUILD_OUT` — adjacent string literals concatenate at compile time into one argv element like `/Fe:C:/.../game.dll`. The whole thing is an **argv array, not a command string**: `SDL_CreateProcess` takes `argv` directly, so paths with spaces (`C:/Program Files/...`) need no quoting and there's no shell to mis-split them.

### The MSVC flags, and the two that matter most

- `/LD` — build a DLL.
- **`/Z7` — debug info goes *into the .obj*, not a separate `.pdb`.** This is the critical one. With `/Zi` (the usual choice) MSVC writes a `game.pdb` and *keeps it locked*; the next rebuild a second later fails because the file is still held. `/Z7` has no separate pdb, so back-to-back rebuilds on every save never collide. Classic hot-reload trick.
- `HOTBUILD_MSVC_CRT` — expands to `/MDd` in Debug, `/MD` in Release (CMake picks per config). **Why bother:** the engine and the game are separate binaries with separate C runtimes. If the engine is `/MDd` (debug CRT) and the game is `/MD` (release CRT), any CRT object that crosses the boundary — a `malloc` here freed there, a `FILE*` — corrupts. Matching the CRT removes that whole class of "works in Release, crashes in Debug" bug. (Your game only passes plain data across the boundary today, but matching costs nothing and prevents a future trap.)
- `/Fo:` to a fixed obj path keeps build droppings out of the source tree.

### The GNU/Clang flags (Linux, macOS, Pi 5)

- `-shared -fPIC` — position-independent shared object (`.so`/`.dylib`).
- `-fvisibility=hidden` — hide all symbols by default; only the two functions marked `GAME_API` (doc 01) are exported. Minimal, intentional export surface.
- We link by passing the full path to `libSDL3.so`/`.dylib` (`HOTBUILD_SDL_LINK`) as an argv element — the linker resolves SDL symbols (`SDL_sinf`, the GPU calls) from it directly, no `-L`/`-l` juggling.

## Running it: `SDL_Process`

This is why we can stay cross-platform without `fork`/`CreateProcess` branches:

```c
SDL_Process *proc = SDL_CreateProcess(argv, true);   // true: merge stderr INTO stdout pipe
size_t len = 0; int code = -1;
char *out = (char *)SDL_ReadProcess(proc, &len, &code);  // blocks till exit, returns all output
```

- `SDL_CreateProcess(argv, pipe_stderr=true)` spawns the compiler with **stderr folded into the stdout pipe.** GCC/Clang write errors to stderr, `cl` writes to stdout — merging means we capture diagnostics no matter which compiler, in one stream.
- `SDL_ReadProcess` waits for the process to exit, hands back a single malloc'd buffer of everything it printed, and fills `code` with the exit status. No manual pipe-draining loop, no risk of the classic "child fills the pipe buffer and deadlocks" bug — SDL handles it.

Then we print and clean up:

```c
if (out && len > 0)
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION,
        code == 0 ? SDL_LOG_PRIORITY_INFO : SDL_LOG_PRIORITY_ERROR,
        "[hotbuild] compiler:\n%.*s", (int)len, out);
if (out) SDL_free(out);
SDL_DestroyProcess(proc);
return code == 0;
```

- `%.*s` with `(int)len` prints exactly the captured bytes — the compiler's own error text, verbatim, **in the engine's terminal**, which is the whole point of the feature: you never alt-tab to a build window.
- We color it by severity: `INFO` on success, `ERROR` on failure.
- `SDL_free` the buffer (SDL allocated it), `SDL_DestroyProcess` releases the handle. Return the exit code as a bool.

## Why this is safe to run mid-frame

We block the render loop for the ~0.3–1 s the compile takes. For a dev hot-reload that pause on save is fine and buys a lot of simplicity: no second build thread, no synchronizing a background compile against the GPU reload. If you later want zero hitch, you'd move `build_game` onto a worker and post the result back — but only after this version is solid. Simple first.

And recall the no-lock guarantee from the README: the engine runs off `game_active.dll` (a copy), so `cl` overwriting `game.dll` never fights the running process. `/Z7` removes the pdb lock. Together they mean a rebuild on every keystroke-save just works.
