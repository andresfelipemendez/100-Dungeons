/* The one engine file that speaks clay (and therefore C99) -- compiled into
   its own object (build/ui.o) and linked into the dll; the strict-C89 unity
   build never includes it. See ui.h for the game-facing API. */

#include "engine/ui/ui.h"
#include "engine/render/render.h"

#include <string.h>
#include <SDL3/SDL.h>
#include "clay.h"
#include "stb_truetype.h"

#define UI_BAKE_PX    32.0f /* glyphs baked at this size, scaled at draw */
#define UI_ATLAS_SIZE 512
#define UI_FONT_PATH  "C:/Windows/Fonts/consola.ttf"

#define UI_MAX_PANELS 8

typedef struct {
    const char *name; /* must outlive the registration (string literal) */
    UiPanelFn   fn;
    void       *user;
} UiPanel;

typedef struct {
    b32              ready;
    stbtt_bakedchar  chars[96]; /* ASCII 32..126 */
    RndTexture       atlas;
    f32              ascent;    /* pixels at UI_BAKE_PX */
    b32              mouse_down;
    b32              prev_mouse_down;
    UiPanel          panels[UI_MAX_PANELS]; /* wiped by ui_init on purpose */
    u32              panel_count;
} UiState;

static UiState ui;

u64 ui_memory_required(void) {
    /* clay context + 8bpp bake bitmap + RGBA expansion scratch */
    return (u64)Clay_MinMemorySize()
         + UI_ATLAS_SIZE * UI_ATLAS_SIZE * 5
         + 64;
}

static void ui_clay_error(Clay_ErrorData err) {
    SDL_Log("clay error: %.*s", (int)err.errorText.length, err.errorText.chars);
}

static Clay_String ui_string(const char *chars) {
    return (Clay_String){ .isStaticallyAllocated = false,
                          .length = (int32_t)strlen(chars),
                          .chars = chars };
}

static Clay_Dimensions ui_measure_text(Clay_StringSlice text,
                                       Clay_TextElementConfig *config,
                                       void *user_data) {
    (void)user_data;
    f32 scale = (f32)config->fontSize / UI_BAKE_PX;
    f32 width = 0.0f;
    for (s32 i = 0; i < text.length; i++) {
        unsigned char c = (unsigned char)text.chars[i];
        if (c >= 32 && c < 127) {
            width += ui.chars[c - 32].xadvance * scale;
        }
    }
    return (Clay_Dimensions){ width, (f32)config->fontSize };
}

static b32 ui_font_bake(u8 *bitmap8, u8 *rgba) {
    size_t ttf_size = 0;
    void *ttf = SDL_LoadFile(UI_FONT_PATH, &ttf_size);
    if (!ttf) {
        SDL_Log("ui: cannot load font '%s': %s", UI_FONT_PATH, SDL_GetError());
        return 0;
    }
    if (stbtt_BakeFontBitmap((const unsigned char *)ttf, 0, UI_BAKE_PX,
                             bitmap8, UI_ATLAS_SIZE, UI_ATLAS_SIZE,
                             32, 96, ui.chars) <= 0) {
        SDL_Log("ui: font bake failed");
        SDL_free(ttf);
        return 0;
    }
    stbtt_fontinfo info;
    int ascent_units = 0, descent, gap;
    if (stbtt_InitFont(&info, (const unsigned char *)ttf,
                       stbtt_GetFontOffsetForIndex((const unsigned char *)ttf, 0))) {
        stbtt_GetFontVMetrics(&info, &ascent_units, &descent, &gap);
        ui.ascent = (f32)ascent_units * stbtt_ScaleForPixelHeight(&info, UI_BAKE_PX);
    } else {
        ui.ascent = UI_BAKE_PX * 0.8f;
    }
    SDL_free(ttf);

    /* white glyphs, coverage in alpha */
    for (u32 i = 0; i < UI_ATLAS_SIZE * UI_ATLAS_SIZE; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap8[i];
    }
    ui.atlas = rnd_texture_create_rgba8(rgba, UI_ATLAS_SIZE, UI_ATLAS_SIZE);
    return ui.atlas.id != 0;
}

b32 ui_init(void *memory, u64 size, f32 screen_w, f32 screen_h) {
    b32 prev = ui.prev_mouse_down;
    memset(&ui, 0, sizeof(ui));
    ui.prev_mouse_down = prev;

    u64 clay_size = Clay_MinMemorySize();
    if (size < ui_memory_required()) {
        SDL_Log("ui: %llu bytes given, %llu required",
                (unsigned long long)size, (unsigned long long)ui_memory_required());
        return 0;
    }

    u8 *bitmap8 = (u8 *)memory + clay_size;
    u8 *rgba = bitmap8 + UI_ATLAS_SIZE * UI_ATLAS_SIZE;
    if (!ui_font_bake(bitmap8, rgba)) {
        return 0;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clay_size, memory);
    Clay_Initialize(arena, (Clay_Dimensions){ screen_w, screen_h },
                    (Clay_ErrorHandler){ ui_clay_error, NULL });
    Clay_SetMeasureTextFunction(ui_measure_text, NULL);
    ui.ready = 1;
    return 1;
}

void ui_frame_begin(f32 screen_w, f32 screen_h,
                    f32 mouse_x, f32 mouse_y, b32 mouse_down) {
    if (!ui.ready) {
        return;
    }
    ui.prev_mouse_down = ui.mouse_down;
    ui.mouse_down = mouse_down;
    Clay_SetLayoutDimensions((Clay_Dimensions){ screen_w, screen_h });
    Clay_SetPointerState((Clay_Vector2){ mouse_x, mouse_y }, mouse_down != 0);
    Clay_BeginLayout();
}

/* ---- widgets ---------------------------------------------------------- */

void ui_panel_begin(const char *id, f32 width) {
    if (!ui.ready) {
        return;
    }
    Clay__OpenElementWithId(Clay__HashString(ui_string(id), 0));
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = { .sizing = { .width = CLAY_SIZING_FIXED(width),
                                .height = CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(12),
                    .childGap = 10,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .backgroundColor = { 24, 26, 34, 235 },
    });
}

void ui_panel_end(void) {
    if (!ui.ready) {
        return;
    }
    Clay__CloseElement();
}

void ui_row_begin(const char *id) {
    if (!ui.ready) {
        return;
    }
    Clay__OpenElementWithId(Clay__HashString(ui_string(id), 0));
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) },
                    .childGap = 8,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
    });
}

void ui_row_end(void) {
    if (!ui.ready) {
        return;
    }
    Clay__CloseElement();
}

static void ui_text_internal(const char *text, int font_size, Clay_Color color) {
    if (!ui.ready) {
        return;
    }
    CLAY_TEXT(ui_string(text), CLAY_TEXT_CONFIG({
        .fontSize = (uint16_t)font_size, .textColor = color }));
}

void ui_label(const char *text, int font_size) {
    ui_text_internal(text, font_size, (Clay_Color){ 230, 232, 244, 255 });
}

void ui_label_dim(const char *text, int font_size) {
    ui_text_internal(text, font_size, (Clay_Color){ 150, 155, 170, 255 });
}

b32 ui_button(const char *id, const char *label) {
    if (!ui.ready) {
        return 0;
    }
    Clay_ElementId eid = Clay__HashString(ui_string(id), 0);
    /* hit test uses the previous frame's layout: standard clay pattern */
    b32 clicked = ui.mouse_down && !ui.prev_mouse_down && Clay_PointerOver(eid);

    Clay__OpenElementWithId(eid);
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = { .sizing = { CLAY_SIZING_FIXED(26), CLAY_SIZING_FIXED(26) },
                    .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = Clay_Hovered()
            ? (Clay_Color){ 95, 100, 140, 255 }
            : (Clay_Color){ 60, 64, 90, 255 },
        .cornerRadius = { 4, 4, 4, 4 },
    });
    ui_text_internal(label, 16, (Clay_Color){ 230, 230, 240, 255 });
    Clay__CloseElement();
    return clicked;
}

/* ---- render command translation --------------------------------------- */

static void ui_draw_rect(Clay_BoundingBox bb, Clay_Color c) {
    rnd_ui_quad(bb.x, bb.y, bb.width, bb.height, 0, 0, 1, 1,
                (RndTexture){ 0 },
                c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

static void ui_draw_text(Clay_BoundingBox bb, Clay_TextRenderData *t) {
    f32 scale = (f32)t->fontSize / UI_BAKE_PX;
    f32 baseline = bb.y + ui.ascent * scale;
    f32 xpos = 0.0f, ypos = 0.0f;
    f32 r = t->textColor.r / 255.0f, g = t->textColor.g / 255.0f;
    f32 b = t->textColor.b / 255.0f, a = t->textColor.a / 255.0f;
    for (s32 i = 0; i < t->stringContents.length; i++) {
        unsigned char c = (unsigned char)t->stringContents.chars[i];
        if (c < 32 || c >= 127) {
            continue;
        }
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(ui.chars, UI_ATLAS_SIZE, UI_ATLAS_SIZE, c - 32,
                           &xpos, &ypos, &q, 1);
        rnd_ui_quad(bb.x + q.x0 * scale, baseline + q.y0 * scale,
                    (q.x1 - q.x0) * scale, (q.y1 - q.y0) * scale,
                    q.s0, q.t0, q.s1, q.t1, ui.atlas, r, g, b, a);
    }
}

static void ui_draw_border(Clay_BoundingBox bb, Clay_BorderRenderData *bd) {
    Clay_Color c = bd->color;
    f32 l = bd->width.left, r = bd->width.right;
    f32 t = bd->width.top, b = bd->width.bottom;
    if (t > 0) ui_draw_rect((Clay_BoundingBox){ bb.x, bb.y, bb.width, t }, c);
    if (b > 0) ui_draw_rect((Clay_BoundingBox){ bb.x, bb.y + bb.height - b, bb.width, b }, c);
    if (l > 0) ui_draw_rect((Clay_BoundingBox){ bb.x, bb.y + t, l, bb.height - t - b }, c);
    if (r > 0) ui_draw_rect((Clay_BoundingBox){ bb.x + bb.width - r, bb.y + t, r, bb.height - t - b }, c);
}

b32 ui_panel_register(const char *name, UiPanelFn fn, void *user) {
    for (u32 i = 0; i < ui.panel_count; i++) {
        if (strcmp(ui.panels[i].name, name) == 0) {
            ui.panels[i].fn = fn;
            ui.panels[i].user = user;
            return 1;
        }
    }
    if (ui.panel_count >= UI_MAX_PANELS) {
        SDL_Log("ui: panel registry full (%d panels), cannot register '%s'",
                UI_MAX_PANELS, name);
        return 0;
    }
    ui.panels[ui.panel_count].name = name;
    ui.panels[ui.panel_count].fn = fn;
    ui.panels[ui.panel_count].user = user;
    ui.panel_count++;
    return 1;
}

void ui_frame_end(f32 dt) {
    if (!ui.ready) {
        return;
    }
    /* extension panels: still inside the layout scope, root level, after
       the game's own elements */
    for (u32 i = 0; i < ui.panel_count; i++) {
        ui.panels[i].fn(ui.panels[i].user);
    }
    Clay_RenderCommandArray cmds = Clay_EndLayout(dt);
    for (s32 i = 0; i < cmds.length; i++) {
        Clay_RenderCommand *cmd = &cmds.internalArray[i];
        Clay_BoundingBox bb = cmd->boundingBox;
        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            ui_draw_rect(bb, cmd->renderData.rectangle.backgroundColor);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            ui_draw_text(bb, &cmd->renderData.text);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            ui_draw_border(bb, &cmd->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            rnd_ui_scissor((s32)bb.x, (s32)bb.y, (s32)bb.width, (s32)bb.height);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            rnd_ui_scissor_clear();
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            RndTexture tex = { (u32)(uintptr_t)cmd->renderData.image.imageData };
            rnd_ui_quad(bb.x, bb.y, bb.width, bb.height, 0, 0, 1, 1, tex,
                        1, 1, 1, 1);
        } break;
        default:
            break; /* overlay/custom unsupported for now */
        }
    }
}
