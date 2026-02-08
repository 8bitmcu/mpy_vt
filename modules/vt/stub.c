#include "stub.h"
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

void xdrawline(Line ln, int _x1, int _y1, int _x2) {
  if (!current_vt_obj)
    return;

  vt_VT_obj_t *vt = current_vt_obj;
  st7789_ST7789_obj_t *display = vt->display_drv;

  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(vt->font_obj->globals);
  const uint8_t f_width =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTH)));
  const uint8_t f_height =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));
  const uint8_t first =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_FIRST)));
  const uint8_t last =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_LAST)));

  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_FONT)),
                      &bufinfo, MP_BUFFER_READ);
  const uint8_t *font_data = bufinfo.buf;

  uint8_t wide = (f_width + 7) / 8;

  uint16_t x_start = _x1 * f_width;
  uint16_t y_start = _y1 * f_height;
  uint16_t num_cols = _x2 - _x1;

  // TODO if no buffer, create one
  if (display->i2c_buffer) {
    uint32_t buf_idx = 0;
    uint16_t x_end = (x_start + (num_cols * f_width)) - 1;

    if (x_end < display->width) {
      set_window(display, x_start, y_start, x_end, y_start + f_height - 1);

      // pre-cache colors
      uint16_t fg_cache[num_cols];
      uint16_t bg_cache[num_cols];
      for (int i = 0; i < num_cols; i++) {
        fg_cache[i] = map_st_color(ln[_x1 + i].fg);
        bg_cache[i] = map_st_color(ln[_x1 + i].bg);
      }

      // rendering loop
      for (uint8_t line_y = 0; line_y < f_height; line_y++) {
        for (uint16_t i = 0; i < num_cols; i++) {
          uint32_t char_val = ln[_x1 + i].u;

          if (char_val >= first && char_val <= last) {
            uint32_t chr_offset = (char_val - first) * (f_height * wide);

            for (uint8_t b_idx = 0; b_idx < wide; b_idx++) {
              uint8_t chr_data =
                  font_data[chr_offset + (line_y * wide) + b_idx];

              // Hot Path: pixel generation
              for (uint8_t b = 0; b < 8; b++) {
                display->i2c_buffer[buf_idx++] =
                    (chr_data & 0x80) ? bg_cache[i] : fg_cache[i];
                chr_data <<= 1;
              }
            }
          } else {
            // fallback for missing characters
            for (uint8_t p = 0; p < f_width; p++) {
              display->i2c_buffer[buf_idx++] = bg_cache[i];
            }
          }
        }
      }

      // one single SPI burst for the whole line segment
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
