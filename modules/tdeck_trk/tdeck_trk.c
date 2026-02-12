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

#define TRK_UP 3
#define TRK_DOWN 15
#define TRK_LEFT 1
#define TRK_RIGHT 2
#define TRK_CLICK 0

// Volatile counters for ISR safety
static volatile int count_up = 0;
static volatile int count_down = 0;
static volatile int count_left = 0;
static volatile int count_right = 0;
static int last_click_state = 1;
static int64_t last_press_time = 0;

// Unified ISR for all directions
static void IRAM_ATTR trackball_isr(void *arg) {
  uint32_t pin = (uint32_t)arg;
  if (pin == TRK_UP)
    count_up++;
  if (pin == TRK_DOWN)
    count_down++;
  if (pin == TRK_LEFT)
    count_left++;
  if (pin == TRK_RIGHT)
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
  int current_state = gpio_get_level(TRK_CLICK);
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

static mp_obj_t tdeck_trk_init(void) {
  gpio_config_t io_conf = {.intr_type = GPIO_INTR_NEGEDGE,
                           .mode = GPIO_MODE_INPUT,
                           .pin_bit_mask =
                               (1ULL << TRK_UP) | (1ULL << TRK_DOWN) |
                               (1ULL << TRK_LEFT) | (1ULL << TRK_RIGHT) |
                               (1ULL << TRK_CLICK),
                           .pull_up_en = 1};
  gpio_config(&io_conf);

  // Ensure ISR service is installed (shared across all pins)
  gpio_install_isr_service(0);
  gpio_isr_handler_add(TRK_UP, trackball_isr, (void *)TRK_UP);
  gpio_isr_handler_add(TRK_DOWN, trackball_isr, (void *)TRK_DOWN);
  gpio_isr_handler_add(TRK_LEFT, trackball_isr, (void *)TRK_LEFT);
  gpio_isr_handler_add(TRK_RIGHT, trackball_isr, (void *)TRK_RIGHT);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(tdeck_trk_init_obj, tdeck_trk_init);

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
