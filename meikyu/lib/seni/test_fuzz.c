/* deterministic fuzz suite: fixed PRNG seeds, reproducible failures.
   four layers:
   - random garbage bytes        -> parser must error or return a sane AST
   - mutated valid headers       -> same
   - grammar-generated headers   -> parser must ACCEPT and the AST must match
                                    the spec the generator built (oracle)
   - random spec pairs           -> diff+generate must succeed (or error
                                    exactly when a type change was injected),
                                    and a few survivors compile to real dlls
   on linux test.sh builds this with -fsanitize=address,undefined. */

#include "../dodai/dodai.h"
#ifdef _WIN32
#include "../dodai/dodai_windows.c"
#else
#include "../dodai/dodai_posix.c"
#endif
#include "../kaji/kaji.c"   /* all compilation goes through kaji */
#include "utest.h"
#include "seni.h"

/* compile a seni-generated migration through kaji, C89-pinned. */
static int seni_test_compile(const char *src, const char *lib, const char *err) {
    kaji *k = kaji_new();
    int rc;
    if (!k) return 1;
    rc = kaji_compile_shared(k, src, lib, err,
                             &(kaji_compile_opts){ KAJI_C89, 1 });
    kaji_free(k);
    return rc;
}
#include "arena.h"
#include "arena.c"
#include "seni.c"
#include "seni_dump.h"

UTEST_MAIN()

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
