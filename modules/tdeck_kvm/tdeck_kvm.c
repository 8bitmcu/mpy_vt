/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 *
 * This module merges the vt and tdeck_kbd modules together into one streamable
 * python object.
 */

#include "py/mperrno.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"
#include <string.h>

extern const mp_obj_type_t vt_VT_type;
extern const mp_obj_type_t tdeck_kbd_type;

// Ring buffer for injected "Ghost Keys"
#define INJECT_BUF_SIZE 32
static char inject_buf[INJECT_BUF_SIZE];
static uint8_t inject_head = 0;
static uint8_t inject_tail = 0;

typedef struct _tdeck_kvm_obj_t {
  mp_obj_base_t base;
  mp_obj_t vt_instance;
  mp_obj_t kbd_instance;
} tdeck_kvm_obj_t;

// --- Injection Logic ---

void internal_inject(const char *data) {
  size_t len = strlen(data);
  for (size_t i = 0; i < len; i++) {
    uint8_t next = (inject_head + 1) % INJECT_BUF_SIZE;
    if (next != inject_tail) { // Avoid overflow
      inject_buf[inject_head] = data[i];
      inject_head = next;
    }
  }
}

// Python Method: kvm.inject("string")
static mp_obj_t kvm_inject(mp_obj_t self_in, mp_obj_t arg) {
  const char *data = mp_obj_str_get_str(arg);
  internal_inject(data);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(kvm_inject_obj, kvm_inject);

// --- Stream Protocol ---

static mp_uint_t kvm_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                          int *errcode) {
  tdeck_kvm_obj_t *self = MP_OBJ_TO_PTR(self_in);
  char *dest = (char *)buf;
  mp_uint_t bytes_read = 0;

  // 1. Check Injection Buffer first
  while (bytes_read < size && inject_tail != inject_head) {
    dest[bytes_read++] = inject_buf[inject_tail];
    inject_tail = (inject_tail + 1) % INJECT_BUF_SIZE;
  }

  if (bytes_read > 0)
    return bytes_read;

  // 2. Fall back to hardware keyboard if buffer is empty
  const mp_stream_p_t *stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
      mp_obj_get_type(self->kbd_instance), protocol);

  if (stream && stream->read) {
    return stream->read(self->kbd_instance, buf, size, errcode);
  }

  *errcode = MP_EOPNOTSUPP;
  return MP_STREAM_ERROR;
}

static mp_uint_t kvm_write(mp_obj_t self_in, const void *buf, mp_uint_t size,
                           int *errcode) {
  tdeck_kvm_obj_t *self = MP_OBJ_TO_PTR(self_in);

  const mp_stream_p_t *stream = (const mp_stream_p_t *)MP_OBJ_TYPE_GET_SLOT(
      mp_obj_get_type(self->vt_instance), protocol);

  if (stream && stream->write) {
    return stream->write(self->vt_instance, buf, size, errcode);
  }

  *errcode = MP_EOPNOTSUPP;
  return MP_STREAM_ERROR;
}

static mp_uint_t kvm_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                           int *errcode) {
  tdeck_kvm_obj_t *self = MP_OBJ_TO_PTR(self_in);

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

// --- Constructor: kv = tdeck_kv.KV(vt_obj, kbd_obj) ---

static mp_obj_t tdeck_kvm_make_new(const mp_obj_type_t *type, size_t n_args,
                                   size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 2, 2, false);

  tdeck_kvm_obj_t *self = m_new_obj(tdeck_kvm_obj_t);
  self->base.type = type;
  self->vt_instance = args[0];
  self->kbd_instance = args[1];

  return MP_OBJ_FROM_PTR(self);
}

// --- Registration ---

// Add the inject method to the locals table
static const mp_rom_map_elem_t kvm_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_inject), MP_ROM_PTR(&kvm_inject_obj)},
    {MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj)},
};
static MP_DEFINE_CONST_DICT(kvm_locals_dict, kvm_locals_dict_table);

static const mp_stream_p_t kvm_stream_p = {
    .read = kvm_read,
    .write = kvm_write,
    .ioctl = kvm_ioctl,
    .is_text = true,
};

MP_DEFINE_CONST_OBJ_TYPE(tdeck_kvm_type, MP_QSTR_KVM,
                         MP_TYPE_FLAG_ITER_IS_STREAM, make_new,
                         tdeck_kvm_make_new, protocol, &kvm_stream_p,
                         locals_dict, &kvm_locals_dict);

static const mp_rom_map_elem_t kvm_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tdeck_kvm)},
    {MP_ROM_QSTR(MP_QSTR_KVM), MP_ROM_PTR(&tdeck_kvm_type)},
};
static MP_DEFINE_CONST_DICT(kvm_module_globals, kvm_module_globals_table);

const mp_obj_module_t kvm_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&kvm_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tdeck_kvm, kvm_user_cmodule);
