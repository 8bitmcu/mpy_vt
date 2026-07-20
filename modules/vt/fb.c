/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 Vincent (8bitmcu)
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

// mp_obj_dict_get() raises a real Python KeyError for a missing key
// (matching Python's d[key] semantics) rather than returning MP_OBJ_NULL --
// fine for keys a font is required to define (WIDTH/HEIGHT/REGULAR/etc,
// see xdrawline()'s "Setup Metrics"), but wrong for genuinely optional
// per-font blocks like UNICODE_FONT/WIDE_FONT, which most fonts don't
// define at all. Look up through the map directly for those instead --
// same underlying lookup mp_obj_dict_get() uses, just without the raise.
static mp_obj_t dict_get_optional(mp_obj_dict_t *dict, qstr key) {
  mp_map_elem_t *elem =
      mp_map_lookup(&dict->map, MP_OBJ_NEW_QSTR(key), MP_MAP_LOOKUP);
  return elem ? elem->value : MP_OBJ_NULL;
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

  return (uint16_t)((r565 >> 8) | (r565 << 8));
}

void draw_bar_ansi(const char *text, size_t len, int bar_type) {
  if (!current_vt_obj)
    return;

  // Determine which buffers to use
  Glyph *now = (bar_type == -1) ? top_line_now : bot_line_now;
  Glyph *last = (bar_type == -1) ? top_line_last : bot_line_last;

  TCursor save_cursor = term.c;
  int save_esc = term.esc;
  Line *original_lines = term.line;
  Line virtual_line = now;
  term.line = &virtual_line;

  // Reset buffer and cursor
  memset(now, 0, sizeof(Glyph) * 256);
  term.c.x = 0;
  term.c.y = 0;
  term.c.attr.mode = ATTR_NULL;
  term.c.attr.fg = defaultfg;
  term.c.attr.bg = defaultbg;
  term.esc = 0;

  // Decode UTF-8 the same way twrite() does (st.c) -- tputc() expects a
  // full, already-decoded Rune, not raw bytes. Feeding it one byte at a
  // time (the old code here) silently "worked" only as long as the
  // status bar's own text was pure ASCII, where every byte already is
  // its own valid codepoint; it breaks for real multi-byte UTF-8 like
  // the icon glyphs, splitting each 3-byte sequence into three bogus
  // single-byte "codepoints" that never match anything (or trigger
  // ATTR_WIDE via st_wcwidth()).
  for (size_t i = 0; i < len;) {
    Rune u;
    size_t charsize = utf8decode(text + i, &u, len - i);
    if (charsize == 0)
      break;
    tputc(u);
    i += charsize;
  }

  term.line = original_lines;
  term.c = save_cursor;
  (void)save_esc;

  // Dirty check: Compare results
  if (memcmp(now, last, term.col * sizeof(Glyph)) == 0) {
    return;
  }

  // Render to the specific hardware location (-1 or -2)
  xdrawline(now, 0, bar_type, term.col);

  // Sync back-buffer
  memcpy(last, now, term.col * sizeof(Glyph));
}

void xdrawline(Line ln, int _x1, int _y1, int _x2) {
  if (!current_vt_obj)
    return;

  vt_VT_obj_t *vt = current_vt_obj;
  st7789_ST7789_obj_t *display = vt->display_drv;

  // 1. Setup Metrics
  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(vt->font->globals);
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
  uint16_t y_start;
  if (_y1 == -1) {
    y_start = 0; // Top Bar
  } else if (_y1 == -2) {
    y_start = display->height - f_height; // Bottom Bar
  } else {
    y_start = (_y1 * f_height) + term.top_offset; // Terminal rows
  }
  uint16_t num_cols = _x2 - _x1;

  if (display->i2c_buffer) {
    uint32_t buf_idx = 0;
    uint16_t x_end = (x_start + (num_cols * f_width)) - 1;

    // DYNAMIC RIGHT MARGIN: If drawing the last column, stretch window to
    // screen edge
    if (_x2 == term.col && x_end < display->width - 1) {
      x_end = display->width - 1;
    }

    if (x_end < display->width) {
      set_window(display, x_start, y_start, x_end, y_start + f_height - 1);

      uint16_t window_width = x_end - x_start + 1;
      uint16_t text_rendered_width = num_cols * f_width;

      // Caches
      uint16_t fg_cache[num_cols];
      uint16_t bg_cache[num_cols];
      const uint8_t *font_ptr_cache[num_cols];
      // Per-column pixel width: f_width normally, WIDE_WIDTH (2x) for an
      // ATTR_WIDE cell drawing a double-width glyph, 0 for an ATTR_WDUMMY
      // cell (the second half of a wide character -- already drawn by
      // the WIDE cell before it, so it contributes no pixels of its
      // own). Total pixels across a row stay num_cols * f_width either
      // way, since a WIDE+WDUMMY pair sums to the same 2*f_width that
      // two normal columns would have used -- so none of the window/
      // padding math above needs to change, only these two loops.
      uint8_t col_width_cache[num_cols];

      // REGULAR/BOLD are normally required -- every font shipped by
      // fontconvert.py's default (non---icons) mode defines them. But
      // --icons mode produces an icons-ONLY module with neither, so an
      // icons-only font selected as the active font (see icontest.py's
      // earlier approach, before ICONS could live alongside a real font)
      // would otherwise hit the same "mp_obj_dict_get raises KeyError on
      // a missing key" crash WIDE_FONT/UNICODE_FONT already hit. Optional
      // here too: missing REGULAR just means ASCII text renders blank
      // for this font rather than crashing (see the ASCII-range branch
      // below, which now checks reg_buf.buf before using it).
      mp_buffer_info_t reg_buf = {0}, bold_buf = {0};
      mp_obj_t reg_obj = dict_get_optional(MP_OBJ_TO_PTR(vt->font->globals),
                                           MP_QSTR_REGULAR);
      if (reg_obj != MP_OBJ_NULL && reg_obj != mp_const_none) {
        mp_get_buffer_raise(reg_obj, &reg_buf, MP_BUFFER_READ);
      }
      mp_obj_t bold_obj = dict_get_optional(MP_OBJ_TO_PTR(vt->font->globals),
                                            MP_QSTR_BOLD);
      if (bold_obj != MP_OBJ_NULL && bold_obj != mp_const_none) {
        mp_get_buffer_raise(bold_obj, &bold_buf, MP_BUFFER_READ);
      }

      // Unicode font handling
      mp_buffer_info_t box_buf = {0};
      uint32_t box_codepoints[32];
      size_t box_count = 0;
      mp_obj_t box_font_obj = dict_get_optional(
          MP_OBJ_TO_PTR(vt->font->globals), qstr_from_str("UNICODE_FONT"));
      if (box_font_obj != MP_OBJ_NULL && box_font_obj != mp_const_none) {
        mp_get_buffer_raise(box_font_obj, &box_buf, MP_BUFFER_READ);
        mp_obj_t chars_obj = dict_get_optional(
            MP_OBJ_TO_PTR(vt->font->globals), qstr_from_str("UNICODE_CHARS"));
        if (chars_obj != MP_OBJ_NULL && chars_obj != mp_const_none) {
          box_count = (size_t)mp_obj_get_int(mp_obj_len(chars_obj));
          if (box_count > 32)
            box_count = 32;
          for (size_t j = 0; j < box_count; j++)
            box_codepoints[j] = (uint32_t)mp_obj_get_int(mp_obj_subscr(
                chars_obj, MP_OBJ_NEW_SMALL_INT(j), MP_OBJ_SENTINEL));
        }
      }

      // Wide (double-width) glyph handling, e.g. Chess Symbols (bundled
      // directly into Unifont's own WIDE_FONT block) or an icon font like
      // Siji (loaded independently via VT.set_icon_font() -- see fb.h).
      // Two supported shapes for WIDE_FONT, checked in this order:
      //   1. Contiguous range (WIDE_FIRST + WIDE_COUNT): O(1) index, no
      //      per-glyph table -- for a big set like Siji's ~400 icons,
      //      where a codepoint array would mean a many-hundred-entry
      //      stack allocation on every single redraw.
      //   2. Explicit small list (WIDE_CHARS): for a handful of
      //      non-contiguous codepoints, e.g. the dozen Chess Symbols.
      // Optional either way: no icon_font set, or a main font with
      // neither shape, just means ATTR_WIDE cells render narrow, same as
      // any other missing glyph.
      mp_buffer_info_t wide_buf = {0};
      uint32_t wide_codepoints[32];
      size_t wide_count = 0;
      uint32_t wide_first = 0;
      uint32_t wide_range_count = 0;
      uint8_t wide_width = f_width;
      uint8_t wide_height = f_height;
      uint8_t wide_row_bytes = wide;
      mp_obj_module_t *icon_font_mod = vt->icon_font ? vt->icon_font : vt->font;
      mp_obj_t wide_font_obj = dict_get_optional(
          MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_FONT"));
      if (wide_font_obj != MP_OBJ_NULL && wide_font_obj != mp_const_none) {
        mp_get_buffer_raise(wide_font_obj, &wide_buf, MP_BUFFER_READ);
        mp_obj_t wfirst_obj = dict_get_optional(
            MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_FIRST"));
        mp_obj_t wrange_count_obj = dict_get_optional(
            MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_COUNT"));
        if (wfirst_obj != MP_OBJ_NULL && wfirst_obj != mp_const_none &&
            wrange_count_obj != MP_OBJ_NULL && wrange_count_obj != mp_const_none) {
          wide_first = (uint32_t)mp_obj_get_int(wfirst_obj);
          wide_range_count = (uint32_t)mp_obj_get_int(wrange_count_obj);
        }
        mp_obj_t wchars_obj = dict_get_optional(
            MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_CHARS"));
        if (wchars_obj != MP_OBJ_NULL && wchars_obj != mp_const_none) {
          wide_count = (size_t)mp_obj_get_int(mp_obj_len(wchars_obj));
          if (wide_count > 32)
            wide_count = 32;
          for (size_t j = 0; j < wide_count; j++)
            wide_codepoints[j] = (uint32_t)mp_obj_get_int(mp_obj_subscr(
                wchars_obj, MP_OBJ_NEW_SMALL_INT(j), MP_OBJ_SENTINEL));
        }
        mp_obj_t wwidth_obj = dict_get_optional(
            MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_WIDTH"));
        if (wwidth_obj != MP_OBJ_NULL && wwidth_obj != mp_const_none) {
          wide_width = (uint8_t)mp_obj_get_int(wwidth_obj);
          wide_row_bytes = (wide_width + 7) / 8;
        }
        mp_obj_t wheight_obj = dict_get_optional(
            MP_OBJ_TO_PTR(icon_font_mod->globals), qstr_from_str("WIDE_HEIGHT"));
        if (wheight_obj != MP_OBJ_NULL && wheight_obj != mp_const_none) {
          wide_height = (uint8_t)mp_obj_get_int(wheight_obj);
        }
      }

      // Wide glyphs always occupy a fixed 2*f_width x f_height cell (that's
      // what ATTR_WIDE + the following ATTR_WDUMMY actually reserve in the
      // grid -- see col_width_cache below), regardless of the icon font's
      // own glyph size. wide_width/wide_height describe the actual bitmap
      // instead, centered within that cell: padded on all sides if
      // smaller, clipped evenly if larger. Lets a fixed-size icon font
      // (e.g. Siji) pair with any main font, not just one whose width
      // happens to be exactly half the icon's.
      int16_t wide_x_off = ((int16_t)(2 * f_width) - (int16_t)wide_width) / 2;
      int16_t wide_y_off = ((int16_t)f_height - (int16_t)wide_height) / 2;

      // Icon glyph handling (fontconvert.py --icons), e.g. Siji. One
      // contiguous codepoint range starting at ICON_FIRST, so unlike
      // UNICODE_FONT/WIDE_FONT this doesn't need a per-glyph codepoint
      // table -- just a base + count to compute the index directly.
      // Optional: most fonts don't define ICONS.
      mp_buffer_info_t icons_buf = {0};
      uint32_t icon_first = 0;
      uint32_t icon_count = 0;
      mp_obj_t icons_font_obj = dict_get_optional(
          MP_OBJ_TO_PTR(vt->font->globals), qstr_from_str("ICONS"));
      if (icons_font_obj != MP_OBJ_NULL && icons_font_obj != mp_const_none) {
        mp_get_buffer_raise(icons_font_obj, &icons_buf, MP_BUFFER_READ);
        mp_obj_t ifirst_obj = dict_get_optional(
            MP_OBJ_TO_PTR(vt->font->globals), qstr_from_str("ICON_FIRST"));
        if (ifirst_obj != MP_OBJ_NULL && ifirst_obj != mp_const_none) {
          icon_first = (uint32_t)mp_obj_get_int(ifirst_obj);
        }
        mp_obj_t icount_obj = dict_get_optional(
            MP_OBJ_TO_PTR(vt->font->globals), qstr_from_str("ICON_COUNT"));
        if (icount_obj != MP_OBJ_NULL && icount_obj != mp_const_none) {
          icon_count = (uint32_t)mp_obj_get_int(icount_obj);
        }
      }

      // Populate caching
      for (uint16_t i = 0; i < num_cols; i++) {
        int col_idx = _x1 + i;

        uint16_t fg = map_st_color(ln[col_idx].fg);
        uint16_t bg = map_st_color(ln[col_idx].bg);
        if (ln[col_idx].mode & ATTR_REVERSE) {
          fg_cache[i] = bg;
          bg_cache[i] = fg;
        } else {
          fg_cache[i] = fg;
          bg_cache[i] = bg;
        }

        if (ln[col_idx].mode & ATTR_WDUMMY) {
          // Second half of a wide character -- already drawn by the
          // WIDE cell before it. Contribute nothing here.
          font_ptr_cache[i] = NULL;
          col_width_cache[i] = 0;
          continue;
        }

        uint32_t char_val = ln[col_idx].u;

        if (ln[col_idx].mode & ATTR_WIDE) {
          // Always reserve the fixed 2 cells' worth of pixels -- see the
          // wide_x_off/wide_y_off comment above -- regardless of whether a
          // matching glyph is actually found below. Its ATTR_WDUMMY partner
          // unconditionally contributes 0 pixels, so if this cell fell back
          // to a narrower width instead, the row would render fewer pixels
          // than set_window() was told to expect and desync the SPI burst
          // for every row after it. A missing glyph renders as a blank
          // 2-wide cell (font_ptr_cache NULL, handled generically by the
          // render loop below), not a narrower one.
          col_width_cache[i] = 2 * f_width;
          font_ptr_cache[i] = NULL;
          if (wide_buf.buf) {
            if (wide_range_count > 0 && char_val >= wide_first &&
                char_val < wide_first + wide_range_count) {
              uint32_t index = char_val - wide_first;
              font_ptr_cache[i] = (uint8_t *)wide_buf.buf +
                                  index * (wide_height * wide_row_bytes);
            } else if (wide_count > 0) {
              for (size_t j = 0; j < wide_count; j++) {
                if (wide_codepoints[j] == char_val) {
                  font_ptr_cache[i] = (uint8_t *)wide_buf.buf +
                                      j * (wide_height * wide_row_bytes);
                  break;
                }
              }
            }
          }
          continue;
        }

        col_width_cache[i] = f_width;
        if (char_val >= first && char_val <= last && reg_buf.buf) {
          uint32_t offset = (char_val - first) * (f_height * wide);
          const uint8_t *base = ((ln[col_idx].mode & ATTR_BOLD) && bold_buf.buf)
                                    ? (uint8_t *)bold_buf.buf
                                    : (uint8_t *)reg_buf.buf;
          font_ptr_cache[i] = base + offset;
        } else if (icons_buf.buf && icon_count > 0 && char_val >= icon_first &&
                   char_val < icon_first + icon_count) {
          uint32_t index = char_val - icon_first;
          font_ptr_cache[i] =
              (uint8_t *)icons_buf.buf + index * (f_height * wide);
        } else if (box_buf.buf && box_count > 0) {
          font_ptr_cache[i] = NULL;
          for (size_t j = 0; j < box_count; j++) {
            if (box_codepoints[j] == char_val) {
              font_ptr_cache[i] =
                  (uint8_t *)box_buf.buf + j * (f_height * wide);
              break;
            }
          }
        } else {
          font_ptr_cache[i] = NULL;
        }
      }

      // Cursor Stats
      uint16_t cursor_fg = 0xFFFF;
      int cur_x = term.c.x;
      int cur_y = term.c.y;
      bool show_cursor = !(wmode & MODE_HIDE);
      int style = term.cursor_style;

      // Rendering loop
      for (uint8_t line_y = 0; line_y < f_height; line_y++) {
        bool is_last_row = (line_y >= f_height - 2);

        for (uint16_t i = 0; i < num_cols; i++) {
          uint8_t col_width = col_width_cache[i];
          if (col_width == 0) {
            // ATTR_WDUMMY -- second half of a wide character, already
            // drawn by the WIDE cell before it.
            continue;
          }

          const uint8_t *char_row_data = font_ptr_cache[i];
          int col_idx = _x1 + i;
          bool is_cursor_cell =
              (show_cursor && _y1 == cur_y && col_idx == cur_x);
          bool has_underline = (ln[col_idx].mode & ATTR_UNDERLINE);
          bool is_wide_cell = col_width > f_width;

          if (char_row_data && is_wide_cell) {
            // Centered draw: col_width is always the fixed 2*f_width
            // cell (see col_width_cache above), which may not match the
            // icon bitmap's own wide_width x wide_height. Pixels outside
            // the centered wide_x_off/wide_y_off box are plain padding;
            // pixels inside it read from the bitmap. A row/column
            // entirely outside the bitmap's vertical/horizontal extent
            // is indistinguishable from "no glyph" for cursor/underline
            // purposes -- there's no ink there to invert.
            int16_t icon_row = (int16_t)line_y - wide_y_off;
            bool row_in_range = (icon_row >= 0 && icon_row < wide_height);
            const uint8_t *row_ptr =
                row_in_range ? (char_row_data + (uint32_t)icon_row * wide_row_bytes)
                             : NULL;

            for (uint8_t pixel_col = 0; pixel_col < col_width; pixel_col++) {
              int16_t icon_col = (int16_t)pixel_col - wide_x_off;
              bool ink = false;
              if (row_ptr != NULL && icon_col >= 0 && icon_col < wide_width) {
                uint8_t byte = row_ptr[icon_col / 8];
                ink = !(byte & (0x80 >> (icon_col % 8)));
              }
              uint16_t final_color = ink ? fg_cache[i] : bg_cache[i];

              if (is_cursor_cell) {
                if (ink) {
                  switch (style) {
                  case 0:
                  case 1:
                  case 2:
                    final_color = ~final_color;
                    break;
                  case 3:
                  case 4:
                    if (is_last_row)
                      final_color = cursor_fg;
                    break;
                  default:
                    if (pixel_col < 2)
                      final_color = cursor_fg;
                    break;
                  }
                } else if ((style <= 2) || (style <= 4 && is_last_row) ||
                          (style > 4 && pixel_col < 2)) {
                  final_color = cursor_fg;
                }
              } else if (has_underline && is_last_row) {
                final_color = fg_cache[i];
              }

              display->i2c_buffer[buf_idx++] = final_color;
            }
          } else if (char_row_data) {
            uint8_t row_bytes = (col_width + 7) / 8;
            const uint8_t *data_ptr = char_row_data + (line_y * row_bytes);
            for (uint8_t b_idx = 0; b_idx < row_bytes; b_idx++) {
              uint8_t bits = data_ptr[b_idx];
              for (uint8_t b = 0; b < 8; b++) {
                uint8_t pixel_col = (b_idx * 8) + b;
                if (pixel_col >= col_width)
                  break;

                uint16_t final_color =
                    (bits & 0x80) ? bg_cache[i] : fg_cache[i];

                if (is_cursor_cell) {
                  switch (style) {
                  case 0:
                  case 1:
                  case 2:
                    final_color = ~final_color;
                    break;
                  case 3:
                  case 4:
                    if (is_last_row)
                      final_color = cursor_fg;
                    break;
                  default:
                    if (pixel_col < 2)
                      final_color = cursor_fg;
                    break;
                  }
                } else if (has_underline && is_last_row) {
                  final_color = fg_cache[i];
                }

                display->i2c_buffer[buf_idx++] = final_color;
                bits <<= 1;
              }
            }
          } else {
            for (uint8_t p = 0; p < col_width; p++) {
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

        // Pad out the right margin for this row inside the active window
        if (window_width > text_rendered_width) {
          uint16_t pad_bg = map_st_color(defaultbg);
          if (num_cols > 0) {
            pad_bg = map_st_color(ln[_x1 + num_cols - 1].bg);
            if (ln[_x1 + num_cols - 1].mode & ATTR_REVERSE) {
              pad_bg = map_st_color(ln[_x1 + num_cols - 1].fg);
            }
          }
          uint16_t pad_pixels = window_width - text_rendered_width;
          for (uint16_t p = 0; p < pad_pixels; p++) {
            display->i2c_buffer[buf_idx++] = pad_bg;
          }
        }
      }

      // SPI Burst for the core row line
      mp_hal_pin_write(display->dc, 1);
      mp_hal_pin_write(display->cs, 0);
      write_spi(display->spi_obj, (uint8_t *)display->i2c_buffer, buf_idx * 2);
      mp_hal_pin_write(display->cs, 1);
    }
  }
}

// Fills the pixel band below the last text row, when the font's height
// doesn't evenly divide the usable area (term.top_offset + term.row *
// f_height falls short of the physical screen height). xdrawline()
// already handles the analogous right-margin remainder by stretching
// and padding each row's own draw, since every row goes through it
// anyway -- but this band isn't a text row at all, so it's never
// otherwise touched by normal drawing and keeps showing whatever was on
// screen before the current font/layout was set. Call once after a
// layout change (tnew()/tresize()); it's static until the next one.
void fill_bottom_margin(void) {
  if (!current_vt_obj)
    return;

  vt_VT_obj_t *vt = current_vt_obj;
  st7789_ST7789_obj_t *display = vt->display_drv;
  if (!display || !display->i2c_buffer)
    return;

  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(vt->font->globals);
  const uint8_t f_height =
      mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));

  int content_bottom = term.top_offset + (term.row * f_height);
  if (content_bottom >= display->height)
    return;

  uint16_t leftover = display->height - content_bottom;
  uint16_t bg = map_st_color(defaultbg);
  uint32_t n_pixels = (uint32_t)display->width * leftover;

  for (uint32_t i = 0; i < n_pixels; i++) {
    display->i2c_buffer[i] = bg;
  }

  set_window(display, 0, content_bottom, display->width - 1,
            display->height - 1);
  mp_hal_pin_write(display->dc, 1);
  mp_hal_pin_write(display->cs, 0);
  write_spi(display->spi_obj, (uint8_t *)display->i2c_buffer, n_pixels * 2);
  mp_hal_pin_write(display->cs, 1);
}

void repaint_bars(void) {
  if (!current_vt_obj || term.col <= 0)
    return;
  xdrawline(top_line_now, 0, -1, term.col);
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

void xsetmode(int set, unsigned int val) {
  if (set)
    wmode |= val;
  else
    wmode &= ~val;
}

/* Variables: st expects these to exist as defaults */
int allowaltscreen = 1;         /* Allow switching to the 'alt' buffer */
char *vtiden = "\033[?6c";      /* Terminal identification string */
wchar_t *worddelimiters = L" "; /* Characters that break selection */
unsigned int defaultcs = 256;   /* Default cursor color index */

/* Functions: Empty stubs to satisfy the linker */
void xsetpointermotion(int val) {}
int xsetcursor(int cursor) { return 1; }
void xbell(void) {}

int xgetcolor(int x, unsigned char *r, unsigned char *g, unsigned char *b) {
  return 1; /* Return failure/not found for now */
}
