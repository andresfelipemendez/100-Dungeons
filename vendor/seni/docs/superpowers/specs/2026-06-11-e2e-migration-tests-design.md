# E2E Migration Test Harness + Pipeline — Design

Date: 2026-06-11
Status: Approved

## Goal

Seni currently has unit tests for `parse_header` only. The core promise — diff two
headers, generate migration code, run it on real memory — is untested and mostly
unimplemented (`diff` is an empty struct, `diff_structs` is a stub, no codegen).

This work adds an end-to-end test harness that reads two header text files, diffs
them, generates C migration code, compiles it with gcc into a DLL, loads it, runs
the migration on a real memory block, and asserts the resulting layout. It also
implements `diff_structs` and `generate_migration` so the e2e cases pass.

## Migration semantics (v1)

- Structs matched across headers **by name**. Struct only in new header → all
  fields zero-initialized. Struct only in old header → dropped (absent from diff).
- Fields matched **by name** within a struct.
- Field present in both → copied (`field_op_copy`).
- Field only in new → zero-initialized (`field_op_zero`).
- Field only in old → dropped.
- Same name, different type → **out of scope for v1** (future: error or convert).
- Migration writes old block → separate new block (never in-place; new layout may
  be larger).

## 1. Library API additions (seni.h)

```c
typedef enum { field_op_copy, field_op_zero } field_op_kind;

typedef struct {
    field_op_kind kind;
    char* name;        // field name in new struct
    ast_type type;     // type in new struct
} field_op;

typedef struct {
    char* name;                                 // struct name
    ast_field* old_fields; size_t old_count;    // for emitting old typedef
    ast_field* new_fields; size_t new_count;    // for emitting new typedef
    field_op* ops;         size_t ops_count;    // one per new field, in new order
} struct_diff;

typedef struct {
    struct_diff* structs;
    size_t struct_count;
} diff;   // replaces the current empty stub

typedef struct { char* code; char* err; } generate_result;
generate_result generate_migration(arena* a, diff d);
```

`diff_structs(arena*, old_header, new_header)` keeps its signature and fills the
new `diff`. All allocations from the arena; errors via the existing
`{value, err}` result pattern with concrete values in messages (line numbers,
names), matching the existing error-message style.

## 2. Generated code shape (typedef-based)

Per diffed struct the generator emits old/new typedefs and an exported migrate
function. The compiler owns layout — no offset/alignment math in the generator.

```c
#include <stddef.h>

typedef struct { float x; float y; } enemy_old;
typedef struct { float x; float y; int health; } enemy_new;

__declspec(dllexport) void migrate_enemy(void* old_p, void* new_p, size_t count) {
    enemy_old* o = (enemy_old*)old_p;
    enemy_new* n = (enemy_new*)new_p;
    for (size_t i = 0; i < count; i++) {
        n[i].x = o[i].x;
        n[i].y = o[i].y;
        n[i].health = 0;
    }
}
```

One `migrate_<name>` export per struct in the diff. Code is built as a string in
the arena (`generate_result.code`).

For a struct that exists only in the new header (`old_count == 0`), no
`<name>_old` typedef is emitted — an empty struct is invalid C. Its migrate
function ignores `old_p` and zero-fills the new array.

## 3. E2E harness (test_e2e.c)

Separate utest executable; unit tests in test.c stay fast and untouched.

Helpers:

- `read_file(path)` — load a fixture header into a buffer; missing file fails the
  test with the path in the message.
- `compile_and_load(code, name)` — write `build/<name>.c`, run
  `gcc -shared -o build/<name>.dll build/<name>.c`, then `LoadLibrary`. Nonzero
  gcc exit fails the test and prints the generated code plus gcc stderr.

Test flow (first case `e2e.add_field`):

1. Read `fixtures/enemy_v1.h` and `fixtures/enemy_v2.h` (v2 adds `int health`).
2. `diff_structs` → `generate_migration`; assert no err at each step.
3. `compile_and_load`, `GetProcAddress("migrate_enemy")`; null proc fails with
   the symbol name.
4. Test declares the v1/v2 typedefs locally; fills an array of 3 v1 enemies with
   known values.
5. Calls migrate into a fresh v2 array pre-filled with `0xCD` (catches missed
   zero-init).
6. Asserts x/y survive per element and `health == 0` for all elements.

Further cases, same fixture-pair pattern:

- `remove_field` — v2 drops a field; survivors keep values.
- `reorder_fields` — same fields, different order; values follow names.
- `multiple_structs` — two structs in one header pair; one migrate fn each.
- `identical` — no changes; pure copy.

`FreeLibrary` after each case; artifacts stay in `build/` (gitignored) for
postmortem.

## 4. Build

`test.bat` extended: build + run `test.exe` (unit), then build + run
`test_e2e.exe` (same single-translation-unit `#include "seni.c"` style).
`.gitignore` gains `build/` and `*.dll`.

## 5. Out of scope

- Type conversion on changed-type fields (v1 drops support; future error).
- Non-primitive field types (structs, pointers, arrays) — parser doesn't support
  them yet either.
- In-place migration.
- Actual hot-reload integration (DLL of a live game) — the e2e DLL stands in for
  it.
