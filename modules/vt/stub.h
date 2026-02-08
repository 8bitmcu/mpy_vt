#ifndef STUB_H
#define STUB_H

#include "st.h" /* To get the Glyph and Rune types */
#include "win.h"     /* To ensure we match original X11 signatures */
#include <stdint.h>

typedef struct _vt_VT_obj_t {
  mp_obj_base_t base;
  st7789_ST7789_obj_t *display_drv;
  mp_obj_module_t *font_obj;
} vt_VT_obj_t;

extern vt_VT_obj_t *current_vt_obj;

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

#endif /* STUB_H */
