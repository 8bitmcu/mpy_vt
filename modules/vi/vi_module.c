#include "vi_module.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h" // Needed for stream protocols
#include "vi.h"
#include <stdint.h>

// Global pointer for vi_main to access the current instance's stream
// Note: If you plan on multiple instances, you'll need to pass 'self' into
// vi_main
vi_vi_obj_t *current_vi_instance = NULL;

// The Constructor: vi.Vi(filename, stream, width, height)
static mp_obj_t vi_vi_make_new(const mp_obj_type_t *type, size_t n_args,
                               size_t n_kw, const mp_obj_t *args) {
  mp_arg_check_num(n_args, n_kw, 2, 4, false);

  vi_vi_obj_t *self = m_new_obj(vi_vi_obj_t);
  self->base.type = type;

  // Handle Filename
  if (!mp_obj_is_str(args[0])) {
    mp_raise_TypeError(MP_ERROR_TEXT("Filename must be a string"));
  }
  // Copy the string into the MP heap so it is safe during the vi session
  const char *fname = mp_obj_str_get_str(args[0]);

  // Handle Stream (e.g., KVM object)
  self->stream_obj = args[1];
  self->stream_p = mp_get_stream_raise(self->stream_obj,
                                       MP_STREAM_OP_READ | MP_STREAM_OP_WRITE);

  // Handle Dimensions
  int tw = 40, th = 16;
  self->width = (n_args > 2) ? mp_obj_get_int(args[2]) : tw;
  self->height = (n_args > 3) ? mp_obj_get_int(args[3]) : th;

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

// Define the Module-level globals
static const mp_rom_map_elem_t vi_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vi)},
    {MP_ROM_QSTR(MP_QSTR_Vi), MP_ROM_PTR(&vi_type_Vi)},
};
static MP_DEFINE_CONST_DICT(vi_module_globals, vi_module_globals_table);

// Register the module
const mp_obj_module_t vi_user_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&vi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vi, vi_user_module);
