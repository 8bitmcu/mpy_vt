#ifndef ZM_H
#define ZM_H

#include "py/obj.h"
#include "py/stream.h"
#include <stdint.h>

typedef struct _zm_zm_obj_t {
  mp_obj_base_t base;
  mp_obj_t stream_obj;
  const mp_stream_p_t *stream_p;
} zm_zm_obj_t;

#endif
