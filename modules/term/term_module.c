#include "py/mphal.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"

// Object Structure ---
typedef struct _term_Terminal_obj_t {
  mp_obj_base_t base;
} term_Terminal_obj_t;

// Forward declaration
const mp_obj_type_t term_Terminal_type;

// Stream Protocol Implementation ---

// READ: Called when MicroPython wants input (e.g. `input()`)
static mp_uint_t term_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                           int *errcode) {
  term_Terminal_obj_t *self = MP_OBJ_TO_PTR(self_in);

  (void)self;

  // TODO: Return 0 if no data, or bytes read if data exists.
  // Use *errcode = MP_EAGAIN and return MP_STREAM_ERROR for non-blocking wait.
  return 0;
}

static mp_uint_t term_write(mp_obj_t self_in, const void *buf, mp_uint_t size,
                            int *errcode) {
  term_Terminal_obj_t *self = MP_OBJ_TO_PTR(self_in);

  (void)self;
  printf("[TERM] %.*s", (int)size, (char *)buf);

  return size;
}

static mp_obj_t term_Terminal_write(mp_obj_t self_in, mp_obj_t arg) {
  mp_buffer_info_t bufinfo;
  // This helper handles both 'bytes' and 'str' types automatically
  if (mp_get_buffer(arg, &bufinfo, MP_BUFFER_READ)) {
    int err;
    term_write(self_in, bufinfo.buf, bufinfo.len, &err);
    return mp_obj_new_int_from_uint(bufinfo.len);
  } else {
    mp_raise_TypeError(MP_ERROR_TEXT("string or bytes required"));
  }
}

// Define the MicroPython function object
MP_DEFINE_CONST_FUN_OBJ_2(term_Terminal_write_obj, term_Terminal_write);

// IOCTL: Called for polling and configuration
static mp_uint_t term_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                            int *errcode) {
  term_Terminal_obj_t *self = MP_OBJ_TO_PTR(self_in);
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
static const mp_stream_p_t term_stream_p = {
    .read = term_read,
    .write = term_write,
    .ioctl = term_ioctl,
    .is_text = true,
};

//  Class Definition ---

// Constructor: term.Terminal()
static mp_obj_t term_Terminal_make_new(const mp_obj_type_t *type, size_t n_args,
                                       size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 0, 0, false);
  term_Terminal_obj_t *self = m_new_obj(term_Terminal_obj_t);
  self->base.type = &term_Terminal_type;
  return MP_OBJ_FROM_PTR(self);
}

// Locals Dict (Methods attached to the object)
// Even if empty, it's good practice to define it to avoid segfaults on dir(obj)
static const mp_rom_map_elem_t term_Terminal_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&term_Terminal_write_obj)},
};
static MP_DEFINE_CONST_DICT(term_Terminal_locals_dict,
                            term_Terminal_locals_dict_table);

// Type Definition
MP_DEFINE_CONST_OBJ_TYPE(term_Terminal_type, MP_QSTR_Terminal,
                         MP_TYPE_FLAG_NONE, make_new, term_Terminal_make_new,
                         protocol, &term_stream_p, locals_dict,
                         &term_Terminal_locals_dict);

//  Module Definition ---

static const mp_rom_map_elem_t term_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_term)},
    {MP_ROM_QSTR(MP_QSTR_Terminal), MP_ROM_PTR(&term_Terminal_type)},
};
static MP_DEFINE_CONST_DICT(term_module_globals, term_module_globals_table);

const mp_obj_module_t term_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&term_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_term, term_module);
