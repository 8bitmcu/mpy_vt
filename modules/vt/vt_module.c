/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * * Based on the st (Suckless Terminal) engine.
 * Original code (c) st engineers.
 * License: MIT
 */

#include "py/mphal.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"

#include "fb.h"
#include "mpfile.h"
#include "st.h"
#include "st7789.h"

// Object Structure ---

extern const mp_obj_type_t st7789_ST7789_type;
vt_VT_obj_t *current_vt_obj = NULL;

// Forward declaration
const mp_obj_type_t vt_VT_type;

static void vt_internal_write(const char *buf, size_t size) {
  for (size_t i = 0; i < size; i++) {
    tputc((uchar)buf[i]);
  }
}

static mp_uint_t vt_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                         int *errcode) {
  *errcode = MP_EAGAIN;
  return MP_STREAM_ERROR;
}

static mp_uint_t vt_write(mp_obj_t self_in, const void *buf, mp_uint_t size,
                          int *errcode) {
  vt_internal_write((const char *)buf, size);
  return size;
}

static mp_uint_t vt_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                          int *errcode) {
  if (request == MP_STREAM_POLL) {
    // We are always writable, but never readable (for now)
    return MP_STREAM_POLL_WR;
  } else if (request == MP_STREAM_CLOSE) {
    return 0;
  }

  *errcode = MP_EINVAL;
  return MP_STREAM_ERROR;
}

static mp_obj_t vt_VT_write(mp_obj_t self_in, mp_obj_t arg) {
  size_t len;
  const char *data = mp_obj_str_get_data(arg, &len);
  vt_internal_write(data, len);
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(vt_VT_write_obj, vt_VT_write);

static mp_obj_t vt_VT_draw(mp_obj_t self_in) {
  draw();
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(vt_VT_draw_obj, vt_VT_draw);

static mp_obj_t vt_vt_top_offset(mp_obj_t self_in, mp_obj_t offset_obj) {
  vt_VT_obj_t *self = MP_OBJ_TO_PTR(self_in);
  int offset = mp_obj_get_int(offset_obj);

  // Update the offset in the terminal engine
  term.top_offset = offset;

  // Mark lines dirty so they move to the new position on next draw()
  for (int i = 0; i < term.row; i++) {
    term.dirty[i] = 1;
  }

  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(vt_vt_top_offset_obj, vt_vt_top_offset);

// Top Bar Bridge
static mp_obj_t vt_vt_top_bar(mp_obj_t self_in, mp_obj_t str_obj) {
  size_t len;
  const char *txt = mp_obj_str_get_data(str_obj, &len);
  draw_bar_ansi(txt, len, -1);
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(vt_vt_top_bar_obj, vt_vt_top_bar);

// Bottom Bar Bridge
static mp_obj_t vt_vt_bottom_bar(mp_obj_t self_in, mp_obj_t str_obj) {
  size_t len;
  const char *txt = mp_obj_str_get_data(str_obj, &len);
  draw_bar_ansi(txt, len, -2);
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(vt_vt_bottom_bar_obj, vt_vt_bottom_bar);

// Invalidate the Top Bar cache
static mp_obj_t vt_vt_top_bar_invalidate(mp_obj_t self_in) {
  memset(top_line_last, 0, sizeof(top_line_last));
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(vt_vt_top_bar_invalidate_obj,
                          vt_vt_top_bar_invalidate);

// Invalidate the Bottom Bar cache
static mp_obj_t vt_vt_bottom_bar_invalidate(mp_obj_t self_in) {
  memset(bot_line_last, 0, sizeof(bot_line_last));
  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(vt_vt_bottom_bar_invalidate_obj,
                          vt_vt_bottom_bar_invalidate);

static const mp_stream_p_t vt_stream_p = {
    .read = vt_read,
    .write = vt_write,
    .ioctl = vt_ioctl,
    .is_text = true,
};

//  Class Definition ---

// Constructor: vt.VT()
static mp_obj_t vt_VT_make_new(const mp_obj_type_t *type, size_t n_args,
                               size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 4, 5, false);

  vt_VT_obj_t *self = m_new_obj(vt_VT_obj_t);
  self->base.type = &vt_VT_type;

  if (!mp_obj_is_type(args[0], &st7789_ST7789_type)) {
    mp_raise_TypeError(MP_ERROR_TEXT("Arg 1 must be ST7789 object"));
  }
  self->display_drv = (st7789_ST7789_obj_t *)MP_OBJ_TO_PTR(args[0]);

  int cols = mp_obj_get_int(args[1]);
  int rows = mp_obj_get_int(args[2]);

  self->font_regular = (mp_obj_module_t *)MP_OBJ_TO_PTR(args[3]);
  if (n_args > 4) {
    self->font_bold = (mp_obj_module_t *)MP_OBJ_TO_PTR(args[4]);
  } else {
    self->font_bold = self->font_regular;
  }

  tnew(cols, rows);

  current_vt_obj = self;

  return MP_OBJ_FROM_PTR(self);
}

// Locals Dict (Methods attached to the object)
// Even if empty, it's good practice to define it to avoid segfaults on dir(obj)
static const mp_rom_map_elem_t vtinal_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&vt_VT_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&vt_VT_draw_obj)},
    {MP_ROM_QSTR(MP_QSTR_top_offset), MP_ROM_PTR(&vt_vt_top_offset_obj)},
    {MP_ROM_QSTR(MP_QSTR_top_bar), MP_ROM_PTR(&vt_vt_top_bar_obj)},
    {MP_ROM_QSTR(MP_QSTR_bottom_bar), MP_ROM_PTR(&vt_vt_bottom_bar_obj)},
    {MP_ROM_QSTR(MP_QSTR_top_bar_invalidate),
     MP_ROM_PTR(&vt_vt_top_bar_invalidate_obj)},
    {MP_ROM_QSTR(MP_QSTR_bottom_bar_invalidate),
     MP_ROM_PTR(&vt_vt_bottom_bar_invalidate_obj)},
};

static MP_DEFINE_CONST_DICT(vt_VT_locals_dict, vtinal_locals_dict_table);

// Type Definition
MP_DEFINE_CONST_OBJ_TYPE(vt_VT_type, MP_QSTR_VT, MP_TYPE_FLAG_NONE, make_new,
                         vt_VT_make_new, protocol, &vt_stream_p, locals_dict,
                         &vt_VT_locals_dict);

//  Module Definition

static const mp_rom_map_elem_t vt_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vt)},
    {MP_ROM_QSTR(MP_QSTR_VT), MP_ROM_PTR(&vt_VT_type)},
};

static MP_DEFINE_CONST_DICT(vt_module_globals, vt_module_globals_table);

const mp_obj_module_t vt_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&vt_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vt, vt_module);
