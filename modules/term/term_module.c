#include "mpfile.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "st_term.h"

#include "mpfile.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "st7789.h"

// Object Structure ---

extern const mp_obj_type_t st7789_ST7789_type;
term_Terminal_obj_t *current_term_obj = NULL;

// Forward declaration
const mp_obj_type_t term_Terminal_type;

static mp_obj_t term_terminal_print_grid(mp_obj_t self_in) {
  // Loop through every row
  for (int y = 0; y < term.row; y++) {
    // Loop through every column
    for (int x = 0; x < term.col; x++) {
      // st stores characters as 'Rune' (uint32_t)
      uint32_t u = term.line[y][x].u;

      // If the cell is empty (0), print a dot or space
      if (u == 0 || u == ' ') {
        printf(".");
      } else {
        printf("%c", (char)u);
      }
    }
    printf("\n"); // End of row
  }
  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(term_terminal_print_grid_obj,
                                 term_terminal_print_grid);

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
  size_t len;
  const char *data = mp_obj_str_get_data(arg, &len);

  for (size_t i = 0; i < len; i++) {
    // tputc is the heart of st_term.c
    // It parses the byte and updates the internal grid
    tputc((uchar)data[i]);
  }

  draw();
  return mp_const_none;
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
  mp_arg_check_num(n_args, n_kw, 2, 2, false);

  term_Terminal_obj_t *self = m_new_obj(term_Terminal_obj_t);
  self->base.type = &term_Terminal_type;

  if (!mp_obj_is_type(args[0], &st7789_ST7789_type)) {
    mp_raise_TypeError(MP_ERROR_TEXT("Arg 1 must be ST7789 object"));
  }
  self->display_drv = (st7789_ST7789_obj_t *)MP_OBJ_TO_PTR(args[0]);

  self->font_obj = (mp_obj_module_t *)MP_OBJ_TO_PTR(args[1]);

  tnew(40, 16);
  current_term_obj = self;

  return MP_OBJ_FROM_PTR(self);
}

// Locals Dict (Methods attached to the object)
// Even if empty, it's good practice to define it to avoid segfaults on dir(obj)
static const mp_rom_map_elem_t terminal_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&term_Terminal_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_print_grid),
     MP_ROM_PTR(&term_terminal_print_grid_obj)},
};
static MP_DEFINE_CONST_DICT(term_Terminal_locals_dict,
                            terminal_locals_dict_table);

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
