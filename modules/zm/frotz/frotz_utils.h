#ifndef FROTZ_UTILS_H
#define FROTZ_UTILS_H

#include "py/misc.h"
#include "py/mphal.h"
#include <stdarg.h>
#include <string.h>

static inline char *zm_basename(char *path) {
  char *base = strrchr(path, '/');
  return base ? base + 1 : path;
}

static inline int zm_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);

  vstr_t vstr;
  vstr_init(&vstr, 64);
  vstr_vprintf(&vstr, format, args);
  va_end(args);

  if (vstr.len > 0) {
    mp_hal_stdout_tx_strn(vstr.buf, vstr.len);
  }

  int len = vstr.len;
  vstr_clear(&vstr);
  return len;
}

#endif
