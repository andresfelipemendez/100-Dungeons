#include "seni.h"
#include "arena.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_NAME 64        /* bound on struct/field names: keeps every generated
                              line and error message far below the 1024-byte
                              format buffers */
#define MAX_ARRAY 65536
#define MAX_TOKEN_ECHO 64  /* longest input token echoed into an error message */

static int is_white_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int is_ident(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* matches ['typedef' ws+] 'struct' [ws+ tag] ws* '{' -- both the typedef
   and the tag-style declaration forms. returns chars consumed (including
   the brace) or 0 on no match; tag span returned via tag/tag_len when the
   declaration names the struct before the brace. shared by the counting
   pass and the parse pass so they can never disagree. */
static int match_struct_start(const char* s, const char** tag, int* tag_len) {
    int i = 0;
    *tag = NULL;
    *tag_len = 0;
    if (strncmp(s, "typedef", 7) == 0 && is_white_space(s[7])) {
        i = 7;
        while (is_white_space(s[i])) i++;
    }
    if (strncmp(&s[i], "struct", 6) != 0) return 0;
    i += 6;
    if (!is_white_space(s[i]) && s[i] != '{') return 0;
    while (is_white_space(s[i])) i++;
    if (is_ident(s[i])) {
        *tag = &s[i];
        while (is_ident(s[i])) { (*tag_len)++; i++; }
        while (is_white_space(s[i])) i++;
    }
    if (s[i] != '{') return 0;
    return i + 1;
}

/* replaces comments with whitespace before parsing, so the state machine and
   the field counting pass never see them. block comments become one space
   (newlines inside are kept so error line numbers stay accurate); line
   comments vanish up to their newline. returns the cleaned copy, or NULL
   with *err_out set. */
static char* strip_comments(arena* a, char* src, char** err_out) {
    size_t len = strlen(src);
    char* dst = allocate_bytes(a, len + 1);
    size_t i = 0;
    size_t o = 0;
    int line = 1;
    *err_out = NULL;
    if (!dst) { *err_out = "out of memory"; return NULL; }
    while (src[i] != '\0') {
        if (src[i] == '/' && src[i + 1] == '*') {
            int cline = line;
            i += 2;
            while (src[i] != '\0' && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') { line++; dst[o++] = '\n'; }
                i++;
            }
            if (src[i] == '\0') {
                char* msg = arena_sprintf(a, "unterminated comment at line %d", cline);
                *err_out = msg ? msg : "unterminated comment";
                return NULL;
            }
            i += 2;
            dst[o++] = ' '; /* a comment between two tokens must keep them apart */
            continue;
        }
        if (src[i] == '/' && src[i + 1] == '/') {
            while (src[i] != '\0' && src[i] != '\n') i++;
            continue; /* the newline itself is copied next iteration */
        }
        if (src[i] == '\n') line++;
        dst[o++] = src[i++];
    }
    dst[o] = '\0';
    return dst;
}

/* shared scan: counts structs, and when field_counts/dropped_counts are
   non-NULL also tallies field separators and SENI_DROPPED annotations per
   struct. running it twice (count, allocate, fill) means there is no fixed
   struct cap -- the arena is the only limit. counts may over-count (the
   parse pass is stricter); they only size allocations. */
static size_t scan_structs(char* header, size_t* field_counts, size_t* dropped_counts) {
    size_t struct_count = 0;
    int in_struct = 0;
    int i;
    const char* tag;
    int tag_len;
    for (i = 0; header[i] != '\0'; i++) {
        int m = !in_struct ? match_struct_start(&header[i], &tag, &tag_len) : 0;
        if (m) {
            struct_count++;
            in_struct = 1;
            i += m - 1;
        } else if (in_struct && (header[i] == ',' || header[i] == ';')) {
            if (field_counts) field_counts[struct_count - 1]++;
        } else if (in_struct && strncmp(&header[i], "SENI_DROPPED", 12) == 0 &&
                   !is_ident(header[i + 12])) {
            if (dropped_counts) dropped_counts[struct_count - 1]++;
            i += 11;
        } else if (in_struct && header[i] == '}') {
            in_struct = 0;
        }
    }
    return struct_count;
}

parse_result parse_header(arena* a, char* header) {
    parse_result r = {0};
    size_t struct_count;
    size_t* field_counts;
    size_t fc;
    int i;
    size_t* dropped_counts;
    parse_state state;
    size_t si;
    size_t fi;
    size_t di;
    ast_type cur_type;
    char* cur_type_name; /* struct name when cur_type == ast_struct_t */
    struct ast_struct* cur_nested; /* referenced struct when ast_struct_t */
    int line;
    const char* tag;
    int tag_len;
    const char* cur_tag = NULL;
    int cur_tag_len = 0;
    char* strip_err;

    header = strip_comments(a, header, &strip_err);
    if (!header) { r.err = strip_err; return r; }

    struct_count = scan_structs(header, NULL, NULL);
    if (struct_count == 0) {
        return r;
    }
    field_counts = allocate(a, sizeof(size_t) * struct_count);
    if (!field_counts) { r.err = "out of memory"; return r; }
    dropped_counts = allocate(a, sizeof(size_t) * struct_count);
    if (!dropped_counts) { r.err = "out of memory"; return r; }
    for (fc = 0; fc < struct_count; fc++) { field_counts[fc] = 0; dropped_counts[fc] = 0; }
    scan_structs(header, field_counts, dropped_counts);
    r.value.struct_count = struct_count;
    r.value.structs = allocate(a, sizeof(ast_struct) * struct_count);
    if (!r.value.structs) { r.err = "out of memory"; return r; }

    state = PARSE_OUTSIDE;
    si = 0;
    fi = 0;
    cur_type = ast_unknown;
    cur_type_name = NULL;
    cur_nested = NULL;

    line = 1;
    for (i = 0; header[i] != '\0'; i++) {
        if (header[i] == '\n') line++;
        if (state == PARSE_OUTSIDE) {
            int m = match_struct_start(&header[i], &tag, &tag_len);
            if (m) {
                int k;
                cur_tag = tag;
                cur_tag_len = tag_len;
                for (k = 1; k < m; k++) {
                    if (header[i + k] == '\n') line++; /* matcher can span lines */
                }
                state = PARSE_IN_STRUCT;
                fi = 0;
                di = 0;
                if (field_counts[si] > 0) {
                    r.value.structs[si].fields = allocate(a, sizeof(ast_field) * field_counts[si]);
                    if (!r.value.structs[si].fields) { r.err = "out of memory"; return r; }
                } else {
                    r.value.structs[si].fields = NULL; /* arena memory is not zeroed */
                }
                r.value.structs[si].fields_count = 0;
                if (dropped_counts[si] > 0) {
                    r.value.structs[si].dropped = allocate(a, sizeof(char*) * dropped_counts[si]);
                    if (!r.value.structs[si].dropped) { r.err = "out of memory"; return r; }
                } else {
                    r.value.structs[si].dropped = NULL;
                }
                r.value.structs[si].dropped_count = 0;
                i += m - 1;
                continue;
            }
        }
        if (state == PARSE_IN_STRUCT && is_white_space(header[i])) {
            continue;
        } else if (state == PARSE_IN_STRUCT && header[i] == '}') {
            int start;
            int len;
            i++;
            while (header[i] != '\0' && is_white_space(header[i])) {
                if (header[i] == '\n') line++;
                i++;
            }
            start = i;
            while (header[i] != ';' && header[i] != '\0') {
                if (header[i] == '\n') line++;
                i++;
            }
            len = i - start;
            while (len > 0 && is_white_space(header[start + len - 1])) len--;
            if (len == 0 && cur_tag_len > 0) {
                /* tag-style 'struct foo { ... };' -- the tag is the name */
                start = (int)(cur_tag - header);
                len = cur_tag_len;
            }
            if (len == 0) {
                char* msg = arena_sprintf(a, "struct missing name at line %d", line);
                r.err = msg ? msg : "struct missing name";
                return r;
            }
            if (len > MAX_NAME) {
                char* msg = arena_sprintf(a, "struct name too long (%d chars, max %d) at line %d", len, MAX_NAME, line);
                r.err = msg ? msg : "struct name too long";
                return r;
            }
            {
                int k;
                for (k = 0; k < len; k++) {
                    if (!is_ident(header[start + k])) {
                        int echo = len > MAX_TOKEN_ECHO ? MAX_TOKEN_ECHO : len;
                        char* msg = arena_sprintf(a, "invalid struct name '%.*s' at line %d", echo, &header[start], line);
                        r.err = msg ? msg : "invalid struct name";
                        return r;
                    }
                }
            }
            r.value.structs[si].name = arena_copy_string(a, &header[start], len);
            if (!r.value.structs[si].name) { r.err = "out of memory"; return r; }
            r.value.structs[si].fields_count = fi;
            r.value.structs[si].dropped_count = di;
            si++;
            state = PARSE_OUTSIDE;
            if (header[i] == '\0') break;
            continue;
        } else if (state == PARSE_IN_STRUCT) {
            /* keyword + any whitespace after it. i stays on the last keyword
               char and we continue, so the loop top sees (and line-counts)
               the whitespace, then READ_FIELD_NAME skips it. */
            if (strncmp(&header[i], "SENI_DROPPED", 12) == 0 && !is_ident(header[i + 12])) {
                /* SENI_DROPPED(name) -- deliberate field removal. no
                   trailing semicolon: the macro expands to nothing and c89
                   forbids a bare ';' in a struct body. */
                int p = i + 12;
                int nstart;
                int nlen;
                while (is_white_space(header[p])) { if (header[p] == '\n') line++; p++; }
                if (header[p] != '(') {
                    char* msg = arena_sprintf(a, "malformed SENI_DROPPED at line %d, expected SENI_DROPPED(field_name)", line);
                    r.err = msg ? msg : "malformed SENI_DROPPED";
                    return r;
                }
                p++;
                while (is_white_space(header[p])) { if (header[p] == '\n') line++; p++; }
                nstart = p;
                while (is_ident(header[p])) p++;
                nlen = p - nstart;
                while (is_white_space(header[p])) { if (header[p] == '\n') line++; p++; }
                if (nlen == 0 || header[p] != ')') {
                    char* msg = arena_sprintf(a, "malformed SENI_DROPPED at line %d, expected SENI_DROPPED(field_name)", line);
                    r.err = msg ? msg : "malformed SENI_DROPPED";
                    return r;
                }
                if (nlen > MAX_NAME) {
                    char* msg = arena_sprintf(a, "SENI_DROPPED name too long (%d chars, max %d) at line %d", nlen, MAX_NAME, line);
                    r.err = msg ? msg : "SENI_DROPPED name too long";
                    return r;
                }
                if (di >= dropped_counts[si]) {
                    r.err = "internal error: dropped count mismatch";
                    return r;
                }
                r.value.structs[si].dropped[di] = arena_copy_string(a, &header[nstart], nlen);
                if (!r.value.structs[si].dropped[di]) { r.err = "out of memory"; return r; }
                di++;
                i = p; /* on ')'; loop increment moves past it */
                continue;
            } else if (strncmp(&header[i], "float", 5) == 0 && is_white_space(header[i + 5])) {
                cur_type = ast_float;
                i += 4;
                state = PARSE_READ_FIELD_NAME;
                continue;
            } else if (strncmp(&header[i], "int", 3) == 0 && is_white_space(header[i + 3])) {
                cur_type = ast_int;
                i += 2;
                state = PARSE_READ_FIELD_NAME;
                continue;
            } else if (strncmp(&header[i], "char", 4) == 0 && is_white_space(header[i + 4])) {
                cur_type = ast_char;
                i += 3;
                state = PARSE_READ_FIELD_NAME;
                continue;
            } else if (strncmp(&header[i], "double", 6) == 0 && is_white_space(header[i + 6])) {
                cur_type = ast_double;
                i += 5;
                state = PARSE_READ_FIELD_NAME;
                continue;
            } else {
                int start = i;
                int len;
                int echo;
                int k;
                int matched = 0;
                size_t sk;
                char* msg;
                while (header[i] != '\0' && !is_white_space(header[i])) i++;
                len = i - start;
                /* a field whose type is another struct already parsed in this
                   header -> a nested struct field */
                for (sk = 0; sk < si; sk++) {
                    char* sn = r.value.structs[sk].name;
                    if (sn && (int)strlen(sn) == len &&
                        strncmp(&header[start], sn, (size_t)len) == 0) {
                        cur_type = ast_struct_t;
                        cur_type_name = sn;
                        cur_nested = &r.value.structs[sk];
                        matched = 1;
                        break;
                    }
                }
                if (matched) {
                    i--; /* land on the last type char; loop ++ then field name */
                    state = PARSE_READ_FIELD_NAME;
                    continue;
                }
                echo = len > MAX_TOKEN_ECHO ? MAX_TOKEN_ECHO : len;
                for (k = 0; k < len; k++) {
                    if (header[start + k] == '*') {
                        msg = arena_sprintf(a, "pointer type '%.*s' at line %d, pointers are not supported, use an index or handle", echo, &header[start], line);
                        r.err = msg ? msg : "pointer type not supported";
                        return r;
                    }
                }
                msg = arena_sprintf(a, "unknown type '%.*s' at line %d", echo, &header[start], line);
                r.err = msg ? msg : "unknown type";
                return r;
            }
        }
        if (state == PARSE_READ_FIELD_NAME && is_white_space(header[i])) {
            continue;
        } else if (state == PARSE_READ_FIELD_NAME) {
            int start = i;
            int len;
            int k;
            size_t arr_size = 0;
            char* was = NULL;
            char* def = NULL;
            while (header[i] != ';' && header[i] != ',' && header[i] != '\0') {
                if (header[i] == '\n') line++;
                i++;
            }
            if (header[i] == '\0') {
                char* msg = arena_sprintf(a, "unexpected end of input at line %d", line);
                r.err = msg ? msg : "unexpected end of input";
                return r;
            }
            len = i - start;
            while (len > 0 && is_white_space(header[start + len - 1])) len--;
            /* optional trailing annotations, in any order:
                   SENI_WAS(old_name)     -- rename source
                   SENI_DEFAULT(literal)  -- value for migration-invented data
               record them and slice them off, so the rest of the declarator
               parses as before */
            for (k = 0; k + 8 <= len; k++) {
                if (strncmp(&header[start + k], "SENI_", 5) == 0 &&
                    (k == 0 || !is_ident(header[start + k - 1])) &&
                    (strncmp(&header[start + k], "SENI_WAS", 8) == 0 ||
                     strncmp(&header[start + k], "SENI_DEFAULT", 12) == 0)) break;
            }
            if (k + 8 <= len) {
                int p = start + k;
                int end = start + len;
                while (p < end) {
                    int is_was;
                    int nstart;
                    int nlen;
                    if (strncmp(&header[p], "SENI_WAS", 8) == 0 &&
                        !is_ident(header[p + 8])) {
                        is_was = 1;
                        p += 8;
                    } else if (strncmp(&header[p], "SENI_DEFAULT", 12) == 0 &&
                               !is_ident(header[p + 12])) {
                        is_was = 0;
                        p += 12;
                    } else {
                        int echo = end - p > MAX_TOKEN_ECHO ? MAX_TOKEN_ECHO : end - p;
                        char* msg = arena_sprintf(a, "unexpected '%.*s' in annotations at line %d", echo, &header[p], line);
                        r.err = msg ? msg : "unexpected text in annotations";
                        return r;
                    }
                    while (p < end && is_white_space(header[p])) p++;
                    if (p >= end || header[p] != '(') {
                        char* msg = arena_sprintf(a, "malformed %s at line %d, expected an argument in parentheses",
                                                  is_was ? "SENI_WAS" : "SENI_DEFAULT", line);
                        r.err = msg ? msg : "malformed annotation";
                        return r;
                    }
                    p++;
                    while (p < end && is_white_space(header[p])) p++;
                    nstart = p;
                    if (is_was) {
                        while (p < end && is_ident(header[p])) p++;
                    } else {
                        /* default literal: any run without parens; the
                           migration compiler type-checks the text */
                        while (p < end && header[p] != ')' && header[p] != '(') p++;
                    }
                    nlen = p - nstart;
                    while (nlen > 0 && is_white_space(header[nstart + nlen - 1])) nlen--;
                    while (p < end && is_white_space(header[p])) p++;
                    if (nlen == 0 || p >= end || header[p] != ')') {
                        char* msg = arena_sprintf(a, "malformed %s at line %d, empty or unterminated argument",
                                                  is_was ? "SENI_WAS" : "SENI_DEFAULT", line);
                        r.err = msg ? msg : "malformed annotation";
                        return r;
                    }
                    p++;
                    if (nlen > MAX_NAME) {
                        char* msg = arena_sprintf(a, "%s argument too long (%d chars, max %d) at line %d",
                                                  is_was ? "SENI_WAS" : "SENI_DEFAULT", nlen, MAX_NAME, line);
                        r.err = msg ? msg : "annotation argument too long";
                        return r;
                    }
                    if (is_was) {
                        was = arena_copy_string(a, &header[nstart], nlen);
                        if (!was) { r.err = "out of memory"; return r; }
                    } else {
                        def = arena_copy_string(a, &header[nstart], nlen);
                        if (!def) { r.err = "out of memory"; return r; }
                    }
                    while (p < end && is_white_space(header[p])) p++;
                }
                len = k;
                while (len > 0 && is_white_space(header[start + len - 1])) len--;
            }
            for (k = 0; k < len; k++) {
                if (header[start + k] == '*') {
                    int echo = len > MAX_TOKEN_ECHO ? MAX_TOKEN_ECHO : len;
                    char* msg = arena_sprintf(a, "pointer field '%.*s' at line %d, pointers are not supported, use an index or handle", echo, &header[start], line);
                    r.err = msg ? msg : "pointer field not supported";
                    return r;
                }
            }
            for (k = 0; k < len && header[start + k] != '['; k++) {}
            if (k < len) {
                int p = k + 1;
                int digits = 0;
                size_t v = 0;
                int overflow = 0;
                while (p < len && header[start + p] >= '0' && header[start + p] <= '9') {
                    if (v > MAX_ARRAY) overflow = 1; /* stop caring about exact value, just flag */
                    else v = v * 10 + (size_t)(header[start + p] - '0');
                    digits++;
                    p++;
                }
                if (digits == 0 || v == 0 || overflow || v > MAX_ARRAY || p >= len || header[start + p] != ']') {
                    int echo = len > MAX_TOKEN_ECHO ? MAX_TOKEN_ECHO : len;
                    char* msg = arena_sprintf(a, "invalid array size for field '%.*s' at line %d", echo, &header[start], line);
                    r.err = msg ? msg : "invalid array size";
                    return r;
                }
                arr_size = v;
                len = k;
                while (len > 0 && is_white_space(header[start + len - 1])) len--;
            }
            if (len == 0) {
                char* msg = arena_sprintf(a, "empty field name at line %d", line);
                r.err = msg ? msg : "empty field name";
                return r;
            }
            if (len > MAX_NAME) {
                char* msg = arena_sprintf(a, "field name too long (%d chars, max %d) at line %d", len, MAX_NAME, line);
                r.err = msg ? msg : "field name too long";
                return r;
            }
            for (k = 0; k < len; k++) {
                if (!is_ident(header[start + k])) {
                    char* msg = arena_sprintf(a, "invalid field name '%.*s' at line %d", len, &header[start], line);
                    r.err = msg ? msg : "invalid field name";
                    return r;
                }
            }
            if (fi >= field_counts[si]) {
                r.err = "internal error: field count mismatch";
                return r;
            }
            r.value.structs[si].fields[fi].name = arena_copy_string(a, &header[start], len);
            if (!r.value.structs[si].fields[fi].name) { r.err = "out of memory"; return r; }
            r.value.structs[si].fields[fi].type = cur_type;
            r.value.structs[si].fields[fi].type_name = cur_type_name;
            r.value.structs[si].fields[fi].nested = cur_nested;
            r.value.structs[si].fields[fi].array_size = arr_size;
            r.value.structs[si].fields[fi].was = was;
            r.value.structs[si].fields[fi].def = def;
            fi++;
            if (header[i] == ',') {
                state = PARSE_READ_FIELD_NAME;  /* comma-list keeps cur_type[_name] */
            } else {
                state = PARSE_IN_STRUCT;
                cur_type_name = NULL;           /* next field re-resolves its type */
                cur_nested = NULL;
            }
        }
    }

    if (state != PARSE_OUTSIDE) {
        char* msg = arena_sprintf(a, "unexpected end of input at line %d", line);
        r.err = msg ? msg : "unexpected end of input";
        return r;
    }
    /* counting pass can over-count (e.g. struct keyword inside a region the
       parse pass consumed differently); never report structs that were not
       actually parsed */
    if (si < r.value.struct_count) r.value.struct_count = si;

    return r;
}

static const char* type_name(ast_type t) {
    switch (t) {
        case ast_int: return "int";
        case ast_float: return "float";
        case ast_char: return "char";
        case ast_double: return "double";
        default: return "void";
    }
}

/* arena-backed string builder: consecutive appends are contiguous because the
   arena hands out sequential memory; each append backs off its own null
   terminator so the next append overwrites it.
   c89: no vsnprintf, so format into a fixed temp first. */
typedef struct {
    arena* a;
    char* start;
    char* err;
} strbuf;

static void sb_appendf(strbuf* b, const char* fmt, ...) {
    char tmp[1024];
    va_list args;
    int n;
    char* dst;
    if (b->err) return;
    va_start(args, fmt);
    n = vsprintf(tmp, fmt, args);
    va_end(args);
    if (n < 0) { b->err = "format error"; return; }
    dst = allocate_bytes(b->a, (size_t)n + 1); /* unaligned: must stay contiguous with previous append */
    if (!dst) { b->err = "out of memory"; return; }
    if (!b->start) b->start = dst;
    memcpy(dst, tmp, (size_t)n + 1);
    b->a->offset -= 1;
}

static void emit_field(strbuf* b, ast_field* f) {
    if (f->type == ast_struct_t) {
        /* a nested struct field is emitted inline (anonymous struct) so the
           migration dll needs no separate typedef for the referenced struct,
           and the old/new typedefs each carry their own snapshot of it. the
           parser always resolves nested for ast_struct_t, and the diff rejects
           arrays of structs, so this is a scalar struct member. */
        size_t k;
        sb_appendf(b, "struct { ");
        for (k = 0; k < f->nested->fields_count; k++)
            emit_field(b, &f->nested->fields[k]);
        sb_appendf(b, "} %s; ", f->name);
        return;
    }
    if (f->array_size > 0)
        sb_appendf(b, "%s %s[%lu]; ", type_name(f->type), f->name, (unsigned long)f->array_size);
    else
        sb_appendf(b, "%s %s; ", type_name(f->type), f->name);
}

/* a nested array member would need a `j` index inside the nested loop too. */
static int ops_need_j(field_op* ops, size_t n) {
    size_t k;
    for (k = 0; k < n; k++) {
        if (ops[k].old_array_size > 0 || ops[k].new_array_size > 0) return 1;
        if (ops[k].nested && ops_need_j(ops[k].nested->ops, ops[k].nested->ops_count)) return 1;
    }
    return 0;
}

/* emit one field's migration. lhs/rhs are the base lvalues ("n[i]" / "o[i]" at
   the top level, "n[i].ed" / "o[i].ed" one level down): the field is reached as
   `<lhs>.<name>`. a struct field recurses into its members with the dotted base
   extended. when a struct field is new (kind == zero) its nested ops are all
   zero, so rhs is never dereferenced. */
static void emit_op(strbuf* b, field_op* op, const char* lhs, const char* rhs) {
    if (op->nested) {
        /* build the dotted base on the stack -- NOT via the arena: sb_appendf
           relies on its appends being contiguous in the arena, so allocating a
           string there mid-build would splice into the output. */
        char nlhs[512];
        char nrhs[512];
        size_t k;
        /* lhs and rhs grow in lockstep (n[i]... vs o[i]..., identical length),
           and every member name is bounded by MAX_NAME, so reserving MAX_NAME
           for the leaf on top of the current base bounds both buffers with a
           single check -- no per-side comparison that could never differ. */
        if (strlen(lhs) + MAX_NAME + 2 > sizeof nlhs) {
            b->err = "nested field path too long"; return;
        }
        sprintf(nlhs, "%s.%s", lhs, op->name);
        sprintf(nrhs, "%s.%s", rhs, op->old_name);
        for (k = 0; k < op->nested->ops_count; k++)
            emit_op(b, &op->nested->ops[k], nlhs, nrhs);
        return;
    }
    if (op->kind == field_op_copy) {
        /* old_name differs from name only for SENI_WAS renames */
        if (op->old_array_size == 0 && op->new_array_size == 0) {
            sb_appendf(b, "        %s.%s = %s.%s;\n", lhs, op->name, rhs, op->old_name);
        } else if (op->old_array_size > 0 && op->new_array_size > 0) {
            size_t m = op->old_array_size < op->new_array_size ? op->old_array_size : op->new_array_size;
            sb_appendf(b, "        for (j = 0; j < %lu; j++) %s.%s[j] = %s.%s[j];\n",
                       (unsigned long)m, lhs, op->name, rhs, op->old_name);
            if (op->new_array_size > m)
                sb_appendf(b, "        for (j = %lu; j < %lu; j++) %s.%s[j] = %s;\n",
                           (unsigned long)m, (unsigned long)op->new_array_size, lhs, op->name,
                           op->def ? op->def : "0");
        } else if (op->old_array_size == 0) { /* scalar -> array */
            sb_appendf(b, "        %s.%s[0] = %s.%s;\n", lhs, op->name, rhs, op->old_name);
            if (op->new_array_size > 1)
                sb_appendf(b, "        for (j = 1; j < %lu; j++) %s.%s[j] = %s;\n",
                           (unsigned long)op->new_array_size, lhs, op->name,
                           op->def ? op->def : "0");
        } else { /* array -> scalar */
            sb_appendf(b, "        %s.%s = %s.%s[0];\n", lhs, op->name, rhs, op->old_name);
        }
    } else {
        /* invented values: SENI_DEFAULT's literal when given, else 0. the
           literal is emitted verbatim -- the migration compiler type-checks it
           against the field */
        if (op->new_array_size == 0)
            sb_appendf(b, "        %s.%s = %s;\n", lhs, op->name, op->def ? op->def : "0");
        else
            sb_appendf(b, "        for (j = 0; j < %lu; j++) %s.%s[j] = %s;\n",
                       (unsigned long)op->new_array_size, lhs, op->name,
                       op->def ? op->def : "0");
    }
}

generate_result generate_migration(arena* a, diff d) {
    generate_result r = {0};
    strbuf b;
    size_t i;
    b.a = a;
    b.start = NULL;
    b.err = NULL;
    sb_appendf(&b, "#include <stddef.h>\n\n"
                   "#if defined(_WIN32)\n"
                   "#define SENI_EXPORT __declspec(dllexport)\n"
                   "#else\n"
                   "#define SENI_EXPORT\n"
                   "#endif\n\n");
    for (i = 0; i < d.struct_count; i++) {
        struct_diff* sd = &d.structs[i];
        size_t j;
        int need_j = ops_need_j(sd->ops, sd->ops_count);
        /* seni__ prefix keeps generated typedefs from colliding with user
           structs named e.g. 'enemy_old'. migrate_<name> stays unprefixed:
           it is the symbol engines look up. */
        if (sd->old_count > 0) {
            sb_appendf(&b, "typedef struct { ");
            for (j = 0; j < sd->old_count; j++)
                emit_field(&b, &sd->old_fields[j]);
            sb_appendf(&b, "} seni__%s_old;\n", sd->name);
        }
        sb_appendf(&b, "typedef struct { ");
        for (j = 0; j < sd->new_count; j++)
            emit_field(&b, &sd->new_fields[j]);
        sb_appendf(&b, "} seni__%s_new;\n", sd->name);

        /* element sizes for both layouts: the migration dll is the only
           artifact that compiled both typedefs, so it is the only place a
           reload driver can learn the new stride (to allocate in the new
           image) and the old stride (to cross-check against the registry
           entry the old code wrote -- a mismatch means the registry and the
           diffed old layout disagree). old size is 0 when the struct did not
           exist in the old layout. */
        if (sd->old_count > 0)
            sb_appendf(&b, "SENI_EXPORT const size_t migrate_%s_old_size = sizeof(seni__%s_old);\n",
                       sd->name, sd->name);
        else
            sb_appendf(&b, "SENI_EXPORT const size_t migrate_%s_old_size = 0;\n", sd->name);
        sb_appendf(&b, "SENI_EXPORT const size_t migrate_%s_new_size = sizeof(seni__%s_new);\n",
                   sd->name, sd->name);

        sb_appendf(&b, "SENI_EXPORT void migrate_%s(void* old_p, void* new_p, size_t count) {\n", sd->name);
        if (sd->old_count > 0)
            sb_appendf(&b, "    seni__%s_old* o = (seni__%s_old*)old_p;\n", sd->name, sd->name);
        sb_appendf(&b, "    seni__%s_new* n = (seni__%s_new*)new_p;\n", sd->name, sd->name);
        sb_appendf(&b, "    size_t i;\n");
        if (need_j)
            sb_appendf(&b, "    size_t j;\n");
        if (sd->old_count == 0)
            sb_appendf(&b, "    (void)old_p;\n");
        sb_appendf(&b, "    for (i = 0; i < count; i++) {\n");
        for (j = 0; j < sd->ops_count; j++)
            emit_op(&b, &sd->ops[j], "n[i]", "o[i]");
        sb_appendf(&b, "    }\n}\n\n");
    }
    if (b.err) { r.err = b.err; return r; }
    /* claim the final null terminator so later allocations don't clobber it */
    if (!allocate_bytes(a, 1)) { r.err = "out of memory"; return r; }
    r.code = b.start;
    return r;
}

/* ---- annotation surgery ------------------------------------------------ */

/* if a comment starts at i, return the index just past it; else i. an
   unterminated block comment runs to the end of the text. */
static size_t skip_comment(const char* s, size_t i) {
    if (s[i] == '/' && s[i + 1] == '*') {
        i += 2;
        while (s[i] != '\0' && !(s[i] == '*' && s[i + 1] == '/')) i++;
        if (s[i] != '\0') i += 2;
    } else if (s[i] == '/' && s[i + 1] == '/') {
        while (s[i] != '\0' && s[i] != '\n') i++;
    }
    return i;
}

/* locate struct `name` in header text (comment-aware, both typedef and
   tag styles): *body_out = index just past '{', *close_out = index of '}'.
   returns 0 when found. */
static int find_struct_body(char* header, const char* name,
                            size_t* body_out, size_t* close_out) {
    size_t i = 0;
    size_t nlen = strlen(name);
    const char* tag;
    int tag_len;
    while (header[i] != '\0') {
        size_t j = skip_comment(header, i);
        if (j != i) { i = j; continue; }
        {
            int m = match_struct_start(&header[i], &tag, &tag_len);
            if (m) {
                size_t body = i + (size_t)m;
                size_t k = body;
                size_t close;
                size_t ns;
                while (header[k] != '\0' && header[k] != '}') {
                    size_t k2 = skip_comment(header, k);
                    if (k2 != k) { k = k2; continue; }
                    k++;
                }
                if (header[k] == '\0') return 1;
                close = k;
                k++;
                while (is_white_space(header[k])) k++;
                ns = k;
                while (is_ident(header[k])) k++;
                if ((k - ns == nlen && strncmp(&header[ns], name, nlen) == 0) ||
                    (k == ns && (size_t)tag_len == nlen && strncmp(tag, name, nlen) == 0)) {
                    *body_out = body;
                    *close_out = close;
                    return 0;
                }
                i = k;
                continue;
            }
        }
        i++;
    }
    return 1;
}

/* header[0..at) + insert + header[at..] */
static annotate_result splice(arena* a, char* header, size_t at, char* insert) {
    annotate_result r = {0};
    size_t hlen = strlen(header);
    size_t ilen = strlen(insert);
    char* out = allocate_bytes(a, hlen + ilen + 1);
    if (!out) { r.err = "out of memory"; return r; }
    memcpy(out, header, at);
    memcpy(out + at, insert, ilen);
    memcpy(out + at + ilen, header + at, hlen - at + 1);
    r.code = out;
    return r;
}

annotate_result annotate_rename(arena* a, char* header, const char* struct_name,
                                const char* old_name, const char* new_name) {
    annotate_result r = {0};
    size_t body;
    size_t close;
    size_t i;
    size_t at = 0;
    size_t nlen = strlen(new_name);
    char* ins;
    if (find_struct_body(header, struct_name, &body, &close)) {
        char* msg = arena_sprintf(a, "annotate: struct '%s' not found in header", struct_name);
        r.err = msg ? msg : "annotate: struct not found";
        return r;
    }
    i = body;
    while (i < close) {
        size_t j = skip_comment(header, i);
        if (j != i) { i = j; continue; }
        if (is_ident(header[i])) {
            size_t s = i;
            while (i < close && is_ident(header[i])) i++;
            if (i - s == nlen && strncmp(&header[s], new_name, nlen) == 0) {
                /* declarator position: optional [N], then ';' or ',' --
                   anything else (e.g. an existing SENI_WAS) is no match */
                size_t p = i;
                while (p < close && is_white_space(header[p])) p++;
                if (header[p] == '[') {
                    while (p < close && header[p] != ']') p++;
                    if (header[p] == ']') p++;
                    while (p < close && is_white_space(header[p])) p++;
                }
                if (p < close && (header[p] == ';' || header[p] == ',')) { at = p; break; }
            }
            continue;
        }
        i++;
    }
    if (at == 0) {
        char* msg = arena_sprintf(a, "annotate: field '%s' not found in struct '%s' (or already annotated)",
                                  new_name, struct_name);
        r.err = msg ? msg : "annotate: field not found";
        return r;
    }
    ins = arena_sprintf(a, " SENI_WAS(%s)", old_name);
    if (!ins) { r.err = "out of memory"; return r; }
    return splice(a, header, at, ins);
}

annotate_result annotate_dropped(arena* a, char* header, const char* struct_name,
                                 const char* old_name) {
    annotate_result r = {0};
    size_t body;
    size_t close;
    size_t i;
    size_t ls;
    size_t nlen = strlen(old_name);
    char* ins;
    if (find_struct_body(header, struct_name, &body, &close)) {
        char* msg = arena_sprintf(a, "annotate: struct '%s' not found in header", struct_name);
        r.err = msg ? msg : "annotate: struct not found";
        return r;
    }
    /* refuse a duplicate SENI_DROPPED(old_name) */
    i = body;
    while (i < close) {
        size_t j = skip_comment(header, i);
        if (j != i) { i = j; continue; }
        if (strncmp(&header[i], "SENI_DROPPED", 12) == 0 && !is_ident(header[i + 12])) {
            size_t p = i + 12;
            size_t s;
            while (p < close && is_white_space(header[p])) p++;
            if (header[p] == '(') {
                p++;
                while (p < close && is_white_space(header[p])) p++;
                s = p;
                while (p < close && is_ident(header[p])) p++;
                if (p - s == nlen && strncmp(&header[s], old_name, nlen) == 0) {
                    char* msg = arena_sprintf(a, "annotate: SENI_DROPPED(%s) already present in struct '%s'",
                                              old_name, struct_name);
                    r.err = msg ? msg : "annotate: already dropped";
                    return r;
                }
            }
            i = p;
            continue;
        }
        i++;
    }
    /* insert as its own line above the line holding the closing brace */
    ls = close;
    while (ls > 0 && header[ls - 1] != '\n') ls--;
    ins = arena_sprintf(a, "    SENI_DROPPED(%s)\n", old_name);
    if (!ins) { r.err = "out of memory"; return r; }
    return splice(a, header, ls, ins);
}

annotate_result strip_was(arena* a, char* header) {
    annotate_result r = {0};
    size_t hlen = strlen(header);
    char* out;
    size_t o = 0;
    size_t i = 0;
    int stripped = 0;
    out = allocate_bytes(a, hlen + 1);
    if (!out) { r.err = "out of memory"; return r; }
    while (header[i] != '\0') {
        size_t j = skip_comment(header, i);
        if (j != i) {
            /* comments copy verbatim -- prose may mention SENI_WAS */
            memcpy(out + o, header + i, j - i);
            o += j - i;
            i = j;
            continue;
        }
        if (strncmp(&header[i], "SENI_WAS", 8) == 0 &&
            (i == 0 || !is_ident(header[i - 1])) &&
            !is_ident(header[i + 8])) {
            size_t p = i + 8;
            while (header[p] != '\0' && is_white_space(header[p])) p++;
            if (header[p] == '(') {
                size_t q = p + 1;
                while (header[q] != '\0' && header[q] != ')' && header[q] != ';' &&
                       header[q] != '\n') q++;
                if (header[q] == ')') {
                    /* drop the annotation plus the space run before it */
                    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\t')) o--;
                    i = q + 1;
                    stripped = 1;
                    continue;
                }
            }
            /* malformed: leave it for the parser to report */
        }
        out[o++] = header[i++];
    }
    out[o] = '\0';
    if (!stripped) {
        r.code = header; /* untouched: callers can skip the file write */
        return r;
    }
    r.code = out;
    return r;
}

/* questions are discovered struct by struct but returned as one array;
   collect them in an arena-backed chain, then flatten */
typedef struct seni_q_node {
    diff_question q;
    struct seni_q_node* next;
} seni_q_node;

/* build the field-by-field ops migrating old struct `os` (NULL if the struct is
   new) into new struct `ns`, filling `sd`. recurses for nested struct fields so
   `op->nested` carries the inner diff. ambiguity questions are raised only by
   the caller, for the top-level structs -- a nested rename is reported at the
   member it lands on, which is enough to act on. returns an error string
   (arena-owned) or NULL. */
static char* build_ops(arena* a, ast_struct* os, ast_struct* ns, struct_diff* sd) {
    size_t f;
    sd->name = ns->name;
    sd->new_fields = ns->fields;
    sd->new_count = ns->fields_count;
    sd->old_fields = os ? os->fields : NULL;
    sd->old_count = os ? os->fields_count : 0;
    sd->ops_count = ns->fields_count;
    sd->ops = NULL;
    if (ns->fields_count > 0) {
        sd->ops = allocate(a, sizeof(field_op) * ns->fields_count);
        if (!sd->ops) return "out of memory";
    }
    for (f = 0; f < ns->fields_count; f++) {
        field_op* op = &sd->ops[f];
        ast_field* nf = &ns->fields[f];
        ast_struct* old_nested = NULL; /* referenced struct of the matched old field */
        size_t g;
        op->name = nf->name;
        op->old_name = nf->name;
        op->type = nf->type;
        op->kind = field_op_zero;
        op->old_array_size = 0;
        op->new_array_size = nf->array_size;
        op->def = nf->def;
        op->nested = NULL;
        for (g = 0; os && g < os->fields_count; g++) {
            if (strcmp(os->fields[g].name, nf->name) == 0) {
                if (os->fields[g].type != nf->type) {
                    char* msg = arena_sprintf(a, "field '%s' in struct '%s' changed type from %s to %s, cannot migrate",
                                              nf->name, ns->name,
                                              type_name(os->fields[g].type), type_name(nf->type));
                    return msg ? msg : "field changed type, cannot migrate";
                }
                op->kind = field_op_copy;
                op->old_array_size = os->fields[g].array_size;
                old_nested = os->fields[g].nested;
                break;
            }
        }
        /* rename: SENI_WAS is consulted only when no same-name match exists, so
           an annotation left in the header after its rename has migrated is
           inert (the renamed field matches by name on every later diff). when
           consulted, it must resolve -- a typo'd SENI_WAS that silently zeroed
           would be the exact data-loss class the annotation exists to prevent. */
        if (op->kind == field_op_zero && nf->was) {
            for (g = 0; os && g < os->fields_count; g++) {
                if (strcmp(os->fields[g].name, nf->was) == 0) break;
            }
            if (!os || g >= os->fields_count) {
                char* msg = arena_sprintf(a, "field '%s' in struct '%s': SENI_WAS(%s) but old layout has no field '%s'",
                                          nf->name, ns->name, nf->was, nf->was);
                return msg ? msg : "SENI_WAS names a missing old field";
            }
            if (os->fields[g].type != nf->type) {
                char* msg = arena_sprintf(a, "field '%s' in struct '%s': SENI_WAS(%s) but '%s' is %s and '%s' is %s, cannot migrate rename",
                                          nf->name, ns->name, nf->was,
                                          nf->was, type_name(os->fields[g].type),
                                          nf->name, type_name(nf->type));
                return msg ? msg : "SENI_WAS type mismatch, cannot migrate rename";
            }
            op->kind = field_op_copy;
            op->old_name = os->fields[g].name;
            op->old_array_size = os->fields[g].array_size;
            old_nested = os->fields[g].nested;
        }
        /* nested struct field: recurse so op->nested migrates the members.
           the parser always resolves nested for an ast_struct_t field. */
        if (nf->type == ast_struct_t) {
            struct_diff* nsd;
            char* e;
            if (nf->array_size > 0) {
                char* msg = arena_sprintf(a, "field '%s' in struct '%s': arrays of structs are not supported",
                                          nf->name, ns->name);
                return msg ? msg : "arrays of structs are not supported";
            }
            if (old_nested && strcmp(old_nested->name, nf->nested->name) != 0) {
                char* msg = arena_sprintf(a, "field '%s' in struct '%s' changed struct type from %s to %s, cannot migrate",
                                          nf->name, ns->name, old_nested->name, nf->nested->name);
                return msg ? msg : "struct field changed type, cannot migrate";
            }
            nsd = allocate(a, sizeof(struct_diff));
            if (!nsd) return "out of memory";
            /* old_nested is non-NULL only when the field copied from an old
               field -- a new struct field diffs against no old layout, so every
               member defaults. */
            e = build_ops(a, op->kind == field_op_copy ? old_nested : NULL, nf->nested, nsd);
            if (e) return e;
            op->nested = nsd;
        }
    }
    return NULL;
}

diff_result diff_structs(arena* a, char *old_header, char *new_header){
    diff_result res = {0};
    parse_result old_r;
    parse_result new_r;
    ast old_ast;
    ast new_ast;
    size_t i;
    seni_q_node* q_head = NULL;
    seni_q_node* q_tail = NULL;

    old_r = parse_header(a, old_header);
    if (old_r.err) {
        char* msg = arena_sprintf(a, "old_header error: %s", old_r.err);
        res.err = msg ? msg : old_r.err;
        return res;
    }
    new_r = parse_header(a, new_header);
    if (new_r.err) {
        char* msg = arena_sprintf(a, "new_header error: %s", new_r.err);
        res.err = msg ? msg : new_r.err;
        return res;
    }
    old_ast = old_r.value;
    new_ast = new_r.value;

    res.value.struct_count = new_ast.struct_count;
    if (new_ast.struct_count == 0) return res;
    res.value.structs = allocate(a, sizeof(struct_diff) * new_ast.struct_count);
    if (!res.value.structs) { res.err = "out of memory"; return res; }

    for (i = 0; i < new_ast.struct_count; i++) {
        ast_struct* ns = &new_ast.structs[i];
        struct_diff* sd = &res.value.structs[i];
        ast_struct* os = NULL;
        size_t j;
        char* e;

        for (j = 0; j < old_ast.struct_count; j++) {
            if (strcmp(old_ast.structs[j].name, ns->name) == 0) { os = &old_ast.structs[j]; break; }
        }
        e = build_ops(a, os, ns, sd);
        if (e) { res.err = e; return res; }

        /* ambiguity advisor: a removed old field plus a same-type added new
           field is either a rename or a real removal, and the diff cannot
           know which. raise a question per candidate pair unless the header
           already answers it (SENI_WAS consumed the pair above; SENI_DROPPED
           declares the removal deliberate). */
        if (os) {
            size_t g;
            for (g = 0; g < os->fields_count; g++) {
                size_t h;
                int matched = 0;
                int declared_dropped = 0;
                for (h = 0; h < sd->ops_count; h++) {
                    if (sd->ops[h].kind == field_op_copy &&
                        strcmp(sd->ops[h].old_name, os->fields[g].name) == 0) { matched = 1; break; }
                }
                if (matched) continue;
                for (h = 0; h < ns->dropped_count; h++) {
                    if (strcmp(ns->dropped[h], os->fields[g].name) == 0) { declared_dropped = 1; break; }
                }
                if (declared_dropped) continue;
                for (h = 0; h < sd->ops_count; h++) {
                    field_op* op = &sd->ops[h];
                    seni_q_node* node;
                    char* decl;
                    char* msg;
                    if (op->kind != field_op_zero || op->type != os->fields[g].type) continue;
                    if (ns->fields[h].was) continue; /* annotated rename from another field */
                    if (op->new_array_size > 0)
                        decl = arena_sprintf(a, "%s %s[%lu]", type_name(op->type), op->name,
                                             (unsigned long)op->new_array_size);
                    else
                        decl = arena_sprintf(a, "%s %s", type_name(op->type), op->name);
                    if (!decl) { res.err = "out of memory"; return res; }
                    msg = arena_sprintf(a, "struct %s: '%s' removed, '%s' added (both %s)\n"
                                           "      rename?  annotate: %s SENI_WAS(%s);\n"
                                           "      really removed?  annotate: SENI_DROPPED(%s)",
                                        ns->name, os->fields[g].name, op->name, type_name(op->type),
                                        decl, os->fields[g].name, os->fields[g].name);
                    if (!msg) { res.err = "out of memory"; return res; }
                    node = allocate(a, sizeof(seni_q_node));
                    if (!node) { res.err = "out of memory"; return res; }
                    node->q.struct_name = ns->name;
                    node->q.removed = os->fields[g].name;
                    node->q.added = op->name;
                    node->q.type = op->type;
                    node->q.message = msg;
                    node->next = NULL;
                    if (q_tail) { q_tail->next = node; q_tail = node; }
                    else { q_head = q_tail = node; }
                    res.question_count++;
                }
            }
        }
    }

    if (res.question_count > 0) {
        seni_q_node* n = q_head;
        size_t qi = 0;
        res.questions = allocate(a, sizeof(diff_question) * res.question_count);
        if (!res.questions) { res.err = "out of memory"; return res; }
        for (; n; n = n->next) res.questions[qi++] = n->q;
    }
    return res;
}
