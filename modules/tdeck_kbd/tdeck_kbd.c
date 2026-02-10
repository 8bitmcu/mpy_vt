/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 *
 * This module handles a LILYGO T-DECK keyboard as a streamble python object
 */

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/stream.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define KBD_ADDR 0x55
#define I2C_FREQ_HZ 400000

typedef struct _tdeck_kbd_obj_t {
  mp_obj_base_t base;
  int sda;
  int scl;
} tdeck_kbd_obj_t;

const mp_obj_type_t tdeck_kbd_type;
static int16_t cached_key = -1;

// --- I2C Driver Management (Now takes Pins) ---

static void init_i2c_hardware(int sda, int scl) {
  // NOTE: this driver assumes PWR on GPIO 10
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = sda,
      .scl_io_num = scl,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_FREQ_HZ,
  };

  i2c_driver_delete(I2C_MASTER_NUM);
  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static uint8_t raw_kbd_read() {
  uint8_t rx_data = 0;
  esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, KBD_ADDR,
                                              &rx_data, 1, pdMS_TO_TICKS(10));
  return (err == ESP_OK) ? rx_data : 0;
}

// --- Stream Protocol (Read Only) ---

static mp_uint_t kbd_read(mp_obj_t self_in, void *buf, mp_uint_t size,
                          int *errcode) {
  if (size == 0)
    return 0;

  uint8_t key = 0;
  if (cached_key != -1) {
    key = (uint8_t)cached_key;
    cached_key = -1;
  } else {
    key = raw_kbd_read();
  }

  if (key != 0) {
    // Map Enter (10 or 13) to Carriage Return (13) for REPL compatibility
    if (key == 10 || key == 13)
      key = 13;
    ((uint8_t *)buf)[0] = key;
    return 1;
  }

  *errcode = MP_EAGAIN;
  return MP_STREAM_ERROR;
}

static mp_uint_t kbd_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg,
                           int *errcode) {
  if (request == MP_STREAM_POLL) {
    uintptr_t flags = arg;
    uintptr_t ret = 0;

    if (flags & MP_STREAM_POLL_RD) {
      if (cached_key == -1) {
        uint8_t k = raw_kbd_read();
        if (k != 0)
          cached_key = k;
      }
      if (cached_key != -1)
        ret |= MP_STREAM_POLL_RD;
    }
    return ret;
  }
  return 0;
}

static const mp_stream_p_t kbd_stream_p = {
    .read = kbd_read,
    .write = NULL, // Explicitly no write support
    .ioctl = kbd_ioctl,
    .is_text = true,
};

// --- Module Functions ---

static mp_obj_t tdeck_kbd_make_new(const mp_obj_type_t *type, size_t n_args,
                                   size_t n_kw, const mp_obj_t *all_args) {
  // We expect 2 arguments: SDA and SCL
  enum { ARG_sda, ARG_scl };
  static const mp_arg_t allowed_args[] = {
      {MP_QSTR_sda, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 18}},
      {MP_QSTR_scl, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 8}},
  };

  // Parse arguments
  mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
  mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args),
                            allowed_args, args);

  tdeck_kbd_obj_t *self = m_new_obj(tdeck_kbd_obj_t);
  self->base.type = &tdeck_kbd_type;
  self->sda = args[ARG_sda].u_int;
  self->scl = args[ARG_scl].u_int;

  // Initialize with the passed pins
  init_i2c_hardware(self->sda, self->scl);

  return MP_OBJ_FROM_PTR(self);
}

static const mp_rom_map_elem_t kbd_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj)},
};
static MP_DEFINE_CONST_DICT(kbd_locals_dict, kbd_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(tdeck_kbd_type, MP_QSTR_Keyboard,
                         MP_TYPE_FLAG_ITER_IS_STREAM, make_new,
                         tdeck_kbd_make_new, protocol, &kbd_stream_p,
                         locals_dict, &kbd_locals_dict);

static const mp_rom_map_elem_t kbd_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tdeck_kbd)},
    {MP_ROM_QSTR(MP_QSTR_Keyboard), MP_ROM_PTR(&tdeck_kbd_type)},
};
static MP_DEFINE_CONST_DICT(kbd_module_globals, kbd_module_globals_table);

const mp_obj_module_t kbd_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&kbd_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tdeck_kbd, kbd_module);
