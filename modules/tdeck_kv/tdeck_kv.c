/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 *
 * This module merges the vt and tdeck_kbd modules together into one streamable python object.
 */

#include "py/mperrno.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"

extern const mp_obj_type_t vt_VT_type;
extern const mp_obj_type_t tdeck_kbd_type;

typedef struct _tdeck_kv_obj_t {
  mp_obj_base_t base;
  mp_obj_t vt_instance;  // The Terminal (Write-only)
  mp_obj_t kbd_instance; // The Keyboard (Read-only)
} tdeck_kv_obj_t;

// --- Stream Protocol Hand-off ---

static mp_uint_t kv_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                         int *errcode) {
  tdeck_kv_obj_t *self = MP_OBJ_TO_PTR(self_in);

  const mp_stream_p_t *stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
      mp_obj_get_type(self->kbd_instance), protocol);

  if (stream && stream->read) {
    return stream->read(self->kbd_instance, buf, size, errcode);
  }

  *errcode = MP_EOPNOTSUPP;
  return MP_STREAM_ERROR;
}

static mp_uint_t kv_write(mp_obj_t self_in, const void *buf, mp_uint_t size,
                          int *errcode) {
  tdeck_kv_obj_t *self = MP_OBJ_TO_PTR(self_in);

  const mp_stream_p_t *stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
      mp_obj_get_type(self->vt_instance), protocol);

  if (stream && stream->write) {
    return stream->write(self->vt_instance, buf, size, errcode);
  }

  *errcode = MP_EOPNOTSUPP;
  return MP_STREAM_ERROR;
}

static mp_uint_t kv_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                          int *errcode) {
  tdeck_kv_obj_t *self = MP_OBJ_TO_PTR(self_in);

  if (request == MP_STREAM_POLL) {
    uintptr_t flags = arg;
    uintptr_t ret = 0;

    // Read poll from Keyboard
    const mp_stream_p_t *k_stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
        mp_obj_get_type(self->kbd_instance), protocol);
    if (k_stream && k_stream->ioctl) {
      ret |= k_stream->ioctl(self->kbd_instance, request,
                             flags & MP_STREAM_POLL_RD, errcode);
    }

    // Write poll from VT
    const mp_stream_p_t *v_stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
        mp_obj_get_type(self->vt_instance), protocol);
    if (v_stream && v_stream->ioctl) {
      ret |= v_stream->ioctl(self->vt_instance, request,
                             flags & MP_STREAM_POLL_WR, errcode);
    }

    return ret;
  }
  return 0;
}

static const mp_stream_p_t kv_stream_p = {
    .read = kv_read,
    .write = kv_write,
    .ioctl = kv_ioctl,
    .is_text = true,
};

// --- Constructor: kv = tdeck_kv.KV(vt_obj, kbd_obj) ---

static mp_obj_t tdeck_kv_make_new(const mp_obj_type_t *type, size_t n_args,
                                  size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 2, 2, false);

  tdeck_kv_obj_t *self = m_new_obj(tdeck_kv_obj_t);
  self->base.type = type;
  self->vt_instance = args[0];
  self->kbd_instance = args[1];

  return MP_OBJ_FROM_PTR(self);
}

// --- Registration ---

MP_DEFINE_CONST_OBJ_TYPE(tdeck_kv_type, MP_QSTR_KV, MP_TYPE_FLAG_ITER_IS_STREAM,
                         make_new, tdeck_kv_make_new, protocol, &kv_stream_p);

static const mp_rom_map_elem_t kv_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tdeck_kv)},
    {MP_ROM_QSTR(MP_QSTR_KV), MP_ROM_PTR(&tdeck_kv_type)},
};
static MP_DEFINE_CONST_DICT(kv_module_globals, kv_module_globals_table);

const mp_obj_module_t kv_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&kv_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tdeck_kv, kv_user_cmodule);
