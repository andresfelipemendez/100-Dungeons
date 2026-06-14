#ifndef THEME_H
#define THEME_H

/* The one place the editor's colour palette lives. The engine UI (ui.c) and the
   project (game.c: clear colour, CSG grid texture) both include this so the
   whole scene stays on one palette. Dark slate base + teal accent.

   Colours are bare comma-lists so they drop straight into a brace initialiser:
       (Clay_Color){ THEME_PANEL }      -> { 31, 34, 41, 242 }
       GRID_RGBA(THEME_GRID_LINE)       -> GRID_RGBA(0x2A, 0x2F, 0x38)
   RGBA components are 0-255; THEME_CLEAR_* are the same base as floats. */

/* surfaces */
#define THEME_PANEL       31, 34, 41, 242   /* left/right panels */
#define THEME_BTN         48, 53, 63, 255   /* button idle */
#define THEME_BTN_HOVER   52, 138, 134, 255 /* button hover: teal accent */
#define THEME_ROW         26, 29, 35, 255   /* list row idle */
#define THEME_ROW_HOVER   44, 49, 59, 255   /* list row hover */
#define THEME_ROW_SEL     44, 110, 106, 255 /* list row selected: teal */

/* text */
#define THEME_TEXT        228, 231, 237, 255
#define THEME_TEXT_DIM    138, 146, 158, 255

/* CSG triplanar grid texture (3-component) */
#define THEME_GRID_LINE   0x2A, 0x2F, 0x38
#define THEME_GRID_CELL   0xC6, 0xCC, 0xD4

/* move-gizmo arrow colours by axis (X/Y/Z), 3-component */
#define THEME_AXIS_X      0xCE, 0x52, 0x4A   /* red   */
#define THEME_AXIS_Y      0x5E, 0xBA, 0x56   /* green */
#define THEME_AXIS_Z      0x4E, 0x80, 0xDA   /* blue  */

/* pack an R,G,B triple into a little-endian RGBA8 u32 (A = 255). The THEME_U32
   wrapper forces the triple to expand before the 3-arg packer counts args:
       THEME_U32(THEME_AXIS_X)  ->  THEME_PACK(0xCE, 0x52, 0x4A) */
#define THEME_PACK(r, g, b) \
    (0xFF000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define THEME_U32(triple)   THEME_PACK(triple)

/* viewport clear -- same slate as the panels, one notch darker, as floats */
#define THEME_CLEAR_R     (23.0f / 255.0f)
#define THEME_CLEAR_G     (25.0f / 255.0f)
#define THEME_CLEAR_B     (29.0f / 255.0f)

/* panel resize grips, as 0-1 R,G,B floats for the 2D quad path */
#define THEME_GRIP_IDLE   0.22f, 0.24f, 0.29f   /* subtle slate */
#define THEME_GRIP_HOT    0.20f, 0.54f, 0.53f   /* teal accent */

#endif /* THEME_H */
