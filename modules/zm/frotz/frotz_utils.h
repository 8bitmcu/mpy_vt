#ifndef FROTZ_UTILS_H
#define FROTZ_UTILS_H

#include "../zm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "py/misc.h"
#include "py/mphal.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern zm_zm_obj_t *current_frotz_instance;

static inline char *zm_basename(char *path) {
  char *base = strrchr(path, '/');
  return base ? base + 1 : path;
}

/* Yield to the RTOS, running any pending Python callbacks (e.g. term.draw())
 * and then unconditionally handing the scheduler one tick via vTaskDelay(1).
 *
 * mp_hal_delay_ms(ms) alone is not sufficient: it exits as soon as the elapsed
 * time exceeds `ms`, which means it skips vTaskDelay() entirely when
 * mp_handle_pending() callbacks (like term.draw() at ~24 ms) take longer than
 * the requested delay.  The explicit vTaskDelay(1) below guarantees the RTOS
 * idle task always gets CPU time and any active watchdog timers are fed,
 * regardless of how long the callbacks took.
 *
 * Any Python exception raised by a callback is caught by the nlr_push/pop and
 * silently discarded — frotz's C call stack cannot handle Python exceptions. */
static inline void zm_yield(mp_uint_t ms) {
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_hal_delay_ms(ms);
    nlr_pop();
  }

  vTaskDelay(1);
}

static int xgetchar(void) {
  while (1) {
    uint8_t byte;
    int errcode;

    mp_uint_t n = current_frotz_instance->stream_p->read(
        current_frotz_instance->stream_obj, &byte, 1, &errcode);

    if (n != (mp_uint_t)-1 && n > 0) {
      if (byte == '\r') {
        // Enter: echo a newline and return \n for frotz's dumb_getline().
        mp_hal_stdout_tx_strn("\r\n", 2);
        return '\n';
      }
      // Echo printable characters to the display as they are typed.
      if (byte >= 32 && byte < 127) {
        mp_hal_stdout_tx_strn((const char *)&byte, 1);
      }
      return (int)byte;
    }

    // No key available: yield to the RTOS so the WDT gets fed.
    zm_yield(10);
  }
}

static void zm_putchar(char c) {
  // Write exactly 1 character to the active MicroPython stdout stream
  if (c == '\n') {
    char cr = '\r';
    mp_hal_stdout_tx_strn(&cr, 1);
  }

  mp_hal_stdout_tx_strn(&c, 1);
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

static inline FILE *zm_fopen(const char *path, const char *mode) {
  // Convert C strings to MicroPython Objects
  mp_obj_t path_obj = mp_obj_new_str(path, strlen(path));
  mp_obj_t mode_obj = mp_obj_new_str(mode, strlen(mode));

  // Look up the standard 'open' function that Python uses
  // This function knows all about the /flash mount point
  mp_obj_t open_fn = mp_load_global(MP_QSTR_open);

  // Call it: open(path, mode)
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_obj_t file_obj =
        mp_call_function_n_kw(open_fn, 2, 0, (mp_obj_t[]){path_obj, mode_obj});
    nlr_pop();
    return (FILE *)file_obj; // Success: Return the object pointer as our "fd"
  } else {
    // If Python threw an exception (like ENOENT), we catch it here
    return NULL;
  }
}

// --- VFS-Aware Read ---
static inline size_t zm_fread(void *ptr, size_t size, size_t nmemb,
                              FILE *stream) {
  mp_obj_t stream_obj = (mp_obj_t)stream;
  int errcode = 0;

  // mp_stream_rw reads directly from the Python file object into your C buffer
  mp_uint_t bytes_read =
      mp_stream_rw(stream_obj, ptr, size * nmemb, &errcode, MP_STREAM_RW_READ);

  if (errcode != 0 || bytes_read == MP_STREAM_ERROR) {
    return 0; // Read failed
  }

  // fread expects the return value to be the number of ITEMS read, not bytes
  return bytes_read / size;
}

// --- VFS-Aware Seek ---
static inline int zm_fseek(FILE *stream, long offset, int whence) {
  mp_obj_t stream_obj = (mp_obj_t)stream;
  int errcode = 0;

  // whence maps perfectly to MicroPython's seek (0=SET, 1=CUR, 2=END)
  mp_uint_t res = mp_stream_seek(stream_obj, offset, whence, &errcode);

  if (errcode != 0 || res == MP_STREAM_ERROR) {
    return -1; // Seek failed
  }
  return 0; // Success
}

// --- VFS-Aware Tell (often needed by fseek logic) ---
static inline long zm_ftell(FILE *stream) {
  mp_obj_t stream_obj = (mp_obj_t)stream;
  int errcode = 0;

  // Seeking 0 bytes from the CURRENT position returns the absolute offset
  mp_uint_t res = mp_stream_seek(stream_obj, 0, SEEK_CUR, &errcode);

  if (errcode != 0 || res == MP_STREAM_ERROR) {
    return -1L;
  }
  return (long)res;
}

// --- VFS-Aware Close ---
static inline int zm_fclose(FILE *stream) {
  mp_obj_t stream_obj = (mp_obj_t)stream;
  mp_stream_close(stream_obj);
  return 0; // Success
}

#endif
