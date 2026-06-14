#include "utest.h"
#include "seni.h"
#include "arena.h"
#include "arena.c"
#include "seni.c"

UTEST_MAIN()
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

/*
UTEST(foo, bar) {
    char buf[4096];
    arena b;
    create_arena(&b, buf, sizeof(buf));
    char* old_header =
    "typedef struct {"
        "float x, y;"
    "} enemy;";
    // allocate 3 enemies

    char* new_header =
    "typedef struct {"
        "float x, y;"
        "int health;"
    "} enemy;";

     diff_structs(old_header, new_header);
    // migration m = generate_migration(diff);
    // migrate(m, b);
    // assert previous enemies still exist with "old values"

  ASSERT_TRUE(1);
}
*/