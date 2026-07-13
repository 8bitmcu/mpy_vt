/*
 * MicroPython vi Interface Library
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 */

#include "vi_module.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "vi.h"
#include <stdint.h>

// Global pointer for vi_main to access the current instance's stream
vi_vi_obj_t *current_vi_instance = NULL;

// The Constructor: vi.Vi(env, args)
static mp_obj_t vi_vi_make_new(const mp_obj_type_t *type, size_t n_args,
                               size_t n_kw, const mp_obj_t *args) {
  // Enforce exactly 2 arguments: env (KVM object) and the tuple/list of shell
  // args
  mp_arg_check_num(n_args, n_kw, 2, 2, false);

  vi_vi_obj_t *self = m_new_obj(vi_vi_obj_t);
  self->base.type = type;

  // 1. Handle Environment (args[0])
  mp_obj_t env_obj = args[0];
  mp_obj_t dest_stream[2];

  // Extract the actual underlying stream object ('kvm') from env
  mp_load_method_maybe(env_obj, qstr_from_str("kvm"), dest_stream);
  if (dest_stream[0] != MP_OBJ_NULL) {
    self->stream_obj = dest_stream[0];
  }

  // Safely query the stream protocol on the extracted stream object
  self->stream_p = mp_get_stream_raise(self->stream_obj,
                                       MP_STREAM_OP_READ | MP_STREAM_OP_WRITE);

  int tw = 40, th = 16;
  mp_obj_t dest[2];

  mp_load_method_maybe(env_obj, MP_QSTR_cols, dest);
  if (dest[0] != MP_OBJ_NULL) {
    tw = mp_obj_get_int(dest[0]);
  } else {
    mp_load_method_maybe(env_obj, qstr_from_str("cols"), dest);
    if (dest[0] != MP_OBJ_NULL) {
      tw = mp_obj_get_int(dest[0]);
    }
  }

  mp_load_method_maybe(env_obj, MP_QSTR_rows, dest);
  if (dest[0] != MP_OBJ_NULL) {
    th = mp_obj_get_int(dest[0]);
  } else {
    mp_load_method_maybe(env_obj, qstr_from_str("rows"), dest);
    if (dest[0] != MP_OBJ_NULL) {
      th = mp_obj_get_int(dest[0]);
    }
  }

  self->width = tw;
  self->height = th;

  size_t shell_argc = 0;
  mp_obj_t *shell_argv;
  mp_obj_get_array(args[1], &shell_argc, &shell_argv);

  // Extract Filename if provided, otherwise default to an empty buffer string
  const char *fname = "";
  if (shell_argc > 0) {
    if (!mp_obj_is_str(shell_argv[0])) {
      mp_raise_TypeError(MP_ERROR_TEXT("Filename must be a string"));
    }
    fname = mp_obj_str_get_str(shell_argv[0]);
  }

  current_vi_instance = self;

  // --- GLOBAL NLR GUARD START ---
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    // Launch Toybox vi_main
    vi_init();
    volatile mp_obj_t keep_alive = vi_state_obj;
    volatile struct vi_data *keep_ptr = ptrTT;

    vi_main((char *)fname, self->width, self->height);

    // Clean up global pointer after exit
    current_vi_instance = NULL;
    nlr_pop();
  } else {
    // If we land here, vi_main crashed due to a MicroPython exception
    current_vi_instance = NULL;
    printf("\n[vi] Emergency Exit: System recovered from exception.\n");
    // Optional: Re-raise if you want the error message in the REPL
    // nlr_jump(nlr.ret_val);
  }
  // --- GLOBAL NLR GUARD END ---

  return MP_OBJ_FROM_PTR(self);
}

// Map the methods to the class
static const mp_rom_map_elem_t vi_vi_locals_dict_table[] = {

};
static MP_DEFINE_CONST_DICT(vi_vi_locals_dict, vi_vi_locals_dict_table);

// Define the Class Type
MP_DEFINE_CONST_OBJ_TYPE(vi_type_Vi, MP_QSTR_Vi, MP_TYPE_FLAG_NONE, make_new,
                         vi_vi_make_new, locals_dict, &vi_vi_locals_dict);

static mp_obj_t _vi_main(mp_obj_t env, mp_obj_t args) {
  mp_obj_t ctor_args[2] = {env, args};
  vi_vi_make_new(&vi_type_Vi, 2, 0, ctor_args);
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(vi_main_obj, _vi_main);

// Define the Module-level globals
static const mp_rom_map_elem_t vi_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vi)},
    {MP_ROM_QSTR(MP_QSTR_Vi), MP_ROM_PTR(&vi_type_Vi)},
    {MP_ROM_QSTR(MP_QSTR_main), MP_ROM_PTR(&vi_main_obj)},
};
static MP_DEFINE_CONST_DICT(vi_module_globals, vi_module_globals_table);

// Register the module
const mp_obj_module_t vi_user_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&vi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vi, vi_user_module);
