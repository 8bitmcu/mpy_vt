#include "../vt/st.h"
#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"

typedef struct _vttui_vttui_obj_t {
  mp_obj_base_t base;
  mp_obj_t stream_obj;
  const mp_stream_p_t *stream_p;
  uint16_t width;
  uint16_t height;
} vttui_vttui_obj_t;

static mp_obj_t vttui_make_new(const mp_obj_type_t *type, size_t n_args,
                               size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 1, 3, false);

  vttui_vttui_obj_t *self = m_new_obj(vttui_vttui_obj_t);
  self->base.type = type;

  self->stream_obj = args[0];
  self->stream_p = mp_get_stream_raise(self->stream_obj,
                                       MP_STREAM_OP_READ | MP_STREAM_OP_WRITE);

  // Handle Dimensions
  int tw = 40, th = 16;
  self->width = (n_args > 1) ? mp_obj_get_int(args[1]) : tw;
  self->height = (n_args > 2) ? mp_obj_get_int(args[2]) : th;

  return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t vttui_draw(mp_obj_t self_in) {
  vttui_vttui_obj_t *self = MP_OBJ_TO_PTR(self_in);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(vttui_draw_obj, vttui_draw);

static const mp_rom_map_elem_t vttui_locals_dict_table[] = {
    // Uncomment when you add methods:
    {MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&vttui_draw_obj)},
};
static MP_DEFINE_CONST_DICT(vttui_locals_dict, vttui_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(vttui_type, MP_QSTR_VTTU, MP_TYPE_FLAG_NONE, make_new,
                         vttui_make_new, locals_dict, &vttui_locals_dict);

static const mp_rom_map_elem_t vttui_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vttui)},
    {MP_ROM_QSTR(MP_QSTR_VTTUI), MP_ROM_PTR(&vttui_type)},
};
static MP_DEFINE_CONST_DICT(vttui_module_globals, vttui_module_globals_table);

const mp_obj_module_t vttui_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&vttui_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vttui, vttui_user_cmodule);
