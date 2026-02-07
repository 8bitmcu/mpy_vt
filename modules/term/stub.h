#ifndef STUB_H
#define STUB_H

#include <stdint.h>
#include "st_term.h"    /* To get the Glyph and Rune types */
#include "win.h"   /* To ensure we match original X11 signatures */

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
