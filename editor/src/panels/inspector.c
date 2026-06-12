/* Hot-state inspector: derives its rows from the layout string the dll
   embeds for seni (seni_layout), so it reflects game_state.h without ever
   including it -- the editor stays game-agnostic and the inspector updates
   itself on every reload/migration. Field offsets are computed with the
   same rules gcc uses for these primitive types (size == alignment). */

#include "editor.h"
#include "engine/ui/ui.h"

#include <stdio.h>
#include <string.h>

#include "seni.h" /* parser (compiled into editor.o, see editor_unity.c) */

/* exported by the game's SENI_EMBED_LAYOUT, resolved at dll link */
extern const char *seni_layout;

#define INSPECTOR_MAX_FIELDS 64

typedef struct {
    const char *name;  /* points into the parse arena (persistent) */
    ast_type    type;
    u64         offset;
    u64         array_size; /* 0 = scalar */
} InspectorField;

typedef struct {
    b32            ready;
    InspectorField fields[INSPECTOR_MAX_FIELDS];
    u32            field_count;
    /* frame-lifetime text for value labels (clay keeps slices) */
    char           value_text[INSPECTOR_MAX_FIELDS][32];
    char           id_text[INSPECTOR_MAX_FIELDS][96];
} Inspector;

static Inspector insp;
static char insp_arena_buf[256 * 1024];

static u64 insp_type_size(ast_type t) {
    switch (t) {
    case ast_char:   return 1;
    case ast_int:    return 4;
    case ast_float:  return 4;
    case ast_double: return 8;
    default:         return 0;
    }
}

b32 inspector_init(void) {
    memset(&insp, 0, sizeof(insp));

    arena a;
    create_arena(&a, insp_arena_buf, sizeof(insp_arena_buf));
    char *copy = arena_copy_string(&a, seni_layout, strlen(seni_layout));
    if (!copy) {
        return 0;
    }
    parse_result pr = parse_header(&a, copy);
    if (pr.err || pr.value.struct_count == 0) {
        return 0;
    }

    /* hot memory holds one instance of the first struct (game_state) */
    ast_struct *st = &pr.value.structs[0];
    u64 offset = 0;
    for (u64 i = 0; i < st->fields_count && insp.field_count < INSPECTOR_MAX_FIELDS; i++) {
        ast_field *f = &st->fields[i];
        u64 size = insp_type_size(f->type);
        if (size == 0) {
            return 0; /* unknown type: offsets past it would be wrong */
        }
        offset = (offset + size - 1) & ~(size - 1); /* align == size here */
        InspectorField *out = &insp.fields[insp.field_count++];
        out->name = f->name;
        out->type = f->type;
        out->offset = offset;
        out->array_size = f->array_size;
        offset += size * (f->array_size ? f->array_size : 1);
    }
    insp.ready = 1;
    return 1;
}

static void inspector_scalar_row(InspectorField *f, u32 i, void *hot) {
    char *value = insp.value_text[i];
    char *ids = insp.id_text[i];
    void *p = (u8 *)hot + f->offset;

    f32 fstep = 0.1f;
    snprintf(ids, 96, "insp_row_%s", f->name);
    ui_row_begin(ids);
    ui_label(f->name, 14);

    switch (f->type) {
    case ast_float: {
        f32 *v = (f32 *)p;
        snprintf(ids + 48, 48, "insp_m_%s", f->name);
        if (ui_button(ids + 48, "-")) *v -= fstep;
        snprintf(value, 32, "%.2f", *v);
        ui_label(value, 14);
        snprintf(ids + 48, 48, "insp_p_%s", f->name);
        if (ui_button(ids + 48, "+")) *v += fstep;
    } break;
    case ast_int: {
        s32 *v = (s32 *)p;
        snprintf(ids + 48, 48, "insp_m_%s", f->name);
        if (ui_button(ids + 48, "-")) *v -= 1;
        snprintf(value, 32, "%d", *v);
        ui_label(value, 14);
        snprintf(ids + 48, 48, "insp_p_%s", f->name);
        if (ui_button(ids + 48, "+")) *v += 1;
    } break;
    case ast_double: {
        f64 *v = (f64 *)p;
        snprintf(value, 32, "%.3f", *v);
        ui_label(value, 14);
    } break;
    case ast_char: {
        snprintf(value, 32, "%d", (int)*(char *)p);
        ui_label(value, 14);
    } break;
    default:
        break;
    }
    ui_row_end();
}

/* Draws the inspector's widget rows (inside an open panel). */
void inspector_draw(PlatformMemory *memory) {
    if (!insp.ready) {
        ui_label_dim("inspector: layout unavailable", 14);
        return;
    }
    ui_label_dim("hot state", 14);
    for (u32 i = 0; i < insp.field_count; i++) {
        InspectorField *f = &insp.fields[i];
        if (f->array_size) {
            /* arrays: name + size only, element editing when it is needed */
            snprintf(insp.value_text[i], 32, "[%u]", (u32)f->array_size);
            snprintf(insp.id_text[i], 96, "insp_row_%s", f->name);
            ui_row_begin(insp.id_text[i]);
            ui_label(f->name, 14);
            ui_label_dim(insp.value_text[i], 14);
            ui_row_end();
        } else {
            inspector_scalar_row(f, i, memory->hot);
        }
    }
}
