#ifndef SENI_H
#define SENI_H

#include <stddef.h>
#include "arena.h"
typedef enum {
    ast_int,
    ast_float,
    ast_char,
    ast_double,
    ast_unknown
} ast_type;

typedef struct {
    char* name;
    ast_type type;
    size_t array_size;  /* 0 = scalar */
    char* was;          /* SENI_WAS(old_name) annotation, NULL = none */
    char* def;          /* SENI_DEFAULT(literal) annotation, NULL = none */
} ast_field;

typedef struct {
    char* name;
    ast_field* fields;
    size_t fields_count;
    char** dropped;        /* SENI_DROPPED(name) annotations in the body */
    size_t dropped_count;
} ast_struct;

typedef struct {
    ast_struct* structs;
    size_t struct_count;
} ast;

typedef enum { field_op_copy, field_op_zero } field_op_kind;

typedef struct {
    field_op_kind kind;
    char* name;              /* field name in new struct */
    char* old_name;          /* source field in old struct; differs from name
                                only for SENI_WAS renames. unused for zero. */
    ast_type type;           /* type in new struct */
    size_t old_array_size;   /* 0 = scalar; meaningful when kind == copy */
    size_t new_array_size;   /* 0 = scalar */
    char* def;               /* SENI_DEFAULT literal for invented values
                                (zero ops, grown array tails); NULL = 0 */
} field_op;

typedef struct {
    char* name;                                 /* struct name */
    ast_field* old_fields; size_t old_count;    /* for emitting old typedef */
    ast_field* new_fields; size_t new_count;    /* for emitting new typedef */
    field_op* ops;         size_t ops_count;    /* one per new field, in new order */
} struct_diff;

typedef struct {
    struct_diff* structs;
    size_t struct_count;
} diff;

typedef enum {
    PARSE_OUTSIDE,
    PARSE_IN_STRUCT,
    PARSE_READ_FIELD_NAME,
    PARSE_READ_STRUCT_NAME
} parse_state;

typedef struct {
    ast value;
    char* err;  /* NULL = success */
} parse_result;

/* the diff cannot recover intent: a same-type remove + add in one struct is
   either a rename (data must move) or a true drop (data must die), and
   guessing silently loses data either way. instead of acting, the diff
   raises a question the caller must surface to the developer, who answers
   by annotating the header (SENI_WAS / SENI_DROPPED, see
   seni_annotations.h). questions are advisory: value/ops are still built
   (the removed field zeroes as before), so callers decide whether to refuse
   the reload until the list is empty. */
typedef struct {
    char* struct_name;
    char* removed;    /* field gone from the old layout */
    char* added;      /* same-type field new in the new layout */
    ast_type type;
    char* message;    /* preformatted, multi-line, ends without newline */
} diff_question;

typedef struct {
    diff value;
    char* err;  /* NULL = success */
    diff_question* questions;
    size_t question_count;
} diff_result;

typedef struct {
    char* code;
    char* err;  /* NULL = success */
} generate_result;

parse_result parse_header(arena* a, char* header);
diff_result diff_structs(arena* a, char* old_header, char* new_header);
generate_result generate_migration(arena* a, diff d);

/* programmatic answers to diff questions: insert an annotation into header
   TEXT (no file io -- callers own reading/writing the file). the edit is
   exactly what a developer would type by hand, so the header stays the
   single source of intent. */
typedef struct {
    char* code;  /* the annotated header text */
    char* err;   /* NULL = success */
} annotate_result;

/* append ` SENI_WAS(old_name)` to field new_name's declarator */
annotate_result annotate_rename(arena* a, char* header, const char* struct_name,
                                const char* old_name, const char* new_name);
/* insert `    SENI_DROPPED(old_name)` before the struct's closing brace */
annotate_result annotate_dropped(arena* a, char* header, const char* struct_name,
                                 const char* old_name);
/* remove every SENI_WAS(...) annotation (outside comments). after a
   successful migration they are all inert -- the next diff's old layout is
   this very header, every field matches by name -- so hosts can strip the
   history once it has served. SENI_DEFAULT / SENI_DROPPED are untouched.
   returns the original pointer in code when nothing was stripped. */
annotate_result strip_was(arena* a, char* header);
#endif
