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

#include "mpfile.h"
#include "st.h"
#include "st7789.h"
#include "fb.h"

// Object Structure ---

extern const mp_obj_type_t st7789_ST7789_type;
vt_VT_obj_t *current_vt_obj = NULL;

// Forward declaration
const mp_obj_type_t vt_VT_type;

// READ: Called when MicroPython wants input (e.g. `input()`)
static mp_uint_t vt_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                         int *errcode) {
  vt_VT_obj_t *self = MP_OBJ_TO_PTR(self_in);

  (void)self;

  // TODO: Return 0 if no data, or bytes read if data exists.
  // Use *errcode = MP_EAGAIN and return MP_STREAM_ERROR for non-blocking wait.
  return 0;
}

static mp_uint_t vt_write(mp_obj_t self_in, const void *buf, mp_uint_t size,
                          int *errcode) {
  vt_VT_obj_t *self = MP_OBJ_TO_PTR(self_in);

  (void)self;
  printf("[vt] %.*s", (int)size, (char *)buf);

  return size;
}

static mp_obj_t vt_VT_write(mp_obj_t self_in, mp_obj_t arg) {
  size_t len;
  const char *data = mp_obj_str_get_data(arg, &len);

  for (size_t i = 0; i < len; i++) {
    // tputc is the heart of st.c
    // It parses the byte and updates the internal grid
    tputc((uchar)data[i]);
  }

  draw();
  return mp_const_none;
}

// Define the MicroPython function object
MP_DEFINE_CONST_FUN_OBJ_2(vt_VT_write_obj, vt_VT_write);

// IOCTL: Called for polling and configuration
static mp_uint_t vt_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                          int *errcode) {
  vt_VT_obj_t *self = MP_OBJ_TO_PTR(self_in);
  mp_uint_t ret;

  (void)self;
  if (request == MP_STREAM_POLL) {
    // Return flags: MP_STREAM_POLL_RD (if readable) | MP_STREAM_POLL_WR (if
    // writable) Since we can always "print" (write), we return WR.
    ret = MP_STREAM_POLL_WR;
  } else {
    *errcode = MP_EINVAL;
    ret = MP_STREAM_ERROR;
  }
  return ret;
}

// The Stream Protocol Struct
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
};
static MP_DEFINE_CONST_DICT(vt_VT_locals_dict, vtinal_locals_dict_table);

// Type Definition
MP_DEFINE_CONST_OBJ_TYPE(vt_VT_type, MP_QSTR_VT, MP_TYPE_FLAG_NONE, make_new,
                         vt_VT_make_new, protocol, &vt_stream_p, locals_dict,
                         &vt_VT_locals_dict);

//  Module Definition ---

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
