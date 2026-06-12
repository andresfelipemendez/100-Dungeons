# 遷移 Seni

inspired by sql migration this is a library to generate memory migration code for hotreloading dlls it will diff previous structs and new ones, from there generate the code to migrate the existing memory to the new layout when hot-reloading

the "old" header comes from the running dll itself: `seni_embed.h` embeds the
header bytes into the dll at build time (assembler `.incbin`, no codegen step)
and exports them as `const char* seni_layout`. at reload time the engine reads
that symbol from the currently-loaded dll, diffs it against the new header on
disk, generates + compiles the migration, runs it, then swaps dlls. this can't
desync — the layout travels inside the binary it describes.

using utest for testing
https://github.com/sheredom/utest.h

## header format

both declaration styles parse, with flexible whitespace (brace on its own
line is fine):

```c
typedef struct { float x, y; } enemy;
struct enemy { float x, y; };            /* tag-style */
typedef struct enemy_t { ... } enemy;    /* typedef name wins over the tag */
```

`/* block */` and `// line` comments are stripped before parsing, anywhere.
struct and field names max 64 chars, array sizes max 65536. anything between
structs (prototypes, includes) is ignored.

## supported fields

scalars (`int`, `float`, `char`, `double`) and fixed-size arrays of them.
array resize copies `min(old, new)` elements and zeroes the tail; scalar <->
array conversions go through element 0. pointers are rejected with a parse
error — a pointer can dangle into the unloaded dll or reference old-layout
memory, use indices/handles in hot-reloaded state instead.

## tests

`test.bat` (windows) or `test.sh` (linux, e.g. `wsl sh test.sh`) runs unit
tests (test.c) then end-to-end tests (test_e2e.c).
The e2e tests read header pairs from `fixtures/`, generate migration code,
compile it with gcc into a shared library in `build/` (dll on windows, so on
linux), load it, run the migration on a real memory block and assert the
resulting layout. OS specifics (mkdir, compile command, dynamic loading) go
through `../dodai` (`dodai_posix.c` / `dodai_windows.c`).