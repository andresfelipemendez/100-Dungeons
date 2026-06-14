// platform impl first: on windows it pulls in windows.h, which must come
// before utest.h so utest uses windows.h's LARGE_INTEGER
#include "../dodai/dodai.h"
#ifdef _WIN32
#include "../dodai/dodai_windows.c"
#else
#include "../dodai/dodai_posix.c"
#endif
#include "../kaji/kaji.c"   /* all compilation goes through kaji */
#include "utest.h"
#include "seni.h"
#include "arena.h"
#include "arena.c"
#include "seni.c"
#include "seni_dump.h"
#include "seni_registry.h"
#include "seni_reload.h"
#include <stdlib.h>
#include <time.h>

UTEST_MAIN()

/* compile a seni-generated migration through kaji, pinning C89 so the test
   still validates that seni's codegen is strict-C89-clean. */
static int seni_test_compile(const char *src, const char *lib, const char *err) {
    kaji *k = kaji_new();
    int rc;
    if (!k) return 1;
    rc = kaji_compile_shared(k, src, lib, err,
                             &(kaji_compile_opts){ KAJI_C89, 1, 0 });
    kaji_free(k);
    return rc;
}

typedef void (*migrate_fn)(void* old_p, void* new_p, size_t count);

/* ============================================================ unit tests == */

UTEST(parse, header) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "float x, y;"
    "} enemy;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)1,r.value.struct_count);
    ASSERT_EQ((size_t)2,r.value.structs[0].fields_count);
    ASSERT_STREQ("enemy",r.value.structs[0].name);
    ASSERT_STREQ("x",r.value.structs[0].fields[0].name);
    ASSERT_EQ(ast_float, (int)r.value.structs[0].fields[0].type);
}

UTEST(parse, unknown_type) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "unsigned int x;\n"
    "} enemy;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("unknown type 'unsigned' at line 2", r.err);
}

UTEST(parse, missing_semicolon) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "int x";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("unexpected end of input at line 2", r.err);
}

UTEST(parse, out_of_memory) {
    char buf[32];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "float x, y, z;"
        "int a, b, c;"
        "double d, e, f;"
    "} big;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("out of memory", r.err);
}

UTEST(parse, trailing_spaces_in_names) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "float x , y ;"
    "} enemy;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("y", r.value.structs[0].fields[1].name);
}

UTEST(parse, multiple_structs) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "float x, y;"
        "int health;"
        "double speed;"
    "} enemy;"
    "typedef struct {"
        "char id;"
        "int score, level;"
        "float damage;"
    "} player;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.struct_count);

    // enemy: float x, float y, int health, double speed
    ASSERT_STREQ("enemy", r.value.structs[0].name);
    ASSERT_EQ((size_t)4, r.value.structs[0].fields_count);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
    ASSERT_EQ(ast_float, (int)r.value.structs[0].fields[0].type);
    ASSERT_STREQ("y", r.value.structs[0].fields[1].name);
    ASSERT_EQ(ast_float, (int)r.value.structs[0].fields[1].type);
    ASSERT_STREQ("health", r.value.structs[0].fields[2].name);
    ASSERT_EQ(ast_int, (int)r.value.structs[0].fields[2].type);
    ASSERT_STREQ("speed", r.value.structs[0].fields[3].name);
    ASSERT_EQ(ast_double, (int)r.value.structs[0].fields[3].type);

    // player: char id, int score, int level, float damage
    ASSERT_STREQ("player", r.value.structs[1].name);
    ASSERT_EQ((size_t)4, r.value.structs[1].fields_count);
    ASSERT_STREQ("id", r.value.structs[1].fields[0].name);
    ASSERT_EQ(ast_char, (int)r.value.structs[1].fields[0].type);
    ASSERT_STREQ("score", r.value.structs[1].fields[1].name);
    ASSERT_EQ(ast_int, (int)r.value.structs[1].fields[1].type);
    ASSERT_STREQ("level", r.value.structs[1].fields[2].name);
    ASSERT_EQ(ast_int, (int)r.value.structs[1].fields[2].type);
    ASSERT_STREQ("damage", r.value.structs[1].fields[3].name);
    ASSERT_EQ(ast_float, (int)r.value.structs[1].fields[3].type);
}

UTEST(parse, many_structs_no_cap) {
    /* real engines have hundreds of structs; the only limit is the arena */
    static char buf[131072];
    static char header[32768];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    int offset = 0;
    for (int i = 0; i < 500; i++) {
        offset += sprintf(&header[offset], "typedef struct {int x;} s%d;", i);
    }
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)500, r.value.struct_count);
    ASSERT_STREQ("s0", r.value.structs[0].name);
    ASSERT_STREQ("s499", r.value.structs[499].name);
    ASSERT_EQ((size_t)1, r.value.structs[499].fields_count);
}

UTEST(parse, missing_semicolon_after_name) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "int x;"
    "} enemy";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("enemy", r.value.structs[0].name);
}

UTEST(parse, empty_struct) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
    "} empty;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("empty", r.value.structs[0].name);
    ASSERT_EQ((size_t)0, r.value.structs[0].fields_count);
}

UTEST(parse, missing_struct_name) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "int x;\n"
    "} ;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("struct missing name at line 3", r.err);
}

UTEST(parse, line_count_after_closing_brace) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "int x;\n"
    "}\n"
    ";";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("struct missing name at line 4", r.err);
}

/* --- review findings: red until fixed --- */

/* finding: allocate() does no alignment; a 1-byte string copy then a struct
   allocation hands back a misaligned pointer (UB on strict-alignment targets) */
UTEST(arena, struct_allocations_are_aligned) {
    double storage[64]; /* force 8-aligned base */
    arena a;
    create_arena(&a, storage, sizeof(storage));
    allocate(&a, 1); /* odd offset */
    void* p = allocate(&a, sizeof(double));
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ((size_t)0, ((size_t)(char*)p) % 8);
}

/* finding: offset + s overflows for huge s, bounds check passes, wild pointer out */
UTEST(arena, allocate_overflow_returns_null) {
    char buf[64];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    allocate(&a, 10);
    ASSERT_TRUE(allocate(&a, (size_t)-1) == NULL);
}

/* finding: zero-field struct leaves fields pointer uninitialized arena garbage */
UTEST(parse, zero_field_struct_has_null_fields) {
    char buf[4096];
    arena a;
    memset(buf, 0xAB, sizeof(buf)); /* make stale memory unmistakable */
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
    "} empty;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_TRUE(r.value.structs[0].fields == NULL);
}

/* finding: unbounded names flow into vsprintf(tmp[1024]) during codegen and
   error formatting -> stack smash. parser must cap name length. */
UTEST(parse, field_name_too_long) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char header[4096];
    char longname[201];
    memset(longname, 'a', 200);
    longname[200] = '\0';
    sprintf(header, "typedef struct {int %s;} s;", longname);
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("field name too long (200 chars, max 64) at line 1", r.err);
}

UTEST(parse, struct_name_too_long) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char header[4096];
    char longname[201];
    memset(longname, 'b', 200);
    longname[200] = '\0';
    sprintf(header, "typedef struct {int x;} %s;", longname);
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("struct name too long (200 chars, max 64) at line 1", r.err);
}

/* finding: unknown-type token echoed unbounded into the error message */
UTEST(parse, long_unknown_type_token_truncated) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char header[4096];
    char longtype[301];
    memset(longtype, 't', 300);
    longtype[300] = '\0';
    sprintf(header, "typedef struct {%s x;} s;", longtype);
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_TRUE(strlen(r.err) < 150); /* token must be truncated, not echoed whole */
}

/* finding: 'float x, ;' silently produces a field with an empty name */
UTEST(parse, empty_field_name) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {float x, ;} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("empty field name at line 1", r.err);
}

/* finding: array size digits accumulate unchecked into size_t and wrap */
UTEST(parse, array_size_too_large) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int x[18446744073709551617];} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("invalid array size for field 'x[18446744073709551617]' at line 1", r.err);
}

/* finding: diff matches by name only; int -> float 'migrates' via silent
   implicit conversion. type change must be a hard error, never a convert. */
UTEST(diff, type_change_rejected) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = "typedef struct {int health;} enemy;";
    char* new_header = "typedef struct {float health;} enemy;";
    diff_result r = diff_structs(&a, old_header, new_header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("field 'health' in struct 'enemy' changed type from int to float, cannot migrate", r.err);
}

/* nit: field-name token scan didn't count newlines -> later error lines drifted */
UTEST(parse, line_count_across_field_name) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "int x\n"
        ";\n"
        "float* p;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("pointer type 'float*' at line 4, pointers are not supported, use an index or handle", r.err);
}

/* nit: unknown-type token only stopped at ' ', swallowing newlines and the next line */
UTEST(parse, unknown_type_followed_by_newline) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "long\n"
        "x;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("unknown type 'long' at line 2", r.err);
}

/* 'typedef  struct {' with flexible whitespace must parse, not silently skip
   the struct (silent skip = diff sees every field as removed = data loss) */
UTEST(parse, flexible_struct_start_whitespace) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef  struct  {"
        "int x;"
    "} a_struct;"
    "typedef struct{"
        "int y;"
    "} b_struct;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.struct_count);
    ASSERT_STREQ("a_struct", r.value.structs[0].name);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("b_struct", r.value.structs[1].name);
    ASSERT_STREQ("y", r.value.structs[1].fields[0].name);
}

UTEST(parse, brace_on_next_line) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct\n"
    "{\n"
        "long x;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    /* line 3: newlines inside the relaxed 'typedef struct {' match must count */
    ASSERT_STREQ("unknown type 'long' at line 3", r.err);
}

UTEST(parse, tag_style_struct) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "struct enemy {\n"
        "float x, y;\n"
    "};";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)1, r.value.struct_count);
    ASSERT_STREQ("enemy", r.value.structs[0].name);
    ASSERT_EQ((size_t)2, r.value.structs[0].fields_count);
}

UTEST(parse, typedef_name_wins_over_tag) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct enemy_t {int x;} enemy;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("enemy", r.value.structs[0].name);
}

UTEST(parse, comments_in_struct) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "/* world position */\n"
        "float x, y;\n"
        "int s; /* int fake; */\n"
    "} e;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)3, r.value.structs[0].fields_count);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("s", r.value.structs[0].fields[2].name);
}

UTEST(parse, comment_decoy_outside_struct) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "/* typedef struct { int ghost; } g; */\n"
    "typedef struct {int x;} real;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)1, r.value.struct_count);
    ASSERT_STREQ("real", r.value.structs[0].name);
}

UTEST(parse, line_comment) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "int hp; // health, int fake;\n"
        "float speed;\n"
    "} e;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.structs[0].fields_count);
    ASSERT_STREQ("speed", r.value.structs[0].fields[1].name);
}

UTEST(parse, unterminated_comment) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "/* oops\n"
        "int x;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("unterminated comment at line 2", r.err);
}

UTEST(parse, mid_declaration_comment) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int /* c */ x /* hp */;} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
}

/* fuzz-derived: type keyword only matched with a single trailing space */
UTEST(parse, tab_or_newline_after_type) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int\tx;float\ny;} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.structs[0].fields_count);
    ASSERT_STREQ("x", r.value.structs[0].fields[0].name);
    ASSERT_EQ(ast_int, (int)r.value.structs[0].fields[0].type);
    ASSERT_STREQ("y", r.value.structs[0].fields[1].name);
    ASSERT_EQ(ast_float, (int)r.value.structs[0].fields[1].type);
}

/* fuzz-derived: 'typedef struct {' inside a struct-name region made the
   counting pass and parse pass diverge -> field_counts slots misaligned ->
   arena overflow writing fields. must die at name validation instead. */
UTEST(parse, struct_keyword_inside_name_region) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {int x;} typedef struct {int y;} b; "
    "typedef struct {int p, q, r;} c;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_TRUE(strstr(r.err, "invalid struct name") != NULL);
}

UTEST(parse, invalid_field_name_chars) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int a+b;} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("invalid field name 'a+b' at line 1", r.err);
}

/* fuzz-derived: struct open at EOF was silently half-parsed */
UTEST(parse, unclosed_struct) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int x;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("unexpected end of input at line 1", r.err);
}

UTEST(parse, array_field) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {"
        "float pos[4];"
        "char name[32];"
        "int id;"
    "} thing;";
    parse_result r = parse_header(&a, header);
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)3, r.value.structs[0].fields_count);
    ASSERT_STREQ("pos", r.value.structs[0].fields[0].name);
    ASSERT_EQ((size_t)4, r.value.structs[0].fields[0].array_size);
    ASSERT_EQ(ast_float, (int)r.value.structs[0].fields[0].type);
    ASSERT_STREQ("name", r.value.structs[0].fields[1].name);
    ASSERT_EQ((size_t)32, r.value.structs[0].fields[1].array_size);
    ASSERT_STREQ("id", r.value.structs[0].fields[2].name);
    ASSERT_EQ((size_t)0, r.value.structs[0].fields[2].array_size);
}

UTEST(parse, pointer_field) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "float *p;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("pointer field '*p' at line 2, pointers are not supported, use an index or handle", r.err);
}

UTEST(parse, pointer_type) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header =
    "typedef struct {\n"
        "float* p;\n"
    "} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("pointer type 'float*' at line 2, pointers are not supported, use an index or handle", r.err);
}

UTEST(parse, bad_array_size) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* header = "typedef struct {int x[a];} s;";
    parse_result r = parse_header(&a, header);
    ASSERT_TRUE(r.err);
    ASSERT_STREQ("invalid array size for field 'x[a]' at line 1", r.err);
}

UTEST(diff, array_resize) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float pos[4];"
    "} thing;";
    char* new_header =
    "typedef struct {"
        "float pos[2];"
        "float vel[3];"
    "} thing;";
    diff_result r = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(r.err);
    struct_diff* sd = &r.value.structs[0];
    ASSERT_EQ((size_t)2, sd->ops_count);
    ASSERT_EQ(field_op_copy, (int)sd->ops[0].kind);
    ASSERT_STREQ("pos", sd->ops[0].name);
    ASSERT_EQ((size_t)4, sd->ops[0].old_array_size);
    ASSERT_EQ((size_t)2, sd->ops[0].new_array_size);
    ASSERT_EQ(field_op_zero, (int)sd->ops[1].kind);
    ASSERT_STREQ("vel", sd->ops[1].name);
    ASSERT_EQ((size_t)3, sd->ops[1].new_array_size);
}

UTEST(generate, arrays) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float pos[4];"
    "} thing;";
    char* new_header =
    "typedef struct {"
        "float pos[2];"
        "float vel[3];"
    "} thing;";
    diff_result d = diff_structs(&a, old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    ASSERT_TRUE(strstr(g.code, "typedef struct { float pos[4]; } seni__thing_old;") != NULL);
    ASSERT_TRUE(strstr(g.code, "typedef struct { float pos[2]; float vel[3]; } seni__thing_new;") != NULL);
    ASSERT_TRUE(strstr(g.code, "for (j = 0; j < 2; j++) n[i].pos[j] = o[i].pos[j];") != NULL);
    ASSERT_TRUE(strstr(g.code, "for (j = 0; j < 3; j++) n[i].vel[j] = 0;") != NULL);
}

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
    ASSERT_EQ(field_op_copy, (int)sd->ops[0].kind);
    ASSERT_STREQ("x", sd->ops[0].name);
    ASSERT_EQ(field_op_copy, (int)sd->ops[1].kind);
    ASSERT_STREQ("y", sd->ops[1].name);
    ASSERT_EQ(field_op_zero, (int)sd->ops[2].kind);
    ASSERT_STREQ("health", sd->ops[2].name);
    ASSERT_EQ(ast_int, (int)sd->ops[2].type);
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
    ASSERT_EQ(field_op_copy, (int)sd->ops[0].kind);
    ASSERT_EQ(field_op_copy, (int)sd->ops[1].kind);
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
    ASSERT_EQ(field_op_zero, (int)sd->ops[0].kind);
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
    ASSERT_TRUE(g.code != NULL);
    ASSERT_TRUE(strstr(g.code, "#include <stddef.h>") != NULL);
    ASSERT_TRUE(strstr(g.code, "typedef struct { float x; float y; } seni__enemy_old;") != NULL);
    ASSERT_TRUE(strstr(g.code, "typedef struct { float x; float y; int health; } seni__enemy_new;") != NULL);
    ASSERT_TRUE(strstr(g.code, "#define SENI_EXPORT __declspec(dllexport)") != NULL);
    ASSERT_TRUE(strstr(g.code, "SENI_EXPORT void migrate_enemy(void* old_p, void* new_p, size_t count)") != NULL);
    ASSERT_TRUE(strstr(g.code, "n[i].x = o[i].x;") != NULL);
    ASSERT_TRUE(strstr(g.code, "n[i].y = o[i].y;") != NULL);
    ASSERT_TRUE(strstr(g.code, "n[i].health = 0;") != NULL);
    /* size exports: the reload driver learns both strides from the migration dll */
    ASSERT_TRUE(strstr(g.code, "SENI_EXPORT const size_t migrate_enemy_old_size = sizeof(seni__enemy_old);") != NULL);
    ASSERT_TRUE(strstr(g.code, "SENI_EXPORT const size_t migrate_enemy_new_size = sizeof(seni__enemy_new);") != NULL);
}

UTEST(generate, new_struct_no_old_typedef) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a, "", "typedef struct {int score;} player;");
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    /* no old typedef ("seni__player_old"); the substring "player_old" alone
       would also match the migrate_player_old_size export */
    ASSERT_TRUE(strstr(g.code, "seni__player_old") == NULL);
    ASSERT_TRUE(strstr(g.code, "(void)old_p;") != NULL);
    ASSERT_TRUE(strstr(g.code, "n[i].score = 0;") != NULL);
    /* no old typedef to take sizeof: old size export must be a literal 0 */
    ASSERT_TRUE(strstr(g.code, "SENI_EXPORT const size_t migrate_player_old_size = 0;") != NULL);
    ASSERT_TRUE(strstr(g.code, "SENI_EXPORT const size_t migrate_player_new_size = sizeof(seni__player_new);") != NULL);
}

/* ---- annotations: the header text is the intent channel ---------------- */

UTEST(parse, seni_was) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    parse_result r = parse_header(&a,
        "typedef struct { int light_count SENI_WAS(num_lights); float x; } enemy;");
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.structs[0].fields_count);
    ASSERT_STREQ("light_count", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("num_lights", r.value.structs[0].fields[0].was);
    ASSERT_TRUE(r.value.structs[0].fields[1].was == NULL);
}

UTEST(parse, seni_was_on_array_field) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    parse_result r = parse_header(&a,
        "typedef struct { float pos[4] SENI_WAS( old_pos ); } thing;");
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("pos", r.value.structs[0].fields[0].name);
    ASSERT_EQ((size_t)4, r.value.structs[0].fields[0].array_size);
    ASSERT_STREQ("old_pos", r.value.structs[0].fields[0].was);
}

UTEST(parse, seni_was_malformed) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    parse_result r = parse_header(&a,
        "typedef struct { int a SENI_WAS num_lights; } s;");
    ASSERT_TRUE(r.err != NULL);
    ASSERT_TRUE(strstr(r.err, "malformed SENI_WAS") != NULL);
}

UTEST(parse, seni_dropped) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    parse_result r = parse_header(&a,
        "typedef struct {\n"
        "    float x, y;\n"
        "    SENI_DROPPED(num_lights)\n"
        "    SENI_DROPPED(old_flags)\n"
        "} enemy;");
    ASSERT_FALSE(r.err);
    ASSERT_EQ((size_t)2, r.value.structs[0].fields_count);
    ASSERT_EQ((size_t)2, r.value.structs[0].dropped_count);
    ASSERT_STREQ("num_lights", r.value.structs[0].dropped[0]);
    ASSERT_STREQ("old_flags", r.value.structs[0].dropped[1]);
}

UTEST(parse, seni_dropped_malformed) {
    char buf[4096];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    parse_result r = parse_header(&a,
        "typedef struct { float x; SENI_DROPPED num_lights } enemy;");
    ASSERT_TRUE(r.err != NULL);
    ASSERT_TRUE(strstr(r.err, "malformed SENI_DROPPED") != NULL);
}

UTEST(diff, rename_via_was) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } enemy;",
        "typedef struct { float x; int light_count SENI_WAS(num_lights); } enemy;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    ASSERT_EQ(field_op_copy, (int)d.value.structs[0].ops[1].kind);
    ASSERT_STREQ("light_count", d.value.structs[0].ops[1].name);
    ASSERT_STREQ("num_lights", d.value.structs[0].ops[1].old_name);
}

UTEST(diff, rename_ambiguity_question) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } enemy;",
        "typedef struct { float x; int light_count; } enemy;");
    ASSERT_FALSE(d.err); /* advisory, not an error: ops still built */
    ASSERT_EQ((size_t)1, d.question_count);
    ASSERT_STREQ("enemy", d.questions[0].struct_name);
    ASSERT_STREQ("num_lights", d.questions[0].removed);
    ASSERT_STREQ("light_count", d.questions[0].added);
    ASSERT_TRUE(strstr(d.questions[0].message,
        "struct enemy: 'num_lights' removed, 'light_count' added (both int)") != NULL);
    ASSERT_TRUE(strstr(d.questions[0].message,
        "rename?  annotate: int light_count SENI_WAS(num_lights);") != NULL);
    ASSERT_TRUE(strstr(d.questions[0].message,
        "really removed?  annotate: SENI_DROPPED(num_lights)") != NULL);
    /* meanwhile the conservative op stands: added field zeroes */
    ASSERT_EQ(field_op_zero, (int)d.value.structs[0].ops[1].kind);
}

UTEST(diff, dropped_silences_question) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } enemy;",
        "typedef struct { float x; int light_count; SENI_DROPPED(num_lights) } enemy;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    ASSERT_EQ(field_op_zero, (int)d.value.structs[0].ops[1].kind);
}

UTEST(diff, no_question_when_types_differ) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } enemy;",
        "typedef struct { float x; float brightness; } enemy;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count); /* int -> float can't be a rename */
}

UTEST(diff, was_missing_target_is_error) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; } enemy;",
        "typedef struct { float x; int light_count SENI_WAS(num_lihgts); } enemy;");
    ASSERT_TRUE(d.err != NULL);
    ASSERT_TRUE(strstr(d.err, "SENI_WAS(num_lihgts)") != NULL);
    ASSERT_TRUE(strstr(d.err, "no field 'num_lihgts'") != NULL);
}

UTEST(diff, was_type_mismatch_is_error) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float num_lights; } enemy;",
        "typedef struct { int light_count SENI_WAS(num_lights); } enemy;");
    ASSERT_TRUE(d.err != NULL);
    ASSERT_TRUE(strstr(d.err, "'num_lights' is float") != NULL);
    ASSERT_TRUE(strstr(d.err, "'light_count' is int") != NULL);
}

UTEST(diff, stale_was_is_inert) {
    /* annotation left in the header after the rename migrated: the renamed
       field now matches by name, SENI_WAS must never be consulted */
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int light_count; } enemy;",
        "typedef struct { float x; int light_count SENI_WAS(num_lights); } enemy;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    ASSERT_EQ(field_op_copy, (int)d.value.structs[0].ops[1].kind);
    ASSERT_STREQ("light_count", d.value.structs[0].ops[1].old_name);
}

/* ---- annotation surgery: programmatic answers to diff questions -------- */

UTEST(annotate, rename_round_trip) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = "typedef struct { float x; int num_lights; } enemy;";
    char* new_header =
        "typedef struct {\n"
        "    float x;\n"
        "    int light_count;\n"
        "} enemy;\n";
    annotate_result an = annotate_rename(&a, new_header, "enemy", "num_lights", "light_count");
    ASSERT_FALSE(an.err);
    ASSERT_TRUE(strstr(an.code, "int light_count SENI_WAS(num_lights);") != NULL);
    /* the produced text answers the question the un-annotated diff raises */
    diff_result d = diff_structs(&a, old_header, an.code);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    ASSERT_EQ(field_op_copy, (int)d.value.structs[0].ops[1].kind);
    ASSERT_STREQ("num_lights", d.value.structs[0].ops[1].old_name);
}

UTEST(annotate, rename_array_field) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    annotate_result an = annotate_rename(&a,
        "typedef struct { float pos[4]; } thing;", "thing", "old_pos", "pos");
    ASSERT_FALSE(an.err);
    ASSERT_TRUE(strstr(an.code, "float pos[4] SENI_WAS(old_pos);") != NULL);
}

UTEST(annotate, rename_in_comma_list) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    annotate_result an = annotate_rename(&a,
        "typedef struct { float x, y, z; } v;", "v", "old_y", "y");
    ASSERT_FALSE(an.err);
    ASSERT_TRUE(strstr(an.code, "float x, y SENI_WAS(old_y), z;") != NULL);
}

UTEST(annotate, rename_tag_style_struct) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    annotate_result an = annotate_rename(&a,
        "struct player { int hp; };", "player", "health", "hp");
    ASSERT_FALSE(an.err);
    ASSERT_TRUE(strstr(an.code, "int hp SENI_WAS(health);") != NULL);
}

UTEST(annotate, rename_errors) {
    char buf[8192];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* h = "typedef struct { int hp SENI_WAS(health); } player;";
    annotate_result an = annotate_rename(&a, h, "ghost", "a", "b");
    ASSERT_TRUE(an.err != NULL);
    ASSERT_TRUE(strstr(an.err, "struct 'ghost' not found") != NULL);
    an = annotate_rename(&a, h, "player", "health", "hp"); /* already annotated */
    ASSERT_TRUE(an.err != NULL);
    ASSERT_TRUE(strstr(an.err, "'hp' not found in struct 'player'") != NULL);
}

UTEST(annotate, dropped_round_trip) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    char* old_header = "typedef struct { float x; int num_lights; } enemy;";
    char* new_header =
        "typedef struct {\n"
        "    float x;\n"
        "    int light_count;\n"
        "} enemy;\n";
    annotate_result an = annotate_dropped(&a, new_header, "enemy", "num_lights");
    ASSERT_FALSE(an.err);
    ASSERT_TRUE(strstr(an.code, "    SENI_DROPPED(num_lights)\n} enemy;") != NULL);
    diff_result d = diff_structs(&a, old_header, an.code);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    ASSERT_EQ(field_op_zero, (int)d.value.structs[0].ops[1].kind);
    /* answering twice is refused */
    an = annotate_dropped(&a, an.code, "enemy", "num_lights");
    ASSERT_TRUE(an.err != NULL);
    ASSERT_TRUE(strstr(an.err, "SENI_DROPPED(num_lights) already present") != NULL);
}

UTEST(generate, rename_copies_from_old_name) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; float old_pos[4]; } enemy;",
        "typedef struct { float x; int light_count SENI_WAS(num_lights); float pos[2] SENI_WAS(old_pos); } enemy;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    ASSERT_TRUE(strstr(g.code, "n[i].light_count = o[i].num_lights;") != NULL);
    ASSERT_TRUE(strstr(g.code, "n[i].pos[j] = o[i].old_pos[j];") != NULL);
}

UTEST(strip, removes_all_was_keeps_everything_else) {
    char buf[8192];
    arena a;
    annotate_result r;
    create_arena(&a, buf, sizeof(buf));
    r = strip_was(&a,
        "/* SENI_WAS(in_comment) stays */\n"
        "typedef struct {\n"
        "    int light_count SENI_WAS(num_lights);\n"
        "    float spin SENI_WAS(rate) SENI_DEFAULT(1.8f);\n"
        "    SENI_DROPPED(legacy)\n"
        "} enemy;\n");
    ASSERT_FALSE(r.err);
    ASSERT_TRUE(strstr(r.code, "SENI_WAS(in_comment)") != NULL);
    ASSERT_TRUE(strstr(r.code, "int light_count;") != NULL);
    ASSERT_TRUE(strstr(r.code, "float spin SENI_DEFAULT(1.8f);") != NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_DROPPED(legacy)") != NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_WAS(num_lights)") == NULL);
    ASSERT_TRUE(strstr(r.code, "SENI_WAS(rate)") == NULL);
}

UTEST(strip, untouched_header_returns_same_pointer) {
    char buf[4096];
    arena a;
    char src[] = "typedef struct { float x SENI_DEFAULT(1.0f); } s;\n";
    annotate_result r;
    create_arena(&a, buf, sizeof(buf));
    r = strip_was(&a, src);
    ASSERT_FALSE(r.err);
    ASSERT_TRUE(r.code == src); /* no-op signalled by identity */
}

UTEST(parse, seni_default) {
    char buf[4096];
    arena a;
    parse_result r;
    create_arena(&a, buf, sizeof(buf));
    r = parse_header(&a,
        "typedef struct { float spin SENI_DEFAULT(1.8f); int n; } s;");
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("spin", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("1.8f", r.value.structs[0].fields[0].def);
    ASSERT_TRUE(r.value.structs[0].fields[1].def == NULL);
}

UTEST(parse, seni_default_combined_with_was) {
    char buf[4096];
    arena a;
    parse_result r;
    create_arena(&a, buf, sizeof(buf));
    r = parse_header(&a,
        "typedef struct { int flags SENI_WAS(old_flags) SENI_DEFAULT(-1); } s;");
    ASSERT_FALSE(r.err);
    ASSERT_STREQ("flags", r.value.structs[0].fields[0].name);
    ASSERT_STREQ("old_flags", r.value.structs[0].fields[0].was);
    ASSERT_STREQ("-1", r.value.structs[0].fields[0].def);
}

UTEST(parse, seni_default_empty_is_error) {
    char buf[4096];
    arena a;
    parse_result r;
    create_arena(&a, buf, sizeof(buf));
    r = parse_header(&a, "typedef struct { float x SENI_DEFAULT(); } s;");
    ASSERT_TRUE(r.err != NULL);
    ASSERT_TRUE(strstr(r.err, "SENI_DEFAULT") != NULL);
}

UTEST(generate, default_fills_new_field_and_array_tail) {
    char buf[16384];
    arena a;
    diff_result d;
    generate_result g;
    create_arena(&a, buf, sizeof(buf));
    d = diff_structs(&a,
        "typedef struct { float x; float w[2]; } s;",
        "typedef struct { float x; float w[4]; float spin SENI_DEFAULT(1.8f); int hp[3] SENI_DEFAULT(100); } s;");
    ASSERT_FALSE(d.err);
    g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    /* new scalar gets the literal */
    ASSERT_TRUE(strstr(g.code, "n[i].spin = 1.8f;") != NULL);
    /* new array fills every element with the literal */
    ASSERT_TRUE(strstr(g.code, "n[i].hp[j] = 100;") != NULL);
    /* grown tail of an UNannotated array still zeroes */
    ASSERT_TRUE(strstr(g.code, "n[i].w[j] = 0;") != NULL);
}

UTEST(generate, out_of_memory) {
    /* diff in a roomy arena, generate into a tiny one: OOM is guaranteed
       regardless of how internal struct sizes evolve */
    char big[8192];
    char small[64];
    arena a;
    arena gen;
    create_arena(&a, big, sizeof(big));
    create_arena(&gen, small, sizeof(small));
    diff_result d = diff_structs(&a, "typedef struct {int x;} s;", "typedef struct {int x;} s;");
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&gen, d.value);
    ASSERT_TRUE(g.err);
    ASSERT_STREQ("out of memory", g.err);
}

/* ================================================================= e2e == */

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
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "short read on %s\n", path);
        fclose(f);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

// compiles src_path to build/<name>.<dll|so>, loads it. on gcc failure prints
// the source (code) + gcc stderr and returns NULL.
static void *compile_src_and_load(const char* src_path, const char* code, const char* name) {
    char lib_path[256], err_path[256];
    snprintf(lib_path, sizeof(lib_path), "build/%s.%s", name, dodai_lib_extension());
    snprintf(err_path, sizeof(err_path), "build/%s.err", name);
    if (seni_test_compile(src_path, lib_path, err_path) != 0) {
        fprintf(stderr, "gcc failed for %s\ngenerated code:\n%s\n", src_path, code);
        FILE* e = fopen(err_path, "rb");
        if (e) {
            char line[512];
            while (fgets(line, sizeof(line), e)) fputs(line, stderr);
            fclose(e);
        }
        return NULL;
    }
    return dodai_lib_open(michi_from_cstr(lib_path));
}

// writes code to build/<name>.c, compiles, loads. for hand-written sources
// (game dlls); generated migrations go through compile_migration_and_load.
static void *compile_and_load(const char* code, const char* name) {
    char src_path[256];
    dodai_make_dir(PATH("build"));
    snprintf(src_path, sizeof(src_path), "build/%s.c", name);
    FILE* f = fopen(src_path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", src_path); return NULL; }
    fputs(code, f);
    fclose(f);
    return compile_src_and_load(src_path, code, name);
}

// generated migration TUs compile from the build/seni_out/migration_NNN.c
// audit log, so the file on disk is exactly what gcc consumed.
static void *compile_migration_and_load(const char* code, const char* name) {
    char src_path[256];
    if (seni_dump_migration(code, name, src_path, sizeof(src_path)) != 0) return NULL;
    return compile_src_and_load(src_path, code, name);
}

static int copy_file(const char* src, const char* dst) {
    char buf[4096];
    size_t n;
    FILE* in = fopen(src, "rb");
    FILE* out;
    if (!in) { fprintf(stderr, "cannot read %s\n", src); return 1; }
    out = fopen(dst, "wb");
    if (!out) { fclose(in); fprintf(stderr, "cannot write %s\n", dst); return 1; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 0;
}

// snapshot the layout header to build/<name>_layout.h and resolve to an
// absolute path. within one compile, the preprocessor reads the layout
// header (#include) and the assembler reads it again (.incbin) at different
// moments -- if the file changes in between, the dll's embedded layout
// disagrees with its compiled layout, which is exactly the desync the embed
// exists to prevent. pointing both readers at a snapshot nothing overwrites
// mid-compile closes that window. absolute path because .incbin resolves
// relative to the compiler's cwd, not the source file.
static int snapshot_layout(const char* header_path, const char* name, char* abs_out, size_t cap) {
    char copy_path[256];
    dodai_make_dir(PATH("build"));
    snprintf(copy_path, sizeof(copy_path), "build/%s_layout.h", name);
    if (copy_file(header_path, copy_path) != 0) return 1;
    michi_buf ab;
    michi_buf_reset(&ab);
    int rc = dodai_absolute_path(michi_from_cstr(copy_path), &ab);
    ito_copy(abs_out, cap, michi_view(&ab).s);
    return rc;
}

// pipeline: read both fixtures, diff, generate, compile, load, return migrate_<struct_name>
static migrate_fn build_migration(arena* a, const char* old_path, const char* new_path,
                                  const char* test_name, const char* struct_name, void ** out_mod) {
    char* old_header = read_file(a, old_path);
    char* new_header = read_file(a, new_path);
    if (!old_header || !new_header) return NULL;
    diff_result d = diff_structs(a, old_header, new_header);
    if (d.err) { fprintf(stderr, "diff error: %s\n", d.err); return NULL; }
    generate_result g = generate_migration(a, d.value);
    if (g.err) { fprintf(stderr, "generate error: %s\n", g.err); return NULL; }
    void *m = compile_migration_and_load(g.code, test_name);
    if (!m) return NULL;
    *out_mod = m;
    char sym[128];
    snprintf(sym, sizeof(sym), "migrate_%s", struct_name);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(m, sym);
    if (!fn) fprintf(stderr, "symbol not found: %s\n", sym);
    return fn;
}

UTEST(e2e, add_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
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
    dodai_lib_close(mod);
}

UTEST(e2e, remove_field) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
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
    dodai_lib_close(mod);
}

UTEST(e2e, reorder_fields) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
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
    dodai_lib_close(mod);
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
    void *mod = compile_migration_and_load(g.code, "multiple_structs");
    ASSERT_TRUE(mod != NULL);

    migrate_fn migrate_enemy_fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_enemy");
    migrate_fn migrate_player_fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_player");
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

    dodai_lib_close(mod);
}

// hot-reload scenario: the old header is not on disk anymore (the save
// overwrote it) — it lives embedded inside the currently-loaded game dll.
// build a fake game dll that embeds enemy_v1.h via seni_embed.h, pull the
// layout back out through the seni_layout symbol, diff against the new
// header file, migrate.
UTEST(e2e, layout_embedded_in_dll) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));

    char layout_abs[512];
    char game_src[1024];
    ASSERT_EQ(0, snapshot_layout("fixtures/enemy_v1.h", "game_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src),
        "#include \"../seni_embed.h\"\n"  // generated source lives in build/
        "SENI_EMBED_LAYOUT(\"%s\");\n", layout_abs);
    void *game = compile_and_load(game_src, "game_v1");
    ASSERT_TRUE(game != NULL);

    const char** layout_p = (const char**)dodai_lib_symbol(game, "seni_layout");
    ASSERT_TRUE(layout_p != NULL);
    const char* old_header = *layout_p;

    // embedded bytes must be identical to the file gcc compiled against
    char* file_header = read_file(&a, "fixtures/enemy_v1.h");
    ASSERT_TRUE(file_header != NULL);
    ASSERT_STREQ(file_header, old_header);

    char* new_header = read_file(&a, "fixtures/enemy_v2.h");
    ASSERT_TRUE(new_header != NULL);
    diff_result d = diff_structs(&a, (char*)old_header, new_header);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "layout_embedded_migration");
    ASSERT_TRUE(mod != NULL);
    migrate_fn migrate = (migrate_fn)dodai_lib_symbol(mod, "migrate_enemy");
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float x, y; } enemy_v1;
    typedef struct { float x, y; int health; } enemy_v2;

    enemy_v1 old_block[3] = { {1.0f, 2.0f}, {3.0f, 4.0f}, {5.0f, 6.0f} };
    enemy_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 3);

    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].y, new_block[i].y);
        ASSERT_EQ(0, new_block[i].health);
    }
    dodai_lib_close(mod);
    dodai_lib_close(game);
}

UTEST(e2e, array_resize) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
    migrate_fn migrate = build_migration(&a, "fixtures/thing_v1.h", "fixtures/thing_v2.h",
                                         "array_resize", "thing", &mod);
    ASSERT_TRUE(migrate != NULL);

    typedef struct { float pos[4]; int id; } thing_v1;
    typedef struct { float pos[2]; int id; float vel[3]; } thing_v2;

    thing_v1 old_block[2] = {
        { {1.0f, 2.0f, 3.0f, 4.0f}, 10 },
        { {5.0f, 6.0f, 7.0f, 8.0f}, 20 },
    };
    thing_v2 new_block[2];
    memset(new_block, 0xCD, sizeof(new_block));

    migrate(old_block, new_block, 2);

    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(old_block[i].pos[0], new_block[i].pos[0]);
        ASSERT_EQ(old_block[i].pos[1], new_block[i].pos[1]);
        ASSERT_EQ(old_block[i].id, new_block[i].id);
        for (int j = 0; j < 3; j++) {
            ASSERT_EQ(0.0f, new_block[i].vel[j]);
        }
    }
    dodai_lib_close(mod);
}

static int write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fputs(content, f);
    fclose(f);
    return 0;
}

typedef void (*game_fn)(void* state, size_t count);

/* full engine lifecycle: host owns the state memory, game code lives in a
   dll that gets rebuilt and hot-reloaded after the struct layout changes.

   1. working header build/game_current.h starts as game_v1.h
   2. game dll v1 (embeds layout, has game_init/game_update) loads, ticks twice
   3. the "save": game_current.h is OVERWRITTEN with the v2 layout --
      from here the old layout exists only inside the loaded v1 dll
   4. game dll v2 builds from the new header, with update logic that uses
      the new fields (health, trail[2])
   5. reload: old layout read from v1 dll's seni_layout symbol, new layout
      from v2 dll's, diff -> generate -> compile migration dll -> migrate
      host state into a new block, unload v1
   6. v2's game_update ticks the MIGRATED memory; correct values coming out
      of v2's view of the block is the layout proof */
UTEST(e2e, full_hot_reload) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));

    /* host's view of the two layouts */
    typedef struct { float x, y; float speed; } host_enemy_v1;
    typedef struct { float x, y; float speed; int health; float trail[2]; } host_enemy_v2;

    /* both the #include and the .incbin point at a per-dll snapshot of the
       working header (see snapshot_layout) -- the live game_current.h may be
       overwritten by a "save" while a compile is in flight */
    static const char* game_v1_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_init(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) {\n"
        "        e[i].x = (float)(i * 10);\n"
        "        e[i].y = 0.0f;\n"
        "        e[i].speed = (float)(1 + i);\n"
        "    }\n"
        "}\n"
        "GEXPORT void game_update(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) { e[i].x += e[i].speed; e[i].y += 1.0f; }\n"
        "}\n";

    static const char* game_v2_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_update(void* state, size_t count) {\n"
        "    enemy* e = (enemy*)state;\n"
        "    size_t i;\n"
        "    for (i = 0; i < count; i++) {\n"
        "        e[i].x += e[i].speed;\n"
        "        e[i].y += 1.0f;\n"
        "        e[i].health += 1;\n"
        "        e[i].trail[0] = e[i].x;\n"
        "        e[i].trail[1] = e[i].y;\n"
        "    }\n"
        "}\n";

    char layout_abs[512];
    char game_src[2048];

    /* 1. working header starts at v1 */
    char* v1_header = read_file(&a, "fixtures/game_v1.h");
    ASSERT_TRUE(v1_header != NULL);
    dodai_make_dir(PATH("build"));
    ASSERT_EQ(0, write_file("build/game_current.h", v1_header));

    /* 2. build + load game v1, init, tick twice */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotgame_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), game_v1_fmt, layout_abs, layout_abs);
    void *game_v1 = compile_and_load(game_src, "hotgame_v1");
    ASSERT_TRUE(game_v1 != NULL);
    game_fn init_v1 = (game_fn)dodai_lib_symbol(game_v1, "game_init");
    game_fn update_v1 = (game_fn)dodai_lib_symbol(game_v1, "game_update");
    ASSERT_TRUE(init_v1 != NULL);
    ASSERT_TRUE(update_v1 != NULL);

    host_enemy_v1 state_v1[3];
    init_v1(state_v1, 3);
    update_v1(state_v1, 3);
    update_v1(state_v1, 3);
    /* after 2 ticks: x = 10*i + 2*(1+i), y = 2 */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), state_v1[i].x);
        ASSERT_EQ(2.0f, state_v1[i].y);
    }

    /* 3. the save: overwrite the working header with the v2 layout.
       old layout is now ONLY inside the loaded v1 dll. */
    char* v2_header = read_file(&a, "fixtures/game_v2.h");
    ASSERT_TRUE(v2_header != NULL);
    ASSERT_EQ(0, write_file("build/game_current.h", v2_header));

    /* 4. rebuild: game dll v2 embeds the new layout */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotgame_v2", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), game_v2_fmt, layout_abs, layout_abs);
    void *game_v2 = compile_and_load(game_src, "hotgame_v2");
    ASSERT_TRUE(game_v2 != NULL);
    game_fn update_v2 = (game_fn)dodai_lib_symbol(game_v2, "game_update");
    ASSERT_TRUE(update_v2 != NULL);

    /* 5. reload: diff the layouts the two dlls were actually built with */
    const char** old_layout_p = (const char**)dodai_lib_symbol(game_v1, "seni_layout");
    const char** new_layout_p = (const char**)dodai_lib_symbol(game_v2, "seni_layout");
    ASSERT_TRUE(old_layout_p != NULL);
    ASSERT_TRUE(new_layout_p != NULL);

    diff_result d = diff_structs(&a, (char*)*old_layout_p, (char*)*new_layout_p);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *migration = compile_migration_and_load(g.code, "hotgame_migration");
    ASSERT_TRUE(migration != NULL);
    migrate_fn migrate = (migrate_fn)dodai_lib_symbol(migration, "migrate_enemy");
    ASSERT_TRUE(migrate != NULL);

    host_enemy_v2 state_v2[3];
    memset(state_v2, 0xCD, sizeof(state_v2));
    migrate(state_v1, state_v2, 3);
    dodai_lib_close(game_v1); /* old game code gone, state survived */

    /* migrated: x/y/speed carried over, health zeroed, trail zeroed */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), state_v2[i].x);
        ASSERT_EQ(2.0f, state_v2[i].y);
        ASSERT_EQ((float)(1 + i), state_v2[i].speed);
        ASSERT_EQ(0, state_v2[i].health);
        ASSERT_EQ(0.0f, state_v2[i].trail[0]);
        ASSERT_EQ(0.0f, state_v2[i].trail[1]);
    }

    /* 6. v2 game code ticks the migrated memory */
    update_v2(state_v2, 3);
    for (int i = 0; i < 3; i++) {
        float expect_x = (float)(10 * i + 3 * (1 + i));
        ASSERT_EQ(expect_x, state_v2[i].x);
        ASSERT_EQ(3.0f, state_v2[i].y);
        ASSERT_EQ(1, state_v2[i].health);
        ASSERT_EQ(expect_x, state_v2[i].trail[0]);
        ASSERT_EQ(3.0f, state_v2[i].trail[1]);
    }

    dodai_lib_close(migration);
    dodai_lib_close(game_v2);
}

/* registry-driven reload: who calls migrate, and with which pointers.
   the exe compiles in ZERO struct layout knowledge -- the game dll carves
   its arrays through seni_carve (recording name/offset/count/stride in the
   registry at the block base), and on reload seni_migrate_block walks that
   registry: migrate fn + strides from the migration dll, locations from the
   data itself. host typedefs below exist only to assert values from the
   test; the reload path never touches them. */
UTEST(e2e, registry_driven_reload) {
    char buf[16384];
    arena a;
    char layout_abs[512];
    char game_src[4096];
    size_t cap = 8192;
    void* old_block = malloc(cap);
    void* new_block = malloc(cap);

    /* game code: carves through the registry, finds its array again by name.
       strict c89 (dodai_compile_shared) -- no // comments. */
    static const char* reg_game_fmt =
        "#include <stddef.h>\n"
        "#include \"%s\"\n"
        "#include \"../seni_embed.h\"\n"
        "#include \"../seni_registry.h\"\n"
        "SENI_EMBED_LAYOUT(\"%s\");\n"
        "#if defined(_WIN32)\n"
        "#define GEXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define GEXPORT\n"
        "#endif\n"
        "GEXPORT void game_boot(void* block) {\n"
        "    enemy* e = (enemy*)seni_carve(block, \"enemy\", 5, sizeof(enemy));\n"
        "    size_t i;\n"
        "    if (!e) return;\n"
        "    for (i = 0; i < 5; i++) {\n"
        "        e[i].x = (float)(i * 10);\n"
        "        e[i].y = 0.0f;\n"
        "        e[i].speed = (float)(1 + i);\n"
        "    }\n"
        "}\n"
        "GEXPORT void game_update(void* block) {\n"
        "    seni_array_desc* d = seni_registry_find(block, \"enemy\");\n"
        "    enemy* e;\n"
        "    size_t i;\n"
        "    if (!d) return;\n"
        "    e = (enemy*)((char*)block + d->offset);\n"
        "    for (i = 0; i < d->count; i++) {%s}\n"
        "}\n";
    static const char* v1_tick =
        " e[i].x += e[i].speed; e[i].y += 1.0f; ";
    static const char* v2_tick =
        " e[i].x += e[i].speed; e[i].y += 1.0f;"
        " e[i].health += 1; e[i].trail[0] = e[i].x; e[i].trail[1] = e[i].y; ";

    /* host's view, for assertions only */
    typedef struct { float x, y; float speed; } host_enemy_v1;
    typedef struct { float x, y; float speed; int health; float trail[2]; } host_enemy_v2;
    typedef void (*block_fn)(void* block);

    ASSERT_TRUE(old_block != NULL);
    ASSERT_TRUE(new_block != NULL);
    create_arena(&a, buf, sizeof(buf));

    /* 1. working header at v1; host inits an empty self-describing block */
    char* v1_header = read_file(&a, "fixtures/game_v1.h");
    ASSERT_TRUE(v1_header != NULL);
    dodai_make_dir(PATH("build"));
    ASSERT_EQ(0, write_file("build/game_current.h", v1_header));
    seni_registry_init(old_block, cap);

    /* 2. game v1 carves and runs */
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotreg_v1", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), reg_game_fmt, layout_abs, layout_abs, v1_tick);
    void *game_v1 = compile_and_load(game_src, "hotreg_v1");
    ASSERT_TRUE(game_v1 != NULL);
    block_fn boot_v1 = (block_fn)dodai_lib_symbol(game_v1, "game_boot");
    block_fn update_v1 = (block_fn)dodai_lib_symbol(game_v1, "game_update");
    ASSERT_TRUE(boot_v1 != NULL);
    ASSERT_TRUE(update_v1 != NULL);
    boot_v1(old_block);
    update_v1(old_block);
    update_v1(old_block);

    seni_array_desc* od = seni_registry_find(old_block, "enemy");
    ASSERT_TRUE(od != NULL);
    ASSERT_EQ((size_t)5, od->count);
    ASSERT_EQ(sizeof(host_enemy_v1), od->stride);
    host_enemy_v1* e1 = (host_enemy_v1*)((char*)old_block + od->offset);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), e1[i].x);
        ASSERT_EQ(2.0f, e1[i].y);
    }

    /* 3. the save, then game v2 */
    char* v2_header = read_file(&a, "fixtures/game_v2.h");
    ASSERT_TRUE(v2_header != NULL);
    ASSERT_EQ(0, write_file("build/game_current.h", v2_header));
    ASSERT_EQ(0, snapshot_layout("build/game_current.h", "hotreg_v2", layout_abs, sizeof(layout_abs)));
    snprintf(game_src, sizeof(game_src), reg_game_fmt, layout_abs, layout_abs, v2_tick);
    void *game_v2 = compile_and_load(game_src, "hotreg_v2");
    ASSERT_TRUE(game_v2 != NULL);
    block_fn update_v2 = (block_fn)dodai_lib_symbol(game_v2, "game_update");
    ASSERT_TRUE(update_v2 != NULL);

    /* 4. diff the embedded layouts, build the migration dll */
    const char** old_layout_p = (const char**)dodai_lib_symbol(game_v1, "seni_layout");
    const char** new_layout_p = (const char**)dodai_lib_symbol(game_v2, "seni_layout");
    ASSERT_TRUE(old_layout_p != NULL);
    ASSERT_TRUE(new_layout_p != NULL);
    diff_result d = diff_structs(&a, (char*)*old_layout_p, (char*)*new_layout_p);
    ASSERT_FALSE(d.err);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *migration = compile_migration_and_load(g.code, "hotreg_migration");
    ASSERT_TRUE(migration != NULL);

    /* 5. the actual answer to "who calls migrate": the host, blind,
       driven entirely by the old block's registry + the migration dll */
    memset(new_block, 0xCD, cap);
    ASSERT_EQ(0, seni_migrate_block(old_block, new_block, cap, migration));
    dodai_lib_close(game_v1);

    seni_array_desc* nd = seni_registry_find(new_block, "enemy");
    ASSERT_TRUE(nd != NULL);
    ASSERT_EQ((size_t)5, nd->count);
    ASSERT_EQ(sizeof(host_enemy_v2), nd->stride);
    host_enemy_v2* e2 = (host_enemy_v2*)((char*)new_block + nd->offset);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ((float)(10 * i + 2 * (1 + i)), e2[i].x);
        ASSERT_EQ(2.0f, e2[i].y);
        ASSERT_EQ((float)(1 + i), e2[i].speed);
        ASSERT_EQ(0, e2[i].health);
        ASSERT_EQ(0.0f, e2[i].trail[0]);
        ASSERT_EQ(0.0f, e2[i].trail[1]);
    }

    /* 6. v2 finds the migrated array through the new registry and ticks it */
    update_v2(new_block);
    for (int i = 0; i < 5; i++) {
        float expect_x = (float)(10 * i + 3 * (1 + i));
        ASSERT_EQ(expect_x, e2[i].x);
        ASSERT_EQ(3.0f, e2[i].y);
        ASSERT_EQ(1, e2[i].health);
        ASSERT_EQ(expect_x, e2[i].trail[0]);
        ASSERT_EQ(3.0f, e2[i].trail[1]);
    }

    dodai_lib_close(migration);
    dodai_lib_close(game_v2);
    free(old_block);
    free(new_block);
}

/* rename through the annotation channel: the data actually moves */
UTEST(e2e, rename_via_annotation) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int num_lights; } lamp;",
        "typedef struct { float x; int light_count SENI_WAS(num_lights); } lamp;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "rename_migration");
    ASSERT_TRUE(mod != NULL);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_lamp");
    ASSERT_TRUE(fn != NULL);

    typedef struct { float x; int num_lights; } lamp_v1;
    typedef struct { float x; int light_count; } lamp_v2;
    lamp_v1 old_block[3] = { {1.0f, 7}, {2.0f, 8}, {3.0f, 9} };
    lamp_v2 new_block[3];
    memset(new_block, 0xCD, sizeof(new_block));
    fn(old_block, new_block, 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(old_block[i].x, new_block[i].x);
        ASSERT_EQ(old_block[i].num_lights, new_block[i].light_count);
    }
    dodai_lib_close(mod);
}

UTEST(e2e, ambiguous_raises_question) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    /* same-type field removed + added in one struct -> rename-or-drop? */
    diff_result d = diff_structs(&a,
        "typedef struct { float x; int health; } unit;",
        "typedef struct { float x; int armor; } unit;");
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)1, d.question_count);
    ASSERT_STREQ("unit", d.questions[0].struct_name);
    ASSERT_STREQ("health", d.questions[0].removed);
    ASSERT_STREQ("armor", d.questions[0].added);
}

UTEST(e2e, answer_rename_moves_data) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    /* multi-line struct: the real game_state.h shape (one field per line) */
    const char *old_lit = "typedef struct {\n    float x;\n    int health;\n} unit;\n";
    const char *new_lit = "typedef struct {\n    float x;\n    int armor;\n} unit;\n";
    char *old_h = arena_copy_string(&a, old_lit, strlen(old_lit));
    char *new_h = arena_copy_string(&a, new_lit, strlen(new_lit));
    /* answer "rename": annotate the new header, re-diff -> unambiguous */
    annotate_result an = annotate_rename(&a, new_h, "unit", "health", "armor");
    ASSERT_FALSE(an.err);
    diff_result d = diff_structs(&a, old_h, an.code);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "ans_rename");
    ASSERT_TRUE(mod != NULL);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_unit");
    ASSERT_TRUE(fn != NULL);
    typedef struct { float x; int health; } unit_v1;
    typedef struct { float x; int armor; } unit_v2;
    unit_v1 oldb[2] = { {1.0f, 30}, {2.0f, 40} };
    unit_v2 newb[2];
    memset(newb, 0xCD, sizeof(newb));
    fn(oldb, newb, 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(oldb[i].x, newb[i].x);
        ASSERT_EQ(oldb[i].health, newb[i].armor); /* data MOVED */
    }
    dodai_lib_close(mod);
}

UTEST(e2e, answer_drop_zeroes_data) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    /* multi-line struct: the real game_state.h shape (one field per line) */
    const char *old_lit = "typedef struct {\n    float x;\n    int health;\n} unit;\n";
    const char *new_lit = "typedef struct {\n    float x;\n    int armor;\n} unit;\n";
    char *old_h = arena_copy_string(&a, old_lit, strlen(old_lit));
    char *new_h = arena_copy_string(&a, new_lit, strlen(new_lit));
    /* answer "drop": annotate the removed field as dropped, re-diff clean */
    annotate_result an = annotate_dropped(&a, new_h, "unit", "health");
    ASSERT_FALSE(an.err);
    diff_result d = diff_structs(&a, old_h, an.code);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
    generate_result g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    void *mod = compile_migration_and_load(g.code, "ans_drop");
    ASSERT_TRUE(mod != NULL);
    migrate_fn fn = (migrate_fn)dodai_lib_symbol(mod, "migrate_unit");
    ASSERT_TRUE(fn != NULL);
    typedef struct { float x; int health; } unit_v1;
    typedef struct { float x; int armor; } unit_v2;
    unit_v1 oldb[2] = { {1.0f, 30}, {2.0f, 40} };
    unit_v2 newb[2];
    memset(newb, 0xCD, sizeof(newb));
    fn(oldb, newb, 2);
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(oldb[i].x, newb[i].x);
        ASSERT_EQ(0, newb[i].armor); /* new field zeroed, old data DIED */
    }
    dodai_lib_close(mod);
}

/* the platform's MIG_DROP_ALL escape: answer every ambiguity as a drop on a
   scratch header copy, re-diff -> unambiguous, dropped field marked dead. */
UTEST(e2e, force_drop_clears_questions) {
    static char buf[1u << 20];
    arena a;
    create_arena(&a, buf, sizeof(buf));

    const char *old_lit = "typedef struct {\n    float x;\n    int health;\n} unit;\n";
    const char *new_lit = "typedef struct {\n    float x;\n    int armor;\n} unit;\n";
    char *old_h = arena_copy_string(&a, old_lit, strlen(old_lit));
    char *new_h = arena_copy_string(&a, new_lit, strlen(new_lit));

    diff_result d0 = diff_structs(&a, old_h, new_h);
    ASSERT_TRUE(d0.err == NULL);
    ASSERT_TRUE(d0.question_count >= 1); /* ambiguous: health->armor? */

    char *dropped = new_h;
    size_t i;
    for (i = 0; i < d0.question_count; i++) {
        annotate_result an = annotate_dropped(&a, dropped,
                d0.questions[i].struct_name, d0.questions[i].removed);
        ASSERT_TRUE(an.err == NULL);
        dropped = an.code;
    }

    diff_result d1 = diff_structs(&a, old_h, dropped);
    ASSERT_TRUE(d1.err == NULL);
    ASSERT_EQ((size_t)0, d1.question_count); /* unambiguous after drop */
    ASSERT_TRUE(strstr(dropped, "SENI_DROPPED(health)") != NULL);
}

UTEST(e2e, strip_was_clears_consumed) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    const char *lit =
        "typedef struct { float x; int armor SENI_WAS(health); } unit;";
    char *annotated = arena_copy_string(&a, lit, strlen(lit));
    annotate_result s = strip_was(&a, annotated);
    ASSERT_FALSE(s.err);
    ASSERT_TRUE(strstr(s.code, "SENI_WAS") == NULL); /* annotation gone */
    /* the stripped header diffed against itself stays clean */
    diff_result d = diff_structs(&a, s.code, s.code);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)0, d.question_count);
}

UTEST(e2e, identical) {
    char buf[16384];
    arena a;
    create_arena(&a, buf, sizeof(buf));
    void *mod = NULL;
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
    dodai_lib_close(mod);
}

/* ================================================================ fuzz == */

static unsigned long fz_state;
static unsigned long fz_rand(void) {
    fz_state = fz_state * 1103515245UL + 12345UL;
    return (fz_state >> 16) & 0x7fffUL;
}
static unsigned long fz_range(unsigned long n) { return fz_rand() % n; }

static char fuzz_arena_buf[65536];

/* ---- AST invariants: hold for every non-error parse, whatever the input -- */
static const char* ast_violation(parse_result* r) {
    size_t i, f;
    if (r->err) return NULL; /* error result is always acceptable */
    if (r->value.struct_count > 65536) return "struct_count implausibly large";
    if (r->value.struct_count > 0 && !r->value.structs) return "structs NULL with count > 0";
    for (i = 0; i < r->value.struct_count; i++) {
        ast_struct* s = &r->value.structs[i];
        if (!s->name) return "struct name NULL";
        if (strlen(s->name) == 0 || strlen(s->name) > 64) return "struct name length out of range";
        if (s->fields_count > 0 && !s->fields) return "fields NULL with count > 0";
        for (f = 0; f < s->fields_count; f++) {
            if (!s->fields[f].name) return "field name NULL";
            if (strlen(s->fields[f].name) == 0 || strlen(s->fields[f].name) > 64) return "field name length out of range";
            if (s->fields[f].array_size > 65536) return "array_size > 65536";
            if (s->fields[f].type > ast_unknown) return "type out of range";
        }
    }
    return NULL;
}

UTEST(fuzz, random_garbage) {
    static const char charset[] =
        "abcxyz_0123456789 \t\n{}[]();,*/intfloachdublestrucypedf\\\"'%";
    char input[320];
    int iter;
    fz_state = 0xC0FFEEUL;
    for (iter = 0; iter < 5000; iter++) {
        unsigned long len = fz_range(300);
        unsigned long k;
        arena a;
        parse_result r;
        const char* bad;
        for (k = 0; k < len; k++) {
            if (fz_range(50) == 0) input[k] = (char)(128 + fz_range(120)); /* raw high bytes */
            else input[k] = charset[fz_range(sizeof(charset) - 1)];
        }
        input[len] = '\0';
        create_arena(&a, fuzz_arena_buf, sizeof(fuzz_arena_buf));
        r = parse_header(&a, input);
        bad = ast_violation(&r);
        if (bad) {
            fprintf(stderr, "fuzz.random_garbage iter %d: %s\ninput:\n%s\n", iter, bad, input);
            ASSERT_TRUE(0);
        }
    }
}

UTEST(fuzz, mutated_valid_headers) {
    static const char* base =
        "typedef struct { float x, y; /* pos */ int health; } enemy;\n"
        "struct player { char name[32]; double speed; };\n"
        "typedef struct t_ { int a[4]; // c\n float b; } thing;\n";
    char input[512];
    int iter;
    fz_state = 0xBEEFUL;
    for (iter = 0; iter < 3000; iter++) {
        size_t blen = strlen(base);
        unsigned long nmut = 1 + fz_range(8);
        unsigned long m;
        arena a;
        parse_result r;
        const char* bad;
        memcpy(input, base, blen + 1);
        for (m = 0; m < nmut; m++) {
            size_t len = strlen(input);
            unsigned long pos = len ? fz_range((unsigned long)len) : 0;
            unsigned long op = fz_range(3);
            if (op == 0 && len > 0) {            /* replace */
                input[pos] = (char)(32 + fz_range(95));
            } else if (op == 1 && len > 1) {      /* delete */
                memmove(&input[pos], &input[pos + 1], len - pos);
            } else if (len + 2 < sizeof(input)) { /* insert */
                memmove(&input[pos + 1], &input[pos], len - pos + 1);
                input[pos] = (char)(32 + fz_range(95));
            }
        }
        create_arena(&a, fuzz_arena_buf, sizeof(fuzz_arena_buf));
        r = parse_header(&a, input);
        bad = ast_violation(&r);
        if (bad) {
            fprintf(stderr, "fuzz.mutated iter %d: %s\ninput:\n%s\n", iter, bad, input);
            ASSERT_TRUE(0);
        }
    }
}

/* ---- grammar generator with oracle ---- */

#define FZ_MAX_S 4
#define FZ_MAX_F 6

typedef struct {
    char name[24];
    ast_type type;
    unsigned long arr; /* 0 = scalar */
    int present;
} fz_field;

typedef struct {
    char name[24];
    fz_field fields[FZ_MAX_F * 2];
    unsigned long nfields;
} fz_struct;

typedef struct {
    fz_struct s[FZ_MAX_S];
    unsigned long n;
} fz_spec;

static const char* fz_type_str(ast_type t) {
    switch (t) {
        case ast_int: return "int";
        case ast_float: return "float";
        case ast_char: return "char";
        default: return "double";
    }
}

static void fz_gen_spec(fz_spec* sp) {
    unsigned long i, f;
    sp->n = 1 + fz_range(FZ_MAX_S);
    for (i = 0; i < sp->n; i++) {
        sprintf(sp->s[i].name, "s%lu_%c", i, (char)('a' + fz_range(26)));
        sp->s[i].nfields = 1 + fz_range(FZ_MAX_F);
        for (f = 0; f < sp->s[i].nfields; f++) {
            sprintf(sp->s[i].fields[f].name, "f%lu_%c", f, (char)('a' + fz_range(26)));
            sp->s[i].fields[f].type = (ast_type)fz_range(4);
            sp->s[i].fields[f].arr = fz_range(3) == 0 ? 1 + fz_range(8) : 0;
            sp->s[i].fields[f].present = 1;
        }
    }
}

static void fz_ws(char* buf, int* o) {
    unsigned long n = 1 + fz_range(2);
    unsigned long i;
    for (i = 0; i < n; i++) {
        unsigned long w = fz_range(4);
        buf[(*o)++] = w == 0 ? '\t' : (w == 1 ? '\n' : ' ');
    }
}

static void fz_maybe_comment(char* buf, int* o) {
    unsigned long c = fz_range(5);
    if (c == 0) *o += sprintf(&buf[*o], "/* int fake_%lu; */", fz_range(100));
    else if (c == 1) *o += sprintf(&buf[*o], "// ghost f%lu;\n", fz_range(100));
}

static void fz_emit(fz_spec* sp, char* buf) {
    int o = 0;
    unsigned long i, f;
    for (i = 0; i < sp->n; i++) {
        int tagstyle = (int)fz_range(2);
        if (tagstyle) {
            o += sprintf(&buf[o], "struct");
            fz_ws(buf, &o);
            o += sprintf(&buf[o], "%s", sp->s[i].name);
            fz_ws(buf, &o);
        } else {
            o += sprintf(&buf[o], "typedef");
            fz_ws(buf, &o);
            o += sprintf(&buf[o], "struct");
            fz_ws(buf, &o);
            if (fz_range(2)) { /* decoy tag: typedef name must win */
                o += sprintf(&buf[o], "tag%lu", i);
                fz_ws(buf, &o);
            }
        }
        buf[o++] = '{';
        fz_maybe_comment(buf, &o);
        for (f = 0; f < sp->s[i].nfields; f++) {
            if (!sp->s[i].fields[f].present) continue;
            fz_ws(buf, &o);
            o += sprintf(&buf[o], "%s", fz_type_str(sp->s[i].fields[f].type));
            fz_ws(buf, &o);
            o += sprintf(&buf[o], "%s", sp->s[i].fields[f].name);
            if (sp->s[i].fields[f].arr)
                o += sprintf(&buf[o], "[%lu]", sp->s[i].fields[f].arr);
            if (fz_range(2)) fz_ws(buf, &o);
            buf[o++] = ';';
            fz_maybe_comment(buf, &o);
        }
        fz_ws(buf, &o);
        buf[o++] = '}';
        if (!tagstyle) {
            fz_ws(buf, &o);
            o += sprintf(&buf[o], "%s", sp->s[i].name);
        }
        if (fz_range(2)) fz_ws(buf, &o);
        buf[o++] = ';';
        fz_ws(buf, &o);
    }
    buf[o] = '\0';
}

/* count fields actually present in a spec struct */
static unsigned long fz_present(fz_struct* s) {
    unsigned long f, n = 0;
    for (f = 0; f < s->nfields; f++) n += s->fields[f].present ? 1 : 0;
    return n;
}

UTEST(fuzz, grammar_roundtrip) {
    static char header[8192];
    int iter;
    fz_state = 0x5EED5EEDUL;
    for (iter = 0; iter < 1500; iter++) {
        fz_spec sp;
        arena a;
        parse_result r;
        unsigned long i, f, pi;
        int ok = 1;
        fz_gen_spec(&sp);
        fz_emit(&sp, header);
        create_arena(&a, fuzz_arena_buf, sizeof(fuzz_arena_buf));
        r = parse_header(&a, header);
        if (r.err) {
            fprintf(stderr, "fuzz.grammar iter %d: unexpected error '%s'\nheader:\n%s\n", iter, r.err, header);
            ASSERT_TRUE(0);
        }
        if (r.value.struct_count != sp.n) ok = 0;
        for (i = 0; ok && i < sp.n; i++) {
            ast_struct* ps = &r.value.structs[i];
            if (strcmp(ps->name, sp.s[i].name) != 0) ok = 0;
            if (ps->fields_count != fz_present(&sp.s[i])) ok = 0;
            pi = 0;
            for (f = 0; ok && f < sp.s[i].nfields; f++) {
                if (!sp.s[i].fields[f].present) continue;
                if (strcmp(ps->fields[pi].name, sp.s[i].fields[f].name) != 0) ok = 0;
                else if (ps->fields[pi].type != sp.s[i].fields[f].type) ok = 0;
                else if (ps->fields[pi].array_size != sp.s[i].fields[f].arr) ok = 0;
                pi++;
            }
        }
        if (!ok) {
            fprintf(stderr, "fuzz.grammar iter %d: AST does not match spec\nheader:\n%s\n", iter, header);
            ASSERT_TRUE(0);
        }
    }
}

/* derive a v2 spec: drop/add/resize fields; optionally inject a type change
   and remember it, because diff must then error */
static int fz_mutate_spec(fz_spec* sp) {
    int type_changed = 0;
    unsigned long i, f;
    for (i = 0; i < sp->n; i++) {
        for (f = 0; f < sp->s[i].nfields; f++) {
            unsigned long roll = fz_range(20);
            if (roll < 3) sp->s[i].fields[f].present = 0;                      /* drop */
            else if (roll < 6 && sp->s[i].fields[f].arr)
                sp->s[i].fields[f].arr = 1 + fz_range(8);                      /* resize */
            else if (roll == 6) {                                              /* type change */
                ast_type nt = (ast_type)fz_range(4);
                if (nt != sp->s[i].fields[f].type) {
                    sp->s[i].fields[f].type = nt;
                    type_changed = 1;
                }
            }
        }
        if (fz_range(3) == 0 && sp->s[i].nfields < FZ_MAX_F * 2) {             /* add */
            fz_field* nf = &sp->s[i].fields[sp->s[i].nfields];
            sprintf(nf->name, "fn%lu_%c", sp->s[i].nfields, (char)('a' + fz_range(26)));
            nf->type = (ast_type)fz_range(4);
            nf->arr = fz_range(3) == 0 ? 1 + fz_range(8) : 0;
            nf->present = 1;
            sp->s[i].nfields++;
        }
    }
    return type_changed;
}

UTEST(fuzz, diff_generate_pipeline) {
    static char h1[8192];
    static char h2[8192];
    int iter;
    fz_state = 0xD1FFUL;
    for (iter = 0; iter < 400; iter++) {
        fz_spec sp;
        fz_spec sp2;
        int type_changed;
        arena a;
        diff_result d;
        unsigned long i;
        fz_gen_spec(&sp);
        fz_emit(&sp, h1);
        sp2 = sp;
        type_changed = fz_mutate_spec(&sp2);
        fz_emit(&sp2, h2);
        create_arena(&a, fuzz_arena_buf, sizeof(fuzz_arena_buf));
        d = diff_structs(&a, h1, h2);
        if (type_changed) {
            if (!d.err || !strstr(d.err, "changed type")) {
                fprintf(stderr, "fuzz.pipeline iter %d: expected type-change error, got '%s'\nold:\n%s\nnew:\n%s\n",
                        iter, d.err ? d.err : "(none)", h1, h2);
                ASSERT_TRUE(0);
            }
        } else {
            generate_result g;
            if (d.err) {
                fprintf(stderr, "fuzz.pipeline iter %d: diff error '%s'\nold:\n%s\nnew:\n%s\n", iter, d.err, h1, h2);
                ASSERT_TRUE(0);
            }
            g = generate_migration(&a, d.value);
            if (g.err) {
                fprintf(stderr, "fuzz.pipeline iter %d: generate error '%s'\n", iter, g.err);
                ASSERT_TRUE(0);
            }
            for (i = 0; i < sp2.n; i++) {
                char sym[64];
                sprintf(sym, "migrate_%s", sp2.s[i].name);
                if (!strstr(g.code, sym)) {
                    fprintf(stderr, "fuzz.pipeline iter %d: generated code missing %s\n%s\n", iter, sym, g.code);
                    ASSERT_TRUE(0);
                }
            }
        }
    }
}

/* migration TUs compile from the build/seni_out/migration_NNN.c audit log */
static void *compile_and_load_fz(const char* code, const char* name) {
    char src_path[256];
    char lib_path[256];
    char err_path[256];
    if (seni_dump_migration(code, name, src_path, sizeof(src_path)) != 0) return NULL;
    sprintf(lib_path, "build/%s.%s", name, dodai_lib_extension());
    sprintf(err_path, "build/%s.err", name);
    if (seni_test_compile(src_path, lib_path, err_path) != 0) {
        fprintf(stderr, "gcc failed for %s, generated code:\n%s\n", src_path, code);
        return NULL;
    }
    return dodai_lib_open(michi_from_cstr(lib_path));
}

/* a handful of random survivors all the way through gcc + dlopen + run */
UTEST(fuzz, compile_and_run) {
    static char h1[8192];
    static char h2[8192];
    static char oldblk[16384];
    static char newblk[16384];
    int iter;
    fz_state = 0xFAB1EUL;
    for (iter = 0; iter < 6; iter++) {
        fz_spec sp;
        fz_spec sp2;
        arena a;
        diff_result d;
        generate_result g;
        void *mod;
        char name[32];
        unsigned long i;
        fz_gen_spec(&sp);
        fz_emit(&sp, h1);
        do {
            sp2 = sp;
        } while (fz_mutate_spec(&sp2)); /* reroll until no type change */
        fz_emit(&sp2, h2);
        create_arena(&a, fuzz_arena_buf, sizeof(fuzz_arena_buf));
        d = diff_structs(&a, h1, h2);
        ASSERT_TRUE(d.err == NULL);
        g = generate_migration(&a, d.value);
        ASSERT_TRUE(g.err == NULL);
        sprintf(name, "fuzz_%d", iter);
        mod = compile_and_load_fz(g.code, name);
        ASSERT_TRUE(mod != NULL);
        for (i = 0; i < sp2.n; i++) {
            char sym[64];
            void (*fn)(void*, void*, size_t);
            sprintf(sym, "migrate_%s", sp2.s[i].name);
            fn = (void (*)(void*, void*, size_t))dodai_lib_symbol(mod, sym);
            ASSERT_TRUE(fn != NULL);
            memset(oldblk, 0x5A, sizeof(oldblk));
            memset(newblk, 0xCD, sizeof(newblk));
            fn(oldblk, newblk, 4); /* must not crash; block is far larger than 4 structs */
        }
        dodai_lib_close(mod);
    }
}

/* ============================================================== stress == */

/* migration TUs compile from the build/seni_out/migration_NNN.c audit log */
static void *compile_and_load_st(const char* code, const char* name) {
    char src_path[256];
    char lib_path[256];
    char err_path[256];
    if (seni_dump_migration(code, name, src_path, sizeof(src_path)) != 0) return NULL;
    sprintf(lib_path, "build/%s.%s", name, dodai_lib_extension());
    sprintf(err_path, "build/%s.err", name);
    if (seni_test_compile(src_path, lib_path, err_path) != 0) {
        fprintf(stderr, "gcc failed for %s, generated code:\n%s\n", src_path, code);
        return NULL;
    }
    return dodai_lib_open(michi_from_cstr(lib_path));
}

/* ---- ceiling: 256 structs x 16 fields, all names at the 64-char cap ----- */

#define BIG_STRUCTS 256

static char big_h1[524288];
static char big_h2[524288];
static char big_arena[8 * 1024 * 1024];

static void cap_name(char* out, const char* prefix, unsigned long idx) {
    int n = sprintf(out, "%s%02lu_", prefix, idx);
    while (n < 64) out[n++] = 'x';
    out[64] = '\0';
}

static void emit_big(char* buf, int extra_field_per_struct) {
    int o = 0;
    unsigned long s, f;
    char name[65];
    for (s = 0; s < BIG_STRUCTS; s++) {
        cap_name(name, "s", s);
        o += sprintf(&buf[o], "typedef struct {\n");
        for (f = 0; f < 16; f++) {
            char fname[65];
            cap_name(fname, "f", f);
            o += sprintf(&buf[o], "    int %s[%d];\n", fname, 64);
        }
        if (extra_field_per_struct) {
            char fname[65];
            cap_name(fname, "g", s);
            o += sprintf(&buf[o], "    double %s;\n", fname);
        }
        o += sprintf(&buf[o], "} %s;\n", name);
    }
    buf[o] = '\0';
}

UTEST(stress, max_structs_max_names) {
    arena a;
    diff_result d;
    generate_result g;
    char want[80];
    emit_big(big_h1, 0);
    emit_big(big_h2, 1); /* v2 adds one double per struct */
    create_arena(&a, big_arena, sizeof(big_arena));
    d = diff_structs(&a, big_h1, big_h2);
    ASSERT_FALSE(d.err);
    ASSERT_EQ((size_t)BIG_STRUCTS, d.value.struct_count);
    ASSERT_EQ((size_t)17, d.value.structs[BIG_STRUCTS - 1].ops_count);
    g = generate_migration(&a, d.value);
    ASSERT_FALSE(g.err);
    cap_name(want + 8, "s", BIG_STRUCTS - 1);
    memcpy(want, "migrate_", 8);
    ASSERT_TRUE(strstr(g.code, want) != NULL);
    fprintf(stderr, "stress.max: header %lu bytes, generated %lu bytes, arena used %lu\n",
            (unsigned long)strlen(big_h1), (unsigned long)strlen(g.code), (unsigned long)a.offset);
}

/* ---- exhaustion: every arena size from 0..4096 must succeed or error,
        never crash or corrupt. catches a missing OOM check at any single
        allocation site. ----------------------------------------------- */

UTEST(stress, arena_exhaustion_sweep) {
    static char sweep_buf[4096];
    char* old_header = "typedef struct { float x, y; int health; } enemy;";
    char* new_header = "typedef struct { float x, y; int health; float trail[2]; } enemy;";
    size_t size;
    int successes = 0;
    for (size = 0; size <= sizeof(sweep_buf); size++) {
        arena a;
        diff_result d;
        create_arena(&a, sweep_buf, size);
        d = diff_structs(&a, old_header, new_header);
        if (!d.err) {
            generate_result g = generate_migration(&a, d.value);
            if (!g.err) {
                ASSERT_TRUE(g.code != NULL);
                ASSERT_TRUE(strstr(g.code, "migrate_enemy") != NULL);
                successes++;
            }
        }
    }
    ASSERT_TRUE(successes > 0); /* the full-size run must have made it through */
}

/* ---- throughput: parse a realistic header many times ------------------- */

UTEST(stress, parse_throughput) {
    static char buf[16384];
    char* header =
        "typedef struct { float x, y, z; float vel[3]; int health; } enemy;\n"
        "struct player { char name[32]; double gold; int level; };\n"
        "typedef struct { int ids[64]; float weights[64]; } inventory;\n";
    int iter;
    clock_t t0 = clock();
    double secs;
    for (iter = 0; iter < 20000; iter++) {
        arena a;
        parse_result r;
        create_arena(&a, buf, sizeof(buf));
        r = parse_header(&a, header);
        ASSERT_FALSE(r.err);
        ASSERT_EQ((size_t)3, r.value.struct_count);
    }
    secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "stress.parse_throughput: 20000 parses in %.3fs (%.0f/s)\n",
            secs, secs > 0 ? 20000.0 / secs : 0.0);
}

/* ---- long dev session: 500 reload cycles migrating back and forth ------ */

static const char* H_A = "typedef struct { float x, y; } enemy;";
static const char* H_B = "typedef struct { float x, y; int health; float trail[2]; } enemy;";

typedef struct { float x, y; } enemy_a;
typedef struct { float x, y; int health; float trail[2]; } enemy_b;

static migrate_fn build_dir(arena* a, const char* from, const char* to,
                            const char* name, void **out_mod) {
    diff_result d = diff_structs(a, (char*)from, (char*)to);
    generate_result g;
    void *m;
    if (d.err) { fprintf(stderr, "diff: %s\n", d.err); return NULL; }
    g = generate_migration(a, d.value);
    if (g.err) { fprintf(stderr, "generate: %s\n", g.err); return NULL; }
    m = compile_and_load_st(g.code, name);
    if (!m) return NULL;
    *out_mod = m;
    return (migrate_fn)dodai_lib_symbol(m, "migrate_enemy");
}

UTEST(stress, migration_chain_500_cycles) {
    static char ab_arena[16384];
    static char ba_arena[16384];
    static enemy_a blk_a[256];
    static enemy_b blk_b[256];
    arena aa, ab;
    void *mod_ab = NULL, *mod_ba = NULL;
    migrate_fn a_to_b, b_to_a;
    int i, cycle;

    create_arena(&aa, ab_arena, sizeof(ab_arena));
    create_arena(&ab, ba_arena, sizeof(ba_arena));
    a_to_b = build_dir(&aa, H_A, H_B, "stress_ab", &mod_ab);
    b_to_a = build_dir(&ab, H_B, H_A, "stress_ba", &mod_ba);
    ASSERT_TRUE(a_to_b != NULL);
    ASSERT_TRUE(b_to_a != NULL);

    for (i = 0; i < 256; i++) {
        blk_a[i].x = (float)i * 1.5f;
        blk_a[i].y = (float)i * 2.5f;
    }
    for (cycle = 0; cycle < 500; cycle++) {
        memset(blk_b, 0xCD, sizeof(blk_b));
        a_to_b(blk_a, blk_b, 256);
        memset(blk_a, 0xCD, sizeof(blk_a));
        b_to_a(blk_b, blk_a, 256);
        /* spot-check every cycle, cheap */
        ASSERT_EQ(0.0f, blk_a[0].x);
        ASSERT_EQ(255.0f * 1.5f, blk_a[255].x);
        ASSERT_EQ(0, blk_b[128].health); /* zeroed on every a->b */
    }
    /* full integrity after 500 round trips */
    for (i = 0; i < 256; i++) {
        ASSERT_EQ((float)i * 1.5f, blk_a[i].x);
        ASSERT_EQ((float)i * 2.5f, blk_a[i].y);
    }
    dodai_lib_close(mod_ab);
    dodai_lib_close(mod_ba);
}

/* ---- repeated load/unload of the same dll ------------------------------ */

UTEST(stress, load_unload_100_cycles) {
    static char arena_buf[16384];
    arena a;
    void *first = NULL;
    migrate_fn fn;
    char lib_path[256];
    int i;
    create_arena(&a, arena_buf, sizeof(arena_buf));
    fn = build_dir(&a, H_A, H_B, "stress_reload", &first);
    ASSERT_TRUE(fn != NULL);
    dodai_lib_close(first);
    sprintf(lib_path, "build/stress_reload.%s", dodai_lib_extension());
    for (i = 0; i < 100; i++) {
        void *m = dodai_lib_open(michi_from_cstr(lib_path));
        ASSERT_TRUE(m != NULL);
        ASSERT_TRUE(dodai_lib_symbol(m, "migrate_enemy") != NULL);
        dodai_lib_close(m);
    }
}

/* ---- 200k-element block through a generated migration ------------------ */

UTEST(stress, migrate_200k_elements) {
    static char arena_buf[16384];
    arena a;
    void *mod = NULL;
    migrate_fn fn;
    size_t n = 200000;
    enemy_a* old_blk = (enemy_a*)malloc(n * sizeof(enemy_a));
    enemy_b* new_blk = (enemy_b*)malloc(n * sizeof(enemy_b));
    size_t i;
    clock_t t0;
    double secs;
    ASSERT_TRUE(old_blk != NULL);
    ASSERT_TRUE(new_blk != NULL);
    create_arena(&a, arena_buf, sizeof(arena_buf));
    fn = build_dir(&a, H_A, H_B, "stress_large", &mod);
    ASSERT_TRUE(fn != NULL);

    for (i = 0; i < n; i++) {
        old_blk[i].x = (float)(i % 8191);
        old_blk[i].y = (float)(i % 127);
    }
    memset(new_blk, 0xCD, n * sizeof(enemy_b));
    t0 = clock();
    fn(old_blk, new_blk, n);
    secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "stress.200k: migrated %lu elements (%.1f MB) in %.4fs\n",
            (unsigned long)n, (double)(n * sizeof(enemy_b)) / 1048576.0, secs);

    for (i = 0; i < n; i += 9973) { /* prime stride sampling */
        ASSERT_EQ((float)(i % 8191), new_blk[i].x);
        ASSERT_EQ((float)(i % 127), new_blk[i].y);
        ASSERT_EQ(0, new_blk[i].health);
    }
    ASSERT_EQ((float)((n - 1) % 8191), new_blk[n - 1].x); /* last element exact */
    dodai_lib_close(mod);
    free(old_blk);
    free(new_blk);
}
