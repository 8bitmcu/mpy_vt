#include "../vt/st.h"
#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"

typedef struct _tu_tu_obj_t {
  mp_obj_base_t base;
} tu_tu_obj_t;

static mp_obj_t tu_make_new(const mp_obj_type_t *type, size_t n_args,
                            size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 0, 0, false);

  tu_tu_obj_t *self = m_new_obj(tu_tu_obj_t);
  self->base.type = type;

  term.dirty[0] = 1;

  return MP_OBJ_FROM_PTR(self);
}

/* static mp_obj_t tu_draw(mp_obj_t self_in) {
    tu_tu_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // Draw logic here
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(tu_draw_obj, tu_draw);
*/

static const mp_rom_map_elem_t tu_locals_dict_table[] = {
    // Uncomment when you add methods:
    // { MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&tu_draw_obj) },
};
static MP_DEFINE_CONST_DICT(tu_locals_dict, tu_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(tu_type, MP_QSTR_TU, MP_TYPE_FLAG_NONE, make_new,
                         tu_make_new, locals_dict, &tu_locals_dict);

static const mp_rom_map_elem_t tu_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tu)},
    {MP_ROM_QSTR(MP_QSTR_TU), MP_ROM_PTR(&tu_type)},
};
static MP_DEFINE_CONST_DICT(tu_module_globals, tu_module_globals_table);

const mp_obj_module_t tu_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&tu_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tu, tu_user_cmodule);
