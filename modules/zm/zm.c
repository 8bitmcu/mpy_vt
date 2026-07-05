#include "zm.h"
#include "py/mpconfig.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/runtime.h"
#include <setjmp.h>
#include <stdio.h>

extern int frotz_main(int argc, char *argv[]);

jmp_buf frotz_exit_env;

// Global pointer for frotz_main to access the current instance's stream
zm_zm_obj_t *current_frotz_instance = NULL;

// The constructor: zm.ZMachine("file.z5")
static mp_obj_t zm_make_new(const mp_obj_type_t *type, size_t n_args,
                            size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 2, 4, false);

  zm_zm_obj_t *self = m_new_obj(zm_zm_obj_t);
  self->base.type = type;

  if (!mp_obj_is_str(args[0])) {
    mp_raise_TypeError(MP_ERROR_TEXT("Filename must be a string"));
  }
  self->fname = mp_obj_str_get_str(args[0]);

  // Handle Stream (e.g., KVM object)
  self->stream_obj = args[1];
  self->stream_p = mp_get_stream_raise(self->stream_obj,
                                       MP_STREAM_OP_READ | MP_STREAM_OP_WRITE);

  int tw = 40, th = 16;
  self->width = (n_args > 2) ? mp_obj_get_int(args[2]) : tw;
  self->height = (n_args > 3) ? mp_obj_get_int(args[3]) : th;

  current_frotz_instance = self;

  return MP_OBJ_FROM_PTR(self);
}

// A method: z.run()
static mp_obj_t zm_run(mp_obj_t self_in) {
  zm_zm_obj_t *self = MP_OBJ_TO_PTR(self_in);

  char width_str[16];
  char height_str[16];

  snprintf(width_str, sizeof(width_str), "%d", self->width);
  snprintf(height_str, sizeof(height_str), "%d", self->height);

  char *dummy_argv[] = {
      "dfrotz",    // argv[0]
      "-w",        // argv[1]
      width_str,   // argv[2]
      "-h",        // argv[3]
      height_str,  // argv[4]
      self->fname, // argv[5]
      NULL         // Required terminator
  };

  if (setjmp(frotz_exit_env) == 0) {
    frotz_main(6, dummy_argv);
  }

  return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_1(zm_run_obj, zm_run);

// Register methods in the class dictionary
static const mp_rom_map_elem_t zm_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&zm_run_obj)},
};
static MP_DEFINE_CONST_DICT(zm_locals_dict, zm_locals_dict_table);

// Define the class type
MP_DEFINE_CONST_OBJ_TYPE(zm_type, MP_QSTR_ZMachine, MP_TYPE_FLAG_NONE, make_new,
                         zm_make_new, locals_dict, &zm_locals_dict);

// Register the module
static const mp_rom_map_elem_t zm_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_zm)},
    {MP_ROM_QSTR(MP_QSTR_ZMachine), MP_ROM_PTR(&zm_type)},
};
static MP_DEFINE_CONST_DICT(zm_module_globals, zm_module_globals_table);

const mp_obj_module_t zm_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&zm_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_zm, zm_user_cmodule);
