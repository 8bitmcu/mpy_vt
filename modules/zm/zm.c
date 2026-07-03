#include "py/mpconfig.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/runtime.h"
#include <setjmp.h>

extern int frotz_main(int argc, char *argv[]);

jmp_buf frotz_exit_env;

// 1. Define the object structure to hold interpreter state
typedef struct _zm_zm_obj_t {
  mp_obj_base_t base;
  // Add your Bocfel state pointers here
} zm_zm_obj_t;

// 2. The constructor: zm.ZMachine("file.z5")
static mp_obj_t zm_make_new(const mp_obj_type_t *type, size_t n_args,
                            size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 1, 1, false);

  zm_zm_obj_t *self = m_new_obj(zm_zm_obj_t);
  self->base.type = type;

  // Initialize your C interpreter logic here
  // const char *filename = mp_obj_str_get_str(args[0]);
  //
  return MP_OBJ_FROM_PTR(self);
}

// 3. A method: z.run()
static mp_obj_t zm_run(mp_obj_t self_in) {
  char *dummy_argv[] = {"dfrotz", "game1.dat", NULL};

  if (setjmp(frotz_exit_env) == 0) {
    frotz_main(2, dummy_argv);
  }

  // zm_zm_obj_t *self = MP_OBJ_TO_PTR(self_in);
  // Call your C interpreter's main loop here
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(zm_run_obj, zm_run);

// 4. Register methods in the class dictionary
static const mp_rom_map_elem_t zm_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&zm_run_obj)},
};
static MP_DEFINE_CONST_DICT(zm_locals_dict, zm_locals_dict_table);

// 5. Define the class type
MP_DEFINE_CONST_OBJ_TYPE(zm_type, MP_QSTR_ZMachine, MP_TYPE_FLAG_NONE, make_new,
                         zm_make_new, locals_dict, &zm_locals_dict);

// 6. Register the module
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
