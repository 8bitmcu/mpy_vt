/*
 * MicroPython ANSI Terminal Wrapper
 * Copyright (c) 2026 8bitmcu
 * License: MIT
 *
 * This module handles the LILIGY T-Deck Trackball
 */

#include "driver/gpio.h"
#include "py/runtime.h"
#include "py/stackctrl.h"
#include "esp_timer.h"

// Pin assignments, set once by init() -- see tdeck_trk_init() below. Not
// #define'd anymore since they vary per board; callers pass them in from
// board.py instead of this module hardcoding one board's wiring.
static int trk_up = 0;
static int trk_down = 0;
static int trk_left = 0;
static int trk_right = 0;
static int trk_click = 0;

// Volatile counters for ISR safety
static volatile int count_up = 0;
static volatile int count_down = 0;
static volatile int count_left = 0;
static volatile int count_right = 0;
static int last_click_state = 1;
static int64_t last_press_time = 0;

// Unified ISR for all directions
static void IRAM_ATTR trackball_isr(void *arg) {
  int pin = (int)(uint32_t)arg;
  if (pin == trk_up)
    count_up++;
  if (pin == trk_down)
    count_down++;
  if (pin == trk_left)
    count_left++;
  if (pin == trk_right)
    count_right++;
}

// Vertical: Up - Down
static mp_obj_t tdeck_trk_get_scroll_vert(void) {
  int delta = count_up - count_down;
  count_up = 0;
  count_down = 0;
  return mp_obj_new_int(delta);
}
static MP_DEFINE_CONST_FUN_OBJ_0(tdeck_trk_get_scroll_vert_obj,
                                 tdeck_trk_get_scroll_vert);

// Horizontal: Right - Left (Standard Cartesian)
static mp_obj_t tdeck_trk_get_scroll_horiz(void) {
  int delta = count_right - count_left;
  count_right = 0;
  count_left = 0;
  return mp_obj_new_int(delta);
}
static MP_DEFINE_CONST_FUN_OBJ_0(tdeck_trk_get_scroll_horiz_obj,
                                 tdeck_trk_get_scroll_horiz);

static mp_obj_t tdeck_trk_get_click(void) {
  int current_state = gpio_get_level(trk_click);
  int64_t now = esp_timer_get_time();
  mp_obj_t result = mp_const_false;

  // 1. Falling Edge (Press)
  if (current_state == 0 && last_click_state == 1) {
    last_press_time = now;
  }

  // 2. Rising Edge (Release)
  else if (current_state == 1 && last_click_state == 0) {
    int64_t duration = now - last_press_time;

    if (duration > 500000) {
      // LONG PRESS: CTRL+C
      mp_sched_keyboard_interrupt();
    } else if (duration > 20000) { // 20ms debounce floor
      result = mp_const_true;
    }
  }

  last_click_state = current_state;
  return result;
}
static MP_DEFINE_CONST_FUN_OBJ_0(tdeck_trk_get_click_obj, tdeck_trk_get_click);

// tdeck_trk.init(up=.., down=.., left=.., right=.., click=..) -- pins come
// from the caller (board.py) rather than being hardcoded here, so this
// module isn't tied to one board's wiring.
static const mp_arg_t tdeck_trk_init_args[] = {
    {MP_QSTR_up, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
    {MP_QSTR_down, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
    {MP_QSTR_left, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
    {MP_QSTR_right, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
    {MP_QSTR_click, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0}},
};

static mp_obj_t tdeck_trk_init(size_t n_args, const mp_obj_t *pos_args,
                               mp_map_t *kw_args) {
  mp_arg_val_t args[MP_ARRAY_SIZE(tdeck_trk_init_args)];
  mp_arg_parse_all(n_args, pos_args, kw_args,
                   MP_ARRAY_SIZE(tdeck_trk_init_args), tdeck_trk_init_args,
                   args);

  trk_up = args[0].u_int;
  trk_down = args[1].u_int;
  trk_left = args[2].u_int;
  trk_right = args[3].u_int;
  trk_click = args[4].u_int;

  gpio_config_t io_conf = {.intr_type = GPIO_INTR_NEGEDGE,
                           .mode = GPIO_MODE_INPUT,
                           .pin_bit_mask =
                               (1ULL << trk_up) | (1ULL << trk_down) |
                               (1ULL << trk_left) | (1ULL << trk_right) |
                               (1ULL << trk_click),
                           .pull_up_en = 1};
  gpio_config(&io_conf);

  // Ensure ISR service is installed (shared across all pins)
  gpio_install_isr_service(0);
  gpio_isr_handler_add(trk_up, trackball_isr, (void *)(uint32_t)trk_up);
  gpio_isr_handler_add(trk_down, trackball_isr, (void *)(uint32_t)trk_down);
  gpio_isr_handler_add(trk_left, trackball_isr, (void *)(uint32_t)trk_left);
  gpio_isr_handler_add(trk_right, trackball_isr, (void *)(uint32_t)trk_right);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(tdeck_trk_init_obj, 0, tdeck_trk_init);

static const mp_rom_map_elem_t tdeck_trk_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tdeck_trk)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&tdeck_trk_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_get_scroll_vert),
     MP_ROM_PTR(&tdeck_trk_get_scroll_vert_obj)},
    {MP_ROM_QSTR(MP_QSTR_get_scroll_horiz),
     MP_ROM_PTR(&tdeck_trk_get_scroll_horiz_obj)},
    {MP_ROM_QSTR(MP_QSTR_get_click), MP_ROM_PTR(&tdeck_trk_get_click_obj)},
};
static MP_DEFINE_CONST_DICT(tdeck_trk_globals, tdeck_trk_globals_table);

const mp_obj_module_t tdeck_trk_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&tdeck_trk_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tdeck_trk, tdeck_trk_user_cmodule);
