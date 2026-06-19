# 01 — Explicit DLL Exports (a real bug)

## The symptom

Hot reload works from your CLion build dir but not from the CMake-preset (`out/`) build dir. Same source, different result. We confirmed it with `dumpbin`:

```
cmake-build-debug/Debug/game.dll   →  exports game_init, game_update   ✅
out/build/x64-debug/Debug/game.dll →  no exports at all                ❌
```

The engine reloads by calling:

```c
code.init   = SDL_LoadFunction(code.handle, "game_init");
code.update = SDL_LoadFunction(code.handle, "game_update");
```

If the DLL exports nothing, `SDL_LoadFunction` returns `NULL`, `load_game_code` fails, and the engine logs *"failed to load game code"*. So one build dir silently works and another silently doesn't.

## Why it happens

On Windows, **a symbol is not exported from a DLL unless you say so.** Three ways to say so:

1. `__declspec(dllexport)` on the function,
2. a `.def` file listing exports,
3. CMake's `WINDOWS_EXPORT_ALL_SYMBOLS` target property (which auto-generates a `.def` for you).
 
Your code has **none** of these. So where did the working build's exports come from? Almost certainly the IDE (CLion) injected `WINDOWS_EXPORT_ALL_SYMBOLS` or an equivalent into its generation, while the bare Ninja preset did not. That makes exporting an **accident of which tool configured the build** — exactly the kind of invisible, environment-dependent behavior that wastes hours later.

It gets worse for this project specifically: we're about to invoke `cl` *directly* (not through CMake at all) to rebuild the DLL on save. A direct `cl /LD` with no export directive produces a DLL with **no exports** — so every hot rebuild would silently break reload. We have to make exports explicit in the source, where every build path sees them.

## The fix

In `src/game.h`, after the `GameUpdateFn` typedef:

```c
#ifdef _WIN32
#define GAME_API __declspec(dllexport)
#else
#define GAME_API __attribute__((visibility("default")))
#endif

GAME_API void game_init(GameMemory *mem);
GAME_API void game_update(GameMemory *mem);
```

### Line by line

- **`__declspec(dllexport)`** (Windows/MSVC) — tells the linker to put this symbol in the DLL's export table. Now `SDL_LoadFunction("game_init")` can find it, no matter who configured the build.
- **`__attribute__((visibility("default")))`** (Linux/macOS, GCC/Clang) — on ELF/Mach-O, symbols are exported by *default*, so why mark it? Because in `build.c` we'll compile the game with `-fvisibility=hidden`, which flips the default to "hide everything." This attribute is the opt-back-in for exactly the two functions we want visible — nothing else leaks. Clean, minimal export surface, same as the Windows side.
- **The two prototypes** — putting `GAME_API` on the *declaration* is enough. When `game.c` includes `game.h` and then defines `game_init`, the export attribute carries from declaration to definition. That's why `game.c` itself needs **no change**.

## Why a macro, not just `__declspec` inline

`game.h` is shared by two sides:

- the **engine** (`main.c`), which only ever *calls through function pointers* (`GameInitFn`) — it never imports these symbols, so it doesn't care about the macro;
- the **game** (`game.c`), which *defines and exports* them.

A single macro that expands correctly per-OS keeps one header valid for both, and gives you one obvious place to change export rules later. If you ever split engine and game into separate headers you might switch to an import/export toggle (`dllimport` vs `dllexport`) — but here, since the engine never links the symbols, plain `dllexport` is all you need.

## Verify before moving on

Rebuild and re-check:

```
cmake --build out/build/x64-debug --target game
dumpbin -exports out/build/x64-debug/Debug/game.dll
```

You should now see `game_init` and `game_update` in **every** build dir. Confirm the engine still reloads on a manual rebuild. Only then add the watcher — you don't want to debug exports and threading at the same time.
