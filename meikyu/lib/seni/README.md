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

`meikyu --test seni` builds + runs the whole suite through kaji, configured by
the `lib_test seni` row in `build.manifest`. All 100 tests live in one `test.c`
(unit + e2e + fuzz + stress under one `UTEST_MAIN`): the unit tests cover the
parser/diff/codegen; the e2e tests read header pairs from `fixtures/`, generate
migration code, compile it via kaji into a shared library in `build/` (dll on
windows, so on linux), load it, run the migration on a real memory block and
assert the resulting layout; fuzz throws random/mutated headers at the parser;
stress pushes max struct/name counts and arena exhaustion. The runner executes
each lib test from its own dir, so `fixtures/` and the `build/` scratch dir
resolve. OS specifics (mkdir, compile command, dynamic loading) go through
`../dodai` (`dodai_posix.c` / `dodai_windows.c`).