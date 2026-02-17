#ifndef VI_MODULE_H
#define VI_MODULE_H

#include <stdint.h>
#include "py/obj.h"
#include "py/stream.h"



typedef struct _vi_vi_obj_t {
  mp_obj_base_t base;
  mp_obj_t stream_obj;
  const mp_stream_p_t *stream_p;
  uint16_t width;
  uint16_t height;
} vi_vi_obj_t;

#endif
