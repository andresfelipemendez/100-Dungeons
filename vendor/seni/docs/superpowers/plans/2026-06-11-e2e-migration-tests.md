# E2E Migration Tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `diff_structs` + `generate_migration` and an end-to-end test harness that reads header fixture files, generates migration C code, compiles it to a DLL with gcc, runs it on a real memory block, and asserts the resulting layout.

**Architecture:** Typedef-based codegen (compiler owns layout). Diff matches structs/fields by name; new fields zero-init, removed fields dropped. E2E tests live in a separate `test_e2e.c` utest executable that shells out to gcc and uses LoadLibrary/GetProcAddress.

**Tech Stack:** C (gcc/MinGW), utest.h, Win32 LoadLibrary.

Spec: `docs/superpowers/specs/2026-06-11-e2e-migration-tests-design.md`

---

### Task 1: diff API + diff_structs

**Files:**
- Modify: `seni.h` (replace empty `diff` struct)
- Modify: `seni.c` (finish `diff_structs`)
- Test: `test.c`

- [ ] **Step 1: Replace the empty `diff` struct in seni.h**

Replace

```c
typedef struct {

} diff;
```

with

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
} diff;
```

- [ ] **Step 2: Write failing unit tests in test.c**

Append:

```c
UTEST(diff, add_field) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float x, y;"
    "} enemy;";
    char* new_header =
    "typedef struct {"
        "float x, y;"
        "int health;"
    "} enemy;";
    diff_result r = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)1, r.value.struct_count);
    struct_diff* sd = &r.value.structs[0];
    ASSERT_STREQ("enemy", sd->name);
    ASSERT_EQ((size_t)2, sd->old_count);
    ASSERT_EQ((size_t)3, sd->new_count);
    ASSERT_EQ((size_t)3, sd->ops_count);
    ASSERT_EQ(field_op_copy, sd->ops[0].kind);
    ASSERT_STREQ("x", sd->ops[0].name);
    ASSERT_EQ(field_op_copy, sd->ops[1].kind);
    ASSERT_STREQ("y", sd->ops[1].name);
    ASSERT_EQ(field_op_zero, sd->ops[2].kind);
    ASSERT_STREQ("health", sd->ops[2].name);
    ASSERT_EQ(ast_int, sd->ops[2].type);
}

UTEST(diff, remove_field) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float x, y;"
        "int health;"
    "} enemy;";
    char* new_header =
    "typedef struct {"
        "float x, y;"
    "} enemy;";
    diff_result r = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(r.err);
    struct_diff* sd = &r.value.structs[0];
    ASSERT_EQ((size_t)2, sd->ops_count);
    ASSERT_EQ(field_op_copy, sd->ops[0].kind);
    ASSERT_EQ(field_op_copy, sd->ops[1].kind);
}

UTEST(diff, new_struct) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = "";
    char* new_header =
    "typedef struct {"
        "int score;"
    "} player;";
    diff_result r = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)1, r.value.struct_count);
    struct_diff* sd = &r.value.structs[0];
    ASSERT_EQ((size_t)0, sd->old_count);
    ASSERT_EQ(field_op_zero, sd->ops[0].kind);
}

UTEST(diff, bad_old_header) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result r = diff_structs(&a, "typedef struct {\nlong x;\n} s;", "typedef struct {int x;} s;");
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("old_header error: unknown type 'long' at line 2", r.err);
}

UTEST(diff, bad_new_header) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result r = diff_structs(&a, "typedef struct {int x;} s;", "typedef struct {\nlong x;\n} s;");
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("new_header error: unknown type 'long' at line 2", r.err);
}
```

- [ ] **Step 3: Run tests, verify diff tests fail**

Run: `cmd /c test.bat`
Expected: diff tests fail (struct_count 0 / ops null) — parse tests still pass.

- [ ] **Step 4: Implement diff_structs in seni.c**

Replace the existing `diff_structs` body:

```c
diff_result diff_structs(arena* a, char* old_header, char* new_header) {
    diff_result res = {0};
    parse_result old_r = parse_header(a, old_header);
    if (old_r.err) {
        char* msg = arena_sprintf(a, "old_header error: %s", old_r.err);
        res.err = msg ? msg : old_r.err;
        return res;
    }
    parse_result new_r = parse_header(a, new_header);
    if (new_r.err) {
        char* msg = arena_sprintf(a, "new_header error: %s", new_r.err);
        res.err = msg ? msg : new_r.err;
        return res;
    }
    ast old_ast = old_r.value;
    ast new_ast = new_r.value;

    res.value.struct_count = new_ast.struct_count;
    if (new_ast.struct_count == 0) return res;
    res.value.structs = allocate(a, sizeof(struct_diff) * new_ast.struct_count);
    if (!res.value.structs) { res.err = "out of memory"; return res; }

    for (size_t i = 0; i < new_ast.struct_count; i++) {
        ast_struct* ns = &new_ast.structs[i];
        struct_diff* sd = &res.value.structs[i];
        sd->name = ns->name;
        sd->new_fields = ns->fields;
        sd->new_count = ns->fields_count;

        ast_struct* os = NULL;
        for (size_t j = 0; j < old_ast.struct_count; j++) {
            if (strcmp(old_ast.structs[j].name, ns->name) == 0) { os = &old_ast.structs[j]; break; }
        }
        sd->old_fields = os ? os->fields : NULL;
        sd->old_count = os ? os->fields_count : 0;

        sd->ops_count = ns->fields_count;
        sd->ops = NULL;
        if (ns->fields_count > 0) {
            sd->ops = allocate(a, sizeof(field_op) * ns->fields_count);
            if (!sd->ops) { res.err = "out of memory"; return res; }
        }
        for (size_t f = 0; f < ns->fields_count; f++) {
            field_op* op = &sd->ops[f];
            op->name = ns->fields[f].name;
            op->type = ns->fields[f].type;
            op->kind = field_op_zero;
            for (size_t g = 0; os && g < os->fields_count; g++) {
                if (strcmp(os->fields[g].name, ns->fields[f].name) == 0) { op->kind = field_op_copy; break; }
            }
        }
    }
    return res;
}
```

- [ ] **Step 5: Run tests, verify all pass**

Run: `cmd /c test.bat`
Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add seni.h seni.c test.c
git commit -m "diff structs by name with per-field copy/zero ops"
```

---

### Task 2: generate_migration

**Files:**
- Modify: `seni.h` (add generate_result + prototype)
- Modify: `seni.c` (string builder + generator)
- Test: `test.c`

- [ ] **Step 1: Add to seni.h after diff_result**

```c
typedef struct {
    char* code;
    char* err;  // NULL = success
} generate_result;
```

and after the `diff_structs` prototype:

```c
generate_result generate_migration(arena* a, diff d);
```

- [ ] **Step 2: Write failing unit tests in test.c**

```c
UTEST(generate, add_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float x, y;"
    "} enemy;";
    char* new_header =
    "typedef struct {"
        "float x, y;"
        "int health;"
    "} enemy;";
    diff_result d = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    ASSERT_TRUE(g.code);
    ASSERT_TRUE(strstr(g.code, "#include <stddef.h>"));
    ASSERT_TRUE(strstr(g.code, "typedef struct { float x; float y; } enemy_old;"));
    ASSERT_TRUE(strstr(g.code, "typedef struct { float x; float y; int health; } enemy_new;"));
    ASSERT_TRUE(strstr(g.code, "__declspec(dllexport) void migrate_enemy(void* old_p, void* new_p, size_t count)"));
    ASSERT_TRUE(strstr(g.code, "n[i].x = o[i].x;"));
    ASSERT_TRUE(strstr(g.code, "n[i].y = o[i].y;"));
    ASSERT_TRUE(strstr(g.code, "n[i].health = 0;"));
}

UTEST(generate, new_struct_no_old_typedef) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a, "", "typedef struct {int score;} player;");
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    ASSERT_FALSE(strstr(g.code, "player_old"));
    ASSERT_TRUE(strstr(g.code, "(void)old_p;"));
    ASSERT_TRUE(strstr(g.code, "n[i].score = 0;"));
}

UTEST(generate, out_of_memory) {
    char small[700];
    arena a;
    create_arena(&a, small, sizeof(small));
    diff_result d = diff_structs(&a, "typedef struct {int x;} s;", "typedef struct {int x;} s;");
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_TRUE(g.err);
    ASSERT_STREQ("out of memory", g.err);
}
```

(Tune the 700 so diff succeeds but generation runs out — adjust after first run if needed.)

- [ ] **Step 3: Run tests, verify generate tests fail to compile/link**

Run: `cmd /c test.bat`
Expected: compile error — `generate_migration` undefined until Step 4.
(Compile error counts as the failing state for TDD here; implement next.)

- [ ] **Step 4: Implement in seni.c**

Add above `diff_structs`:

```c
static const char* type_name(ast_type t) {
    switch (t) {
        case ast_int: return "int";
        case ast_float: return "float";
        case ast_char: return "char";
        case ast_double: return "double";
        default: return "void";
    }
}

// arena-backed string builder: consecutive appends are contiguous because the
// arena hands out sequential memory; each append backs off its own null
// terminator so the next append overwrites it.
typedef struct {
    arena* a;
    char* start;
    char* err;
} strbuf;

static void sb_appendf(strbuf* b, const char* fmt, ...) {
    if (b->err) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char* dst = allocate(b->a, (size_t)n + 1);
    if (!dst) { b->err = "out of memory"; return; }
    if (!b->start) b->start = dst;
    va_start(args, fmt);
    vsnprintf(dst, (size_t)n + 1, fmt, args);
    va_end(args);
    b->a->offset -= 1;
}

generate_result generate_migration(arena* a, diff d) {
    generate_result r = {0};
    strbuf b = { a, NULL, NULL };
    sb_appendf(&b, "#include <stddef.h>\n\n");
    for (size_t i = 0; i < d.struct_count; i++) {
        struct_diff* sd = &d.structs[i];
        if (sd->old_count > 0) {
            sb_appendf(&b, "typedef struct { ");
            for (size_t j = 0; j < sd->old_count; j++)
                sb_appendf(&b, "%s %s; ", type_name(sd->old_fields[j].type), sd->old_fields[j].name);
            sb_appendf(&b, "} %s_old;\n", sd->name);
        }
        sb_appendf(&b, "typedef struct { ");
        for (size_t j = 0; j < sd->new_count; j++)
            sb_appendf(&b, "%s %s; ", type_name(sd->new_fields[j].type), sd->new_fields[j].name);
        sb_appendf(&b, "} %s_new;\n", sd->name);

        sb_appendf(&b, "__declspec(dllexport) void migrate_%s(void* old_p, void* new_p, size_t count) {\n", sd->name);
        if (sd->old_count > 0)
            sb_appendf(&b, "    %s_old* o = (%s_old*)old_p;\n", sd->name, sd->name);
        else
            sb_appendf(&b, "    (void)old_p;\n");
        sb_appendf(&b, "    %s_new* n = (%s_new*)new_p;\n", sd->name, sd->name);
        sb_appendf(&b, "    for (size_t i = 0; i < count; i++) {\n");
        for (size_t j = 0; j < sd->ops_count; j++) {
            field_op* op = &sd->ops[j];
            if (op->kind == field_op_copy)
                sb_appendf(&b, "        n[i].%s = o[i].%s;\n", op->name, op->name);
            else
                sb_appendf(&b, "        n[i].%s = 0;\n", op->name);
        }
        sb_appendf(&b, "    }\n}\n\n");
    }
    if (b.err) { r.err = b.err; return r; }
    allocate(a, 1); // claim the final null terminator so later allocations don't clobber it
    r.code = b.start;
    return r;
}
```

Also add `#include <stdarg.h>` to seni.c includes.

- [ ] **Step 5: Run tests, verify all pass**

Run: `cmd /c test.bat`
Expected: all PASS. If `generate.out_of_memory` doesn't hit OOM, shrink the 700 buffer until it does (diff must still succeed).

- [ ] **Step 6: Commit**

```bash
git add seni.h seni.c test.c
git commit -m "generate typedef-based migration code from diff"
```

---

### Task 3: fixtures + e2e harness + first e2e test

**Files:**
- Create: `fixtures/enemy_v1.h`, `fixtures/enemy_v2.h`
- Create: `test_e2e.c`
- Modify: `test.bat`, `.gitignore`

- [ ] **Step 1: Create fixture files**

`fixtures/enemy_v1.h`:

```c
typedef struct {
    float x, y;
} enemy;
```

`fixtures/enemy_v2.h`:

```c
typedef struct {
    float x, y;
    int health;
} enemy;
```

- [ ] **Step 2: Create test_e2e.c with harness + add_field test**

```c
#include "utest.h"
#include "seni.h"
#include "arena.h"
#include "arena.c"
#include "seni.c"
#include <windows.h>

UTEST_MAIN()

static char* read_file(arena* a, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "fixture not found: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = allocate(a, (size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

// writes code to build/<name>.c, compiles to build/<name>.dll, loads it.
// on gcc failure prints the generated code + gcc stderr and returns NULL.
static HMODULE compile_and_load(const char* code, const char* name) {
    char src_path[256], dll_path[256], err_path[256], cmd[1024];
    CreateDirectoryA("build", NULL);
    snprintf(src_path, sizeof(src_path), "build/%s.c", name);
    snprintf(dll_path, sizeof(dll_path), "build/%s.dll", name);
    snprintf(err_path, sizeof(err_path), "build/%s.err", name);
    FILE* f = fopen(src_path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", src_path); return NULL; }
    fputs(code, f);
    fclose(f);
    snprintf(cmd, sizeof(cmd), "gcc -shared -o %s %s 2> %s", dll_path, src_path, err_path);
    if (system(cmd) != 0) {
        fprintf(stderr, "gcc failed for %s\ngenerated code:\n%s\n", src_path, code);
        FILE* e = fopen(err_path, "rb");
        if (e) {
            char line[512];
            while (fgets(line, sizeof(line), e)) fputs(line, stderr);
            fclose(e);
        }
        return NULL;
    }
    HMODULE m = LoadLibraryA(dll_path);
    if (!m) fprintf(stderr, "LoadLibrary failed for %s (error %lu)\n", dll_path, GetLastError());
    return m;
}

typedef void (*migrate_fn)(void* old_p, void* new_p, size_t count);

// pipeline: read both fixtures, diff, generate, compile, load, return migrate_<struct_name>
static migrate_fn build_migration(arena* a, const char* old_path, const char* new_path,
                                  const char* test_name, const char* struct_name, HMODULE* out_mod) {
    char* old_header = read_file(a, old_path);
    char* new_header = read_file(a, new_path);
    if (!old_header || !new_header) return NULL;
    diff_result d = diff_structs(a, old_header, new_header);
    if (d.err) { fprintf(stderr, "diff error: %s\n", d.err); return NULL; }
    generate_result g = generate_migration(a, d.value);
    if (g.err) { fprintf(stderr, "generate error: %s\n", g.err); return NULL; }
    HMODULE m = compile_and_load(g.code, test_name);
    if (!m) return NULL;
    *out_mod = m;
    char sym[128];
    snprintf(sym, sizeof(sym), "migrate_%s", struct_name);
    migrate_fn fn = (migrate_fn)(void*)GetProcAddress(m, sym);
    if (!fn) fprintf(stderr, "GetProcAddress failed for symbol %s\n", sym);
    return fn;
}

UTEST(e2e, add_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    HMODULE mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v1.h", "fixtures/enemy_v2.h",
                                         "add_field", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));  // poison: catches missed zero-init

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
        ASSERT_EQ(0, new_block[i].health);
    }
    FreeLibrary(mod);
}
```

- [ ] **Step 3: Update test.bat**

```bat
@echo off
gcc test.c -o test.exe || exit /b 1
test.exe || exit /b 1
gcc test_e2e.c -o test_e2e.exe || exit /b 1
test_e2e.exe
```

- [ ] **Step 4: Update .gitignore**

Append:

```
build/
*.dll
test_e2e.exe
```

- [ ] **Step 5: Run, verify e2e passes**

Run: `cmd /c test.bat`
Expected: unit tests pass, then `e2e.add_field` PASS.

- [ ] **Step 6: Commit**

```bash
git add fixtures/ test_e2e.c test.bat .gitignore
git commit -m "e2e harness: fixture headers, gcc-to-dll pipeline, add_field test"
```

---

### Task 4: remaining e2e cases

**Files:**
- Create: `fixtures/enemy_v3.h` (for remove: reuse v2→v1), `fixtures/reorder_v1.h`, `fixtures/reorder_v2.h`, `fixtures/world_v1.h`, `fixtures/world_v2.h`
- Modify: `test_e2e.c`

- [ ] **Step 1: Create fixtures**

`fixtures/reorder_v1.h`:

```c
typedef struct {
    float x;
    int health;
    double speed;
} enemy;
```

`fixtures/reorder_v2.h`:

```c
typedef struct {
    double speed;
    float x;
    int health;
} enemy;
```

`fixtures/world_v1.h`:

```c
typedef struct {
    float x, y;
} enemy;
typedef struct {
    int score;
} player;
```

`fixtures/world_v2.h`:

```c
typedef struct {
    float x, y;
    int health;
} enemy;
typedef struct {
    int score, level;
} player;
```

(remove_field reuses enemy_v2.h → enemy_v1.h; identical reuses enemy_v1.h twice.)

- [ ] **Step 2: Add tests to test_e2e.c**

```c
UTEST(e2e, remove_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    HMODULE mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v2.h", "fixtures/enemy_v1.h",
                                         "remove_field", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; int health; } enemy_v2;
    typedef struct { float x, y; } enemy_v1;

    enemy_v2 old_block[2] = { {1.0f, 2.0f, 99}, {3.0f, 4.0f, 50} };
    enemy_v1 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
    }
    FreeLibrary(mod);
}

UTEST(e2e, reorder_fields) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    HMODULE mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/reorder_v1.h", "fixtures/reorder_v2.h",
                                         "reorder_fields", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x; int health; double speed; } enemy_v1;
    typedef struct { double speed; float x; int health; } enemy_v2;

    enemy_v1 old_block[2] = { {1.5f, 7, 2.25}, {3.5f, 9, 4.75} };
    enemy_v2 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].health, new_block[i].health);
        ASSERT_EQ(old_block[i].speed, new_block[i].speed);
    }
    FreeLibrary(mod);
}

UTEST(e2e, multiple_structs) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = read_file(&a, "fixtures/world_v1.h");
    char* new_header = read_file(&a, "fixtures/world_v2.h");
    ASSERT_TRUE(old_header != NULL);
    ASSERT_TRUE(new_header != NULL);
    diff_result d = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    HMODULE mod = compile_and_load(g.code, "multiple_structs");
    ASSERT_TRUE(mod != NULL);

    migrate_fn migrate_enemy_fn = (migrate_fn)(void*)GetProcAddress(mod, "migrate_enemy");
    migrate_fn migrate_player_fn = (migrate_fn)(void*)GetProcAddress(mod, "migrate_player");
    ASSERT_TRUE(migrate_enemy_fn != NULL);
    ASSERT_TRUE(migrate_player_fn != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;
    typedef struct { int score; } player_v1;
    typedef struct { int score, level; } player_v2;

    enemy_v1 old_enemies[2] = { {1.0f, 2.0f}, {3.0f, 4.0f} };
    enemy_v2 new_enemies[2];
    memset(new_enemies, 0xCD, sizeof(new_enemies));
    migrate_enemy_fn(old_enemies, new_enemies, 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_enemies[i].x, new_enemies[i].x);
        ASSERT_EQ(old_enemies[i].y, new_enemies[i].y);
        ASSERT_EQ(0, new_enemies[i].health);
    }

    player_v1 old_players[1] = { {1234} };
    player_v2 new_players[1];
    memset(new_players, 0xCD, sizeof(new_players));
    migrate_player_fn(old_players, new_players, 1);
    ASSERT_EQ(1234, new_players[0].score);
    ASSERT_EQ(0, new_players[0].level);

    FreeLibrary(mod);
}

UTEST(e2e, identical) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    HMODULE mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/enemy_v1.h", "fixtures/enemy_v1.h",
                                         "identical", "enemy", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v1 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
    }
    FreeLibrary(mod);
}
```

- [ ] **Step 3: Run, verify all pass**

Run: `cmd /c test.bat`
Expected: all unit + 5 e2e tests PASS.

- [ ] **Step 4: Commit**

```bash
git add fixtures/ test_e2e.c
git commit -m "e2e: remove, reorder, multiple structs, identical cases"
```

- [ ] **Step 5: Update README testing note**

Append to README.md:

```markdown

## tests

`test.bat` runs unit tests (test.c) then end-to-end tests (test_e2e.c).
The e2e tests read header pairs from `fixtures/`, generate migration code,
compile it with gcc into a DLL in `build/`, load it, run the migration on a
real memory block and assert the resulting layout.
```

- [ ] **Step 6: Commit**

```bash
git add README.md
git commit -m "document test setup"
```
