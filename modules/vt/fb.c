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

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask) {
  // We ignore the sigmask and convert timespec to timeval
  struct timeval tv = {.tv_sec = timeout->tv_sec,
                       .tv_usec = timeout->tv_nsec / 1000};
  return select(nfds, readfds, writefds, exceptfds, &tv);
}

uint16_t map_st_color(int number) {
  uint16_t r565;

  // 1. Standard/Bright (0-15)
  if (number < 16) {
    static const uint16_t ansi_16[16] = {
        0x0000, 0x8000, 0x0400, 0x8400, 0x0010, 0x8010, 0x0410, 0xBDF7,
        0x8410, 0xF800, 0x07E0, 0xFFE0, 0x001F, 0xF81F, 0x07FF, 0xFFFF};
    r565 = ansi_16[number];
  }
  // 2. Color Cube (16-231)
  else if (number <= 231) {
    int idx = number - 16;
    static const uint8_t levels[] = {0, 95, 135, 175, 215, 255};

    uint8_t r = levels[(idx / 36) % 6];
    uint8_t g = levels[(idx / 6) % 6];
    uint8_t b = levels[idx % 6];

    r565 = ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) |
           (uint16_t)(b >> 3);
  }
  // 3. Grayscale (232-255)
  else if (number <= 255) {
    uint8_t g_val = (number - 232) * 10 + 8;
    r565 = ((uint16_t)(g_val >> 3) << 11) | ((uint16_t)(g_val >> 2) << 5) |
           (uint16_t)(g_val >> 3);
  } else {
    if (number == 256)
      r565 = 0x0000; // BG
    else if (number == 257)
      r565 = 0xFFFF; // FG
    else
      r565 = 0xFFFF;
  }

  // FINAL STEP: Perform the byte swap here so the loop doesn't have to
  return (uint16_t)((r565 >> 8) | (r565 << 8));
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
        uint16_t fg = map_st_color(ln[col_idx].fg);
        uint16_t bg = map_st_color(ln[col_idx].bg);
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

      // ... (Inside xdrawline, after caching fonts and colors) ...

      // Cursor Stats
      uint16_t cursor_fg = 0xFFFF; // White or custom cursor color
      int cur_x = term.c.x;
      int cur_y = term.c.y;
      bool show_cursor = !(term.mode & MODE_HIDE);

      // Get current style from terminal state (Default to 6 if not set)
      // 0,1,2 = Block | 3,4 = Underline | 5,6 = Beam
      int style = term.cursor_style;

      // rendering loop
      for (uint8_t line_y = 0; line_y < f_height; line_y++) {
        bool is_last_row = (line_y >= f_height - 2); // 2-pixel thick underline

        for (uint16_t i = 0; i < num_cols; i++) {
          const uint8_t *char_row_data = font_ptr_cache[i];
          int col_idx = _x1 + i;
          bool is_cursor_cell =
              (show_cursor && _y1 == cur_y && col_idx == cur_x);
          bool has_underline = (ln[col_idx].mode & ATTR_UNDERLINE);

          if (char_row_data) {
            const uint8_t *data_ptr = char_row_data + (line_y * wide);
            for (uint8_t b_idx = 0; b_idx < wide; b_idx++) {
              uint8_t bits = data_ptr[b_idx];
              for (uint8_t b = 0; b < 8; b++) {
                uint8_t pixel_col = (b_idx * 8) + b;
                if (pixel_col >= f_width)
                  break; // Font width safety

                uint16_t final_color =
                    (bits & 0x80) ? bg_cache[i] : fg_cache[i];

                // --- CURSOR OVERLAY LOGIC ---
                if (is_cursor_cell) {
                  switch (style) {
                  case 0:
                  case 1:
                  case 2: // BLOCK
                    // Cursor Color
                    final_color = ~final_color;
                    break;
                  case 3:
                  case 4: // UNDERLINE
                    if (is_last_row)
                      final_color = cursor_fg;
                    break;
                  case 5:
                  case 6: // BEAM (Vertical Bar)
                  default:
                    if (pixel_col < 2)
                      final_color = cursor_fg;
                    break;
                  }
                }
                // --- ATTR_UNDERLINE LOGIC (Normal text) ---
                else if (has_underline && is_last_row) {
                  final_color = fg_cache[i];
                }

                display->i2c_buffer[buf_idx++] = final_color;
                bits <<= 1;
              }
            }
          } else {
            // Handle Empty Space Cursor
            for (uint8_t p = 0; p < f_width; p++) {
              uint16_t final_color = bg_cache[i];
              if (is_cursor_cell) {
                if ((style <= 2) || (style <= 4 && is_last_row) ||
                    (style > 4 && p < 2)) {
                  final_color = cursor_fg;
                }
              } else if (has_underline && is_last_row) {
                final_color = fg_cache[i];
              }
              display->i2c_buffer[buf_idx++] = final_color;
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
