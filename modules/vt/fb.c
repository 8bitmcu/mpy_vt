/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * * Based on the st (Suckless Terminal) engine.
 * Original code (c) st engineers.
 * License: MIT
 */

#include "fb.h"
#include "st.h"
#include "win.h"
#include <stdint.h>

#define _swap_bytes(val) ((((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00))

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
    return 0x8000; // Red (Dim)
  case 2:
    return 0x0400; // Green (Dim)
  case 3:
    return 0x8400; // Yellow (Dim)
  case 4:
    return 0x0010; // Blue (Dim)
  case 5:
    return 0x8010; // Magenta (Dim)
  case 6:
    return 0x0410; // Cyan (Dim)
  case 7:
    return 0xBDF7; // Light Gray
  case 8:
    return 0x8410; // Gray (Bright Black)
  case 9:
    return 0xF800; // Bright Red
  case 10:
    return 0x07E0; // Bright Green
  case 11:
    return 0xFFE0; // Bright Yellow
  case 12:
    return 0x001F; // Bright Blue
  case 13:
    return 0xF81F; // Bright Magenta
  case 14:
    return 0x07FF; // Bright Cyan
  case 15:
    return 0xFFFF; // White (Bright Gray)

  // --- Default Fallbacks ---
  case 256:
    return 0x0000; // Default BG
  case 257:
    return 0xFFFF; // Default FG

  default:
    return 0xFFFF; // Fallback to White
  }
}

void xdrawline(Line ln, int _x1, int _y1, int _x2) {
  if (!current_vt_obj)
    return;

  vt_VT_obj_t *vt = current_vt_obj;
  st7789_ST7789_obj_t *display = vt->display_drv;

  // 1. Setup Metrics
  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(vt->font_regular->globals);
  const uint8_t f_width =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTH)));
  const uint8_t f_height =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));
  const uint8_t first =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_FIRST)));
  const uint8_t last =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_LAST)));
  uint8_t wide = (f_width + 7) / 8;

  uint16_t x_start = _x1 * f_width;
  uint16_t y_start = _y1 * f_height;
  uint16_t num_cols = _x2 - _x1;

  // TODO if no buffer
  if (display->i2c_buffer) {
    uint32_t buf_idx = 0;
    uint16_t x_end = (x_start + (num_cols * f_width)) - 1;

    if (x_end < display->width) {
      set_window(display, x_start, y_start, x_end, y_start + f_height - 1);

      // Caches
      uint16_t fg_cache[num_cols];
      uint16_t bg_cache[num_cols];
      const uint8_t *font_ptr_cache[num_cols];

      mp_buffer_info_t reg_buf, bold_buf;
      mp_get_buffer_raise(
          mp_obj_dict_get(MP_OBJ_TO_PTR(vt->font_regular->globals),
                          MP_OBJ_NEW_QSTR(MP_QSTR_FONT)),
          &reg_buf, MP_BUFFER_READ);
      mp_get_buffer_raise(mp_obj_dict_get(MP_OBJ_TO_PTR(vt->font_bold->globals),
                                          MP_OBJ_NEW_QSTR(MP_QSTR_FONT)),
                          &bold_buf, MP_BUFFER_READ);

      // caching
      for (uint16_t i = 0; i < num_cols; i++) {
        int col_idx = _x1 + i;

        // Color & Invert
        uint16_t fg = _swap_bytes(map_st_color(ln[col_idx].fg));
        uint16_t bg = _swap_bytes(map_st_color(ln[col_idx].bg));
        if (ln[col_idx].mode & ATTR_REVERSE) {
          fg_cache[i] = bg;
          bg_cache[i] = fg;
        } else {
          fg_cache[i] = fg;
          bg_cache[i] = bg;
        }

        // Font & Offset
        uint32_t char_val = ln[col_idx].u;
        if (char_val >= first && char_val <= last) {
          uint32_t offset = (char_val - first) * (f_height * wide);
          const uint8_t *base = (ln[col_idx].mode & ATTR_BOLD)
                                    ? (uint8_t *)bold_buf.buf
                                    : (uint8_t *)reg_buf.buf;
          font_ptr_cache[i] = base + offset;
        } else {
          font_ptr_cache[i] = NULL;
        }
      }

      // Cursor Stats
      const uint8_t beam_width = 2;
      uint16_t cursor_col = 0xFFFF;
      int cur_x = term.c.x;
      int cur_y = term.c.y;
      bool show_cursor = !(term.mode & MODE_HIDE);

      // rendering loop
      for (uint8_t line_y = 0; line_y < f_height; line_y++) {
        // Optimization: Is this specific scanline part of the underline?
        // Usually, the last or second-to-last row looks best.
        bool is_underline_row = (line_y == f_height - 1);

        for (uint16_t i = 0; i < num_cols; i++) {
          const uint8_t *char_row_data = font_ptr_cache[i];
          bool is_cursor_cell =
              (show_cursor && _y1 == cur_y && (_x1 + i) == cur_x);

          // Check if THIS character has the underline attribute
          bool has_underline = (ln[_x1 + i].mode & ATTR_UNDERLINE);

          if (char_row_data) {
            const uint8_t *data_ptr = char_row_data + (line_y * wide);
            for (uint8_t b_idx = 0; b_idx < wide; b_idx++) {
              uint8_t bits = data_ptr[b_idx];
              for (uint8_t b = 0; b < 8; b++) {

                // 1. Priority: Beam Cursor
                if (is_cursor_cell && b_idx == 0 && b < beam_width) {
                  display->i2c_buffer[buf_idx++] = cursor_col;
                }
                // 2. Priority: Underline Attribute
                else if (has_underline && is_underline_row) {
                  display->i2c_buffer[buf_idx++] = fg_cache[i];
                }
                // 3. Normal Pixel
                else {
                  display->i2c_buffer[buf_idx++] =
                      (bits & 0x80) ? bg_cache[i] : fg_cache[i];
                }
                bits <<= 1;
              }
            }
          } else {
            // Handle Empty Space (Still underline empty spaces if attribute is
            // set!)
            for (uint8_t p = 0; p < f_width; p++) {
              if (is_cursor_cell && p < beam_width) {
                display->i2c_buffer[buf_idx++] = cursor_col;
              } else if (has_underline && is_underline_row) {
                display->i2c_buffer[buf_idx++] = fg_cache[i];
              } else {
                display->i2c_buffer[buf_idx++] = bg_cache[i];
              }
            }
          }
        }
      }
      // SPI Burst
      mp_hal_pin_write(display->dc, 1);
      mp_hal_pin_write(display->cs, 0);
      write_spi(display->spi_obj, (uint8_t *)display->i2c_buffer, buf_idx * 2);
      mp_hal_pin_write(display->cs, 1);
    }
  }
}

int xstartdraw(void) {
  // Prepare a display for a frame update
  return 1;
}

void xfinishdraw(void) {
  // Push a buffer to the hardware
}

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
