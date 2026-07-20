/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 Vincent (8bitmcu)
 * * Based on the st (Suckless Terminal) engine.
 * Original code (c) st engineers.
 * License: MIT
 */

#ifndef FB_H
#define FB_H

#include "st.h" /* To get the Glyph and Rune types */
#include "st7789.h"
#include "win.h" /* To ensure we match original X11 signatures */
#include <stdint.h>

typedef struct _vt_VT_obj_t {
  mp_obj_base_t base;
  st7789_ST7789_obj_t *display_drv;
  mp_obj_module_t *font;
  // Optional, independent of `font`: a supplemental double-width glyph
  // source (WIDE_FONT/WIDE_FIRST/WIDE_COUNT/WIDE_WIDTH -- see fb.c's
  // xdrawline()) for ATTR_WIDE codepoints, e.g. an icon font like Siji
  // paired with whichever text font is active. NULL if unset -- ATTR_WIDE
  // cells then just render narrow, same as any other missing glyph.
  // Set via VT.set_icon_font(), independent of the main font/layout.
  mp_obj_module_t *icon_font;
} vt_VT_obj_t;

// Allocate a persistent line for the status bar
static Glyph top_line_now[256];
static Glyph top_line_last[256];

static Glyph bot_line_now[256];  // Buffer for bottom bar
static Glyph bot_line_last[256]; // Back-buffer for bottom bar

extern vt_VT_obj_t *current_vt_obj;

static unsigned int wmode = MODE_VISIBLE;

/* Window/UI operations */
void xsettitle(char *p);
void xseticontitle(char *p);
void xsetsel(char *p);
void xclipcopy(void);

/* Configuration/Settings */
extern int allowwindowops;

/* Color and Graphics */
int xsetcolorname(int n, const char *s);
void xloadcols(void);

/* Drawing/Cursor operations */
void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og);
void xdrawglyph(Glyph g, int x, int y);

/* Mode and Input stubs */
void xsetpointermotion(int val);
int xsetcursor(int cursor);
void xbell(void);
int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b);

/* Global config variables used by st_term.c */
extern int allowaltscreen;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern unsigned int defaultcs;

void draw_bar_ansi(const char *text, size_t len, int bar_type);
void repaint_bars(void);
void fill_bottom_margin(void);

#endif /* FB_H */
