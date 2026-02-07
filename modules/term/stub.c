#include "st_term.h"
#include "win.h"
#include <stdint.h>

extern struct _term_Terminal_obj_t *current_term_obj;

// TODO: stub
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask) {
  // We ignore the sigmask and convert timespec to timeval
  struct timeval tv = {.tv_sec = timeout->tv_sec,
                       .tv_usec = timeout->tv_nsec / 1000};
  return select(nfds, readfds, writefds, exceptfds, &tv);
}

uint16_t map_st_color(int color_idx) {
  switch (color_idx) {
  case 0:
    return 0x0000; // Black
  case 1:
    return 0xF800; // Red
  case 2:
    return 0x07E0; // Green
  case 3:
    return 0xFFE0; // Yellow
  case 4:
    return 0x001F; // Blue
  case 7:
    return 0xFFFF; // White (Standard)
  case 256:
    return 0x0000; // Default BG
  case 257:
    return 0xFFFF; // Default FG
  default:
    return 0xFFFF;
  }
}

// TODO: stub
void xdrawline(Line line, int x1, int y1, int x2) {
  if (!current_term_obj)
    return;

  term_Terminal_obj_t *term = current_term_obj;

  // Pixel coordinates
  int char_w = 8;  // Adjust to your font width
  int char_h = 14; // Adjust to your font height

  // Buffer for the string chunk we are building
  // Max length is the line width (x2 - x1) + null terminator
  uint8_t buf[256];
  int buf_len = 0;

  // Start of the current chunk
  int chunk_start_x = x1;

  // Helper to flush the buffer
  void flush_chunk(int start_idx, uint32_t fg, uint32_t bg) {
    if (buf_len == 0)
      return;

    st7789_ST7789_write_raw(term->display_drv, term->font_obj, buf, buf_len,
                            start_idx * char_w, // X Pixel
                            y1 * char_h,        // Y Pixel
                            map_st_color(fg),   // FG (mapped to 565)
                            map_st_color(bg),   // BG (mapped to 565)
                            0, 0, NULL,         // No background image
                            false               // Fill background
    );
    buf_len = 0;
  }

  // Loop through characters
  for (int i = x1; i < x2; i++) {
    // If color changes, flush what we have so far
    if (i > x1 &&
        (line[i].fg != line[i - 1].fg || line[i].bg != line[i - 1].bg)) {
      flush_chunk(chunk_start_x, line[i - 1].fg, line[i - 1].bg);
      chunk_start_x = i;
    }

    // Add char to buffer
    buf[buf_len++] = (uint8_t)line[i].u;
  }

  // Flush the remaining characters
  flush_chunk(chunk_start_x, line[x2 - 1].fg, line[x2 - 1].bg);
}
// TODO: stub
int xstartdraw(void) {
  // Prepare your display for a frame update
  return 1;
}

// TODO: stub
void xfinishdraw(void) {
  // Push your buffer to the hardware
}

// TODO: stub
void xximspot(int x, int y) {
  // Used for Input Method Editors (IME); safe to leave empty
}

// Window/UI operations that don't apply to an LCD
void xsettitle(char *p) {}
void xseticontitle(char *p) {}
void xsetsel(char *p) {}
void xclipcopy(void) {}

// Configuration/Settings
int allowwindowops = 0;

// Color and Graphics
int xsetcolorname(int n, const char *s) { return 1; }
void xloadcols(void) {}

// If st_term calls these for the cursor:
void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og) {}
void xdrawglyph(Glyph g, int x, int y) {}

/* Variables: st expects these to exist as defaults */
int allowaltscreen = 1;         /* Allow switching to the 'alt' buffer */
char *vtiden = "\033[?6c";      /* Terminal identification string */
wchar_t *worddelimiters = L" "; /* Characters that break selection */
unsigned int defaultcs = 256;   /* Default cursor color index */

/* Functions: Empty stubs to satisfy the linker */
void xsetmode(int mode, unsigned int val) {}
void xsetpointermotion(int val) {}
int xsetcursor(int cursor) { return 1; }
void xbell(void) {}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b) {
  return 1; /* Return failure/not found for now */
}
