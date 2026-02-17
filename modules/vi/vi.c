/* vi.c - You can't spell "evil" without "vi".
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 * Copyright 2019 Jarno Mäkipää <jmakip87@gmail.com>
 * Copyright 2026 Vincent (8bitmcu) (MicroPython/VFS Port)
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/vi.html
 * Licensed under 0BSD.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "extmod/vfs.h"
#include "py/gc.h"
#include "py/misc.h"
#include "py/mpconfig.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream.h"

#include "vi_module.h"

extern vi_vi_obj_t *current_vi_instance;

#define FOR_vi
#define CTL(a) a - '@'
#define KEY_UP 0
#define KEY_DOWN 1
#define KEY_RIGHT 2
#define KEY_LEFT 3
#define KEY_PGUP 4
#define KEY_PGDN 5
#define KEY_HOME 6
#define KEY_END 7
#define KEY_INSERT 8
#define KEY_DELETE 9
#define KEY_FN 10 // F1 = KEY_FN+1, F2 = KEY_FN+2, ...
#define KEY_SHIFT (1 << 16)
#define KEY_CTRL (1 << 17)
#define KEY_ALT (1 << 18)

#define ARRAY_LEN(array) (sizeof(array) / sizeof(*array))

#ifndef IUTF8
#define IUTF8 0x00004000
#endif
#ifndef ECHOCTL
#define ECHOCTL 0x00000200
#endif
#ifndef ECHOKE
#define ECHOKE 0x00000800
#endif

#define PROT_READ 0x1    // Page can be read
#define PROT_WRITE 0x2   // Page can be written
#define MAP_SHARED 0x01  // Share changes
#define MAP_PRIVATE 0x02 // Changes are private

#define REG_EXTENDED 0
#define REG_ICASE 1
#define REG_NOMATCH 1

typedef struct {
  int dummy;
} regex_t;

// TODO implement these
inline int regcomp(regex_t *preg, const char *regex, int cflags) { return 0; }
inline int regexec(const regex_t *preg, const char *string, size_t nmatch,
                   void *pmatch, int eflags) {
  return REG_NOMATCH;
}
inline void regfree(regex_t *preg) {}

char toybuf[4096];

struct vi_data {
  char *c, *s;
  char *filename;
  int vi_mode, tabstop, list, cur_col, cur_row, scr_row, drawn_row, drawn_col,
      count0, count1, vi_mov_flag, vi_exit;
  unsigned screen_height, screen_width;
  char vi_reg, *last_search;

  struct str_line {
    int alloc, len;
    char *data;
  } *il;

  size_t screen, cursor;

  struct yank_buf {
    char reg;
    int alloc;
    char *data;
  } yank;

  size_t filesize;
  struct block_list {
    struct block_list *next, *prev;
    struct mem_block {
      size_t size, len;
      enum alloc_flag { MMAP, HEAP, STACK } alloc;
      const char *data;
    } *node;
  } *text;

  struct slice_list {
    struct slice_list *next, *prev;
    struct slice {
      size_t len;
      const char *data;
    } *node;
  } *slices;
};

struct vi_data TT;

struct winsize {
  unsigned short ws_row;    // Number of rows (lines)
  unsigned short ws_col;    // Number of columns (chars)
  unsigned short ws_xpixel; // Horizontal size, pixels (usually ignored)
  unsigned short ws_ypixel; // Vertical size, pixels (usually ignored)
};

struct toys_shim {
  int signal;
} toys = {0};

struct double_list {
  struct double_list *next, *prev;
  char *data;
};

static const char *blank = " \n\r\t";
static const char *specials = ",.:;=-+*/(){}<>[]!@#$%^&|\\?\"\'";

static inline FILE *vfs_fopen(const char *path, const char *mode) {
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

static inline int vfs_open(const char *path, const char *mode) {
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
    return (int)file_obj; // Success: Return the object pointer as our "fd"
  } else {
    // If Python threw an exception (like ENOENT), we catch it here
    return -1;
  }
}

static inline int vfs_fclose(FILE *stream) {
  mp_obj_t file = (mp_obj_t)stream;
  mp_obj_t dest[2];
  mp_load_method(file, MP_QSTR_close, dest);
  mp_call_method_n_kw(0, 0, dest);
  return 0;
}

void vfs_close_obj(mp_obj_t file_obj) {
  mp_obj_t dest[2];
  mp_load_method(file_obj, MP_QSTR_close, dest);
  mp_call_method_n_kw(0, 0, dest);
}

int vfs_rename(const char *old, const char *new) {
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_vfs_rename(mp_obj_new_str(old, strlen(old)),
                  mp_obj_new_str(new, strlen(new)));
    nlr_pop();
    return 0;
  }
  return -1;
}

static inline void vfs_remove(const char *path) {
  nlr_buf_t nlr;
  if (nlr_push(&nlr) == 0) {
    mp_vfs_remove(mp_obj_new_str(path, strlen(path)));
    nlr_pop();
  }
}

static inline int vfs_write(int fd, const void *buf, size_t n) {
  mp_obj_t file_obj =
      (mp_obj_t)fd; // Cast our "fd" back to a MicroPython Object
  int errcode;

  // Get the stream protocol from the file object
  const mp_stream_p_t *stream_p =
      mp_get_stream_raise(file_obj, MP_STREAM_OP_WRITE);

  // Perform the write
  mp_uint_t n_written = stream_p->write(file_obj, buf, n, &errcode);

  if (n_written == (mp_uint_t)-1)
    return -1;
  return (int)n_written;
}

// Replacement for fdlength that works with MicroPython objects
long long vfs_fdlength(int fd) {
  mp_obj_t file_obj = (mp_obj_t)fd;
  mp_obj_t dest[2];

  // Seek to the end (offset 0 from SEEK_END which is 2)
  mp_load_method(file_obj, MP_QSTR_seek, dest);
  mp_call_method_n_kw(
      2, 0,
      (mp_obj_t[]){dest[0], dest[1], mp_obj_new_int(0), mp_obj_new_int(2)});

  // Tell gives us the current position (the end)
  mp_load_method(file_obj, MP_QSTR_tell, dest);
  mp_obj_t size_obj = mp_call_method_n_kw(0, 0, dest);

  // Convert Python Object to C long long
  long long length = (long long)mp_obj_get_int(size_obj);

  // Seek back to the beginning (SEEK_SET is 0)
  mp_load_method(file_obj, MP_QSTR_seek, dest);
  mp_call_method_n_kw(1, 0, (mp_obj_t[]){dest[0], dest[1], mp_obj_new_int(0)});

  return length;
}

static inline int vfs_read(int fd, void *buf, size_t n) {
  int errcode;
  const mp_stream_p_t *stream_p =
      mp_get_stream_raise((mp_obj_t)fd, MP_STREAM_OP_READ);
  return (int)stream_p->read((mp_obj_t)fd, buf, n, &errcode);
}

int xprintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char buf[256];
  int len = vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (len > 0)
    mp_hal_stdout_tx_strn(buf, len);
  return len;
}

#define error_exit(fmt, ...)                                                   \
  mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT(fmt), ##__VA_ARGS__)

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream) {
  if (lineptr == NULL || n == NULL || stream == NULL)
    return -1;

  if (*lineptr == NULL || *n == 0) {
    *n = 128;
    *lineptr = m_new(char, *n);
    if (!*lineptr)
      return -1;
  }

  int c;
  size_t pos = 0;

  while ((c = fgetc(stream)) != EOF) {
    // Safety Check: Ensure space for current char + null terminator
    if (pos + 1 >= *n) {
      size_t new_n =
          (*n < 64) ? 128
                    : (*n * 2); // Exponential growth to prevent fragmentation

      // Use m_renew for GC-tracked realloc
      char *new_ptr = m_renew_maybe(char, *lineptr, *n, new_n, true);

      if (!new_ptr) {
        // If we run out of memory, we don't crash.
        // We null-terminate what we have and return it.
        (*lineptr)[pos] = '\0';
        return (pos > 0) ? (ssize_t)pos : -1;
      }

      *lineptr = new_ptr;
      *n = new_n;
    }

    ((unsigned char *)(*lineptr))[pos++] = (unsigned char)c;
    if (c == delim)
      break;
  }

  if (pos == 0)
    return -1;

  (*lineptr)[pos] = '\0';
  return (ssize_t)pos;
}

// Die unless we can open/create a file, returning FILE *.
FILE *xfopen(char *path, char *mode) {
  FILE *f = vfs_fopen(path, mode);
  if (!f) {
    // We use mp_raise_msg to pass control back to the MicroPython REPL
    error_exit("xopen");
  }
  return f;
}

void vi_xwrite(int fd, const void *buf, size_t len) {
  size_t count = 0;
  const char *ptr = (const char *)buf;

  while (count < len) {
    // Attempt to write the remaining buffer
    int i = vfs_write(fd, ptr + count, len - count);

    // Error handling
    if (i < 1) {
      // Instead of error_exit, we raise a MicroPython exception.
      // This is caught by your NLR guard, returning the user to the REPL
      // instead of rebooting the device.
      mp_raise_msg(&mp_type_OSError,
                   MP_ERROR_TEXT("vi: write failed (disk full or VFS error)"));
    }
    count += i;
  }
}

// Die unless we can allocate memory.
void *vi_xmalloc(size_t size) {
  // Allocate from MicroPython's GC-tracked heap
  void *ret = m_malloc_maybe(size);

  if (!ret) {
    // Instead of a hard crash, we can try to trigger a GC collection
    // and try one last time.
    gc_collect();
    ret = m_malloc_maybe(size);
  }

  if (!ret) {
    // If it STILL fails, we use MicroPython's controlled emergency exit
    // This will be caught by the NLR guard we added to your constructor!
    mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("vi: out of memory"));
  }

  return ret;
}

// Die unless we can allocate enough space to sprintf() into.
char *xmprintf(char *format, ...) {
  va_list va, va2;
  int len;
  char *ret;

  va_start(va, format);
  va_copy(va2, va);

  // How long is it?
  len = vsnprintf(0, 0, format, va) + 1;
  va_end(va);

  // Allocate and do the sprintf()
  ret = vi_xmalloc(len);
  vsnprintf(ret, len, format, va2);
  va_end(va2);

  return ret;
}

// Die unless we can change the size of an existing allocation, possibly
// moving it.  (Notice different arguments from libc function.)
void *vi_xrealloc(void *ptr, size_t old_size, size_t new_size) {
  void *ret = m_renew_maybe(char, ptr, old_size, new_size, true);
  if (!ret) {
    mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("vi: realloc failed"));
  }
  return ret;
}

int vi_wcwidth(uint32_t u) {
  if (u == 0)
    return 0;
  if (u < 32 || (u >= 0x7f && u < 0xa0))
    return -1; // Control characters
  return 1;    // Everything else is 1 cell wide
}

int utf8towc(unsigned *wc, char *str, unsigned len) {
  unsigned result, mask, first;
  char *s, c;

  // fast path ASCII
  if (len && *str < 128)
    return !!(*wc = *str);

  result = first = *(s = str++);
  if (result < 0xc2 || result > 0xf4)
    return -1;
  for (mask = 6; (first & 0xc0) == 0xc0; mask += 5, first <<= 1) {
    if (!--len)
      return -2;
    if (((c = *(str++)) & 0xc0) != 0x80)
      return -1;
    result = (result << 6) | (c & 0x3f);
  }
  result &= (1 << mask) - 1;
  c = str - s;

  // Avoid overlong encodings
  if (result < (unsigned[]){0x80, 0x800, 0x10000}[c - 2])
    return -1;

  // Limit unicode so it can't encode anything UTF-16 can't.
  if (result > 0x10ffff || (result >= 0xd800 && result <= 0xdfff))
    return -1;
  *wc = result;

  return str - s;
}

// Show width many columns, negative means from right edge, out=0 just measure
// if escout, send it unprintable chars, otherwise pass through raw data.
// Returns width in columns, moves *str to end of data consumed.
int crunch_str(char **str, int width, FILE *out, char *escmore,
               int (*escout)(FILE *out, int cols, int wc)) {
  int columns = 0, col, bytes;
  char *start, *end;
  unsigned wc;

  for (end = start = *str; *end; columns += col, end += bytes) {
    if ((bytes = utf8towc(&wc, end, 4)) > 0 && (col = vi_wcwidth(wc)) >= 0) {
      if (!escmore || wc > 255 || !strchr(escmore, wc)) {
        if (width - columns < col)
          break;
        if (out)
          mp_hal_stdout_tx_strn(end, bytes);

        continue;
      }
    }

    if (bytes < 1) {
      bytes = 1;
      wc = *end;
    }
    col = width - columns;
    if (col < 1)
      break;
    if (escout) {
      if ((col = escout(out, col, wc)) < 0)
        break;
    } else if (out)
      mp_hal_stdout_tx_strn(end, 1);
  }
  *str = end;

  return columns;
}

// standard escapes: ^X if <32, <XX> if invalid UTF8, U+XXXX if UTF8 !iswprint()
int crunch_escape(FILE *out, int cols, int wc) {
  char buf[11];
  int rc;

  if (wc < ' ')
    rc = sprintf(buf, "^%c", '@' + wc);
  else if (wc < 256)
    rc = sprintf(buf, "<%02X>", wc);
  else
    rc = sprintf(buf, "U+%04X", wc);

  if (rc > cols)
    buf[rc = cols] = 0;
  if (out)
    fputs(buf, out);

  return rc;
}

// Return line of text from file.
char *xgetdelim(FILE *fp, int delim) {
  char *new = 0;
  size_t len = 0;
  long ll;

  if (1 > (ll = getdelim(&new, &len, delim, fp))) {
    if (new) {
      m_free(new);
      new = 0;

      if (ferror(fp)) {
        mp_raise_msg(&mp_type_OSError,
                     MP_ERROR_TEXT("vi: I/O error during read"));
      }
      return NULL;
    }
  }

  return new;
}

char *xgetline(FILE *fp) {
  // xgetdelim already uses m_renew_maybe and handles GC allocation
  char *line = xgetdelim(fp, '\n');

  if (line) {
    size_t len = strlen(line);
    // Chomp: Remove \n and \r from the end of the string
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[--len] = '\0';
    }
  }

  return line;
}

void xputsl(char *s, int len) { mp_hal_stdout_tx_strn(s, 1); }

// Append to list in-order (*list unchanged unless empty, ->prev is new node)
void dlist_add_nomalloc(struct double_list **list, struct double_list *new) {
  if (*list) {
    new->next = *list;
    new->prev = (*list)->prev;
    (*list)->prev->next = new;
    (*list)->prev = new;
  } else
    *list = new->next = new->prev = new;
}

// Add an entry to the end of a doubly linked list
struct double_list *dlist_add(struct double_list **list, char *data) {
  struct double_list *new = vi_xmalloc(sizeof(struct double_list));

  new->data = data;
  dlist_add_nomalloc(list, new);

  return new;
}

// Remove first item from &list and return it
void *dlist_pop(void *list) {
  struct double_list **pdlist = (struct double_list **)list, *dlist = *pdlist;

  if (!dlist)
    return 0;
  if (dlist->next == dlist)
    *pdlist = 0;
  else {
    if (dlist->next)
      dlist->next->prev = dlist->prev;
    if (dlist->prev)
      dlist->prev->next = dlist->next;
    *pdlist = dlist->next;
  }

  return dlist;
}

// Return the first item from the list, advancing the list (which must be called
// as &list)
void *llist_pop(void *list) {
  void **llist = list, **next;

  if (!list || !*llist)
    return 0;
  next = (void **)*llist;
  *llist = *next;

  return next;
}

void llist_free_double(void *node) {
  struct double_list *d = (struct double_list *)node;

  if (d) {
    // Free the string data (the line content)
    if (d->data) {
      // Using m_free because we might not have the original
      // allocation size handy in this context.
      m_free(d->data);
    }

    // Free the node structure itself
    // Note: m_free is used here to match your likely
    // m_new/m_malloc calls for the node.
    m_free(d);
  }
}

// Call a function (such as free()) on each element of a linked list.
void llist_traverse(void *list, void (*using)(void *node)) {
  void *old = list;

  while (list) {
    void *pop = llist_pop(&list);
    using(pop);

    // End doubly linked list too.
    if (old == list)
      break;
  }
}

int munmap(void *addr, size_t length) {
  // On ESP32, if we mapped something to HEAP, we should have used free().
  // If ToyBox calls munmap on a HEAP pointer, that's a bug in our port.
  return 0;
}

// Die unless we can allocate a copy of this string.
char *vi_xstrdup(const char *s) {
  size_t len = strlen(s) + 1;
  char *new = m_new(char, len);
  if (new)
    memcpy(new, s, len);
  return new;
}

// xputs with no newline
void xputsn(char *s) { xputsl(s, strlen(s)); }

// Reset terminal to known state, saving copy of old state if old != NULL.
int set_terminal(int fd, int raw, int speed, struct termios *old) {
  struct termios tio;
  int i = tcgetattr(fd, &tio);

  // Fetch local copy of old terminfo, and copy struct contents to *old if set
  if (i)
    return i;
  if (old)
    *old = tio;

  if (!raw) {
    // Put the "cooked" bits back.

    // Convert CR to NL on input, UTF8 aware backspace, Any key unblocks input.
    tio.c_iflag |= ICRNL | IUTF8 | IXANY;

    // Output appends CR to NL and does magic undocumented postprocessing.
    tio.c_oflag |= OPOST | ONLCR;

    // 8 bit chars, enable receiver.
    tio.c_cflag |= CS8 | CREAD;

    // Generate signals, input entire line at once, echo output erase,
    // line kill, escape control characters with ^, erase line char at a time
    // "extended" behavior: ctrl-V quotes next char, ctrl-R reprints unread line
    // ctrl-W erases word
    tio.c_lflag |=
        ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
  }

  return tcsetattr(fd, TCSAFLUSH, &tio);
}

struct scan_key_list {
  int key;
  char *seq;
} const scan_key_list[] = {
    {KEY_UP, "\e[A"},
    {KEY_DOWN, "\e[B"},
    {KEY_RIGHT, "\e[C"},
    {KEY_LEFT, "\e[D"},

    {KEY_UP | KEY_SHIFT, "\e[1;2A"},
    {KEY_DOWN | KEY_SHIFT, "\e[1;2B"},
    {KEY_RIGHT | KEY_SHIFT, "\e[1;2C"},
    {KEY_LEFT | KEY_SHIFT, "\e[1;2D"},

    {KEY_UP | KEY_ALT, "\e[1;3A"},
    {KEY_DOWN | KEY_ALT, "\e[1;3B"},
    {KEY_RIGHT | KEY_ALT, "\e[1;3C"},
    {KEY_LEFT | KEY_ALT, "\e[1;3D"},

    {KEY_UP | KEY_CTRL, "\e[1;5A"},
    {KEY_DOWN | KEY_CTRL, "\e[1;5B"},
    {KEY_RIGHT | KEY_CTRL, "\e[1;5C"},
    {KEY_LEFT | KEY_CTRL, "\e[1;5D"},

    // VT102/VT220 escapes.
    {KEY_HOME, "\e[1~"},
    {KEY_HOME | KEY_CTRL, "\e[1;5~"},
    {KEY_INSERT, "\e[2~"},
    {KEY_DELETE, "\e[3~"},
    {KEY_END, "\e[4~"},
    {KEY_END | KEY_CTRL, "\e[4;5~"},
    {KEY_PGUP, "\e[5~"},
    {KEY_PGDN, "\e[6~"},
    // "Normal" "PC" escapes (xterm).
    {KEY_HOME, "\eOH"},
    {KEY_END, "\eOF"},
    // "Application" "PC" escapes (gnome-terminal).
    {KEY_HOME, "\e[H"},
    {KEY_END, "\e[F"},
    {KEY_HOME | KEY_CTRL, "\e[1;5H"},
    {KEY_END | KEY_CTRL, "\e[1;5F"},

    {KEY_FN + 1, "\eOP"},
    {KEY_FN + 2, "\eOQ"},
    {KEY_FN + 3, "\eOR"},
    {KEY_FN + 4, "\eOS"},
    {KEY_FN + 5, "\e[15~"},
    {KEY_FN + 6, "\e[17~"},
    {KEY_FN + 7, "\e[18~"},
    {KEY_FN + 8, "\e[19~"},
    {KEY_FN + 9, "\e[20~"},
};

// Scan stdin for a keypress, parsing known escape sequences, including
// responses to screen size queries.
// Blocks for timeout_ms milliseconds, 0=return immediately, -1=wait forever.
// Returns 0-255=literal, -1=EOF, -2=TIMEOUT, -3=RESIZE, 256+= a KEY_ constant.
// Scratch space is necessary because last char of !seq could start new seq.
// Zero out first byte of scratch before first call to scan_key.

int scan_key_getsize(char *scratch, int timeout_ms, unsigned *xx,
                     unsigned *yy) {
  int maybe, i, j;
  char *test;

  for (;;) {
    maybe = 0;
    if (*scratch > 0) {
      int pos[6];
      unsigned x, y;

      memset(pos, 0, sizeof(pos));
      scratch[*scratch + 1] = 0;

      sscanf(scratch + 1, "\x1b%n[%n%3u%n;%n%3u%nR%n", pos, pos + 1, &y,
             pos + 2, pos + 3, &x, pos + 4, pos + 5);

      if (pos[5] > 0) {
        *scratch = 0;
        if (xx)
          *xx = x;
        if (yy)
          *yy = y;
        return -3;
      } else {
        for (i = 0; i < 6; i++) {
          if (pos[i] == *scratch)
            maybe = 1;
        }
      }

      for (i = 0; i < ARRAY_LEN(scan_key_list); i++) {
        test = scan_key_list[i].seq;
        for (j = 0; j < *scratch; j++) {
          if (scratch[j + 1] != test[j])
            break;
        }
        if (j == *scratch) {
          maybe = 1;
          if (!test[j]) {
            *scratch = 0;
            return 256 + scan_key_list[i].key;
          }
        }
      }

      if (!maybe)
        break;
    }

    int wait_ms = maybe ? 30 : timeout_ms;
    uint32_t start = mp_hal_ticks_ms();
    int c = -1;

    while (c == -1) {
      uint8_t byte;
      int errcode;

      mp_uint_t n = current_vi_instance->stream_p->read(
          current_vi_instance->stream_obj, &byte, 1, &errcode);

      if (n != (mp_uint_t)-1 && n > 0) {
        c = byte;
      } else {
        if (wait_ms != -1 && (mp_hal_ticks_ms() - start) > (uint32_t)wait_ms) {
          goto break_loop;
        }
        mp_handle_pending(true);
        mp_hal_delay_ms(1);
      }
    }

    if (c == 3)
      return -1;

    if (*scratch < 15) { // Prevent overflow
      scratch[(unsigned char)++(*scratch)] = (char)c;
    }
  }

break_loop:
  if (*scratch == 0)
    return -2;

  // Use a local unsigned variable to avoid char/int sign issues
  unsigned char *u_scratch = (unsigned char *)scratch;
  int key = u_scratch[1];

  // Shift everything left
  int remaining = u_scratch[0] - 1;
  if (remaining > 0) {
    memmove(scratch + 1, scratch + 2, remaining);
  }
  u_scratch[0] = (char)remaining;

  return key;
}

// Wrapper that ignores results from ANSI probe to update screensize.
// Otherwise acts like scan_key_getsize().
int scan_key(char *scratch, int timeout_ms) {
  return scan_key_getsize(scratch, timeout_ms, NULL, NULL);
}

void tty_reset(void) {
  set_terminal(0, 0, 0, 0);
  xputsn("\e[?25h\e[0m\e[999H\e[K");
}

// If you call set_terminal(), use sigatexit(tty_sigreset);
void tty_sigreset(int i) {
  tty_reset();
  _exit(i ? 128 + i : 0);
}

void start_redraw(unsigned *width, unsigned *height) {
  // TODO
  *width = 40;
  *height = 16;

  TT.screen_width = *width;
  TT.screen_height = *height - 1;

  xputsn("\e[H\e[J");
}

///////////////////////
// ACTUAL VI CODE //////
// /////////////////////

// get utf8 length and width at same time
static int utf8_lnw(int *width, char *s, int bytes) {
  unsigned wc;
  int length = 1;

  if (*s == '\t')
    *width = TT.tabstop;
  else {
    length = utf8towc(&wc, s, bytes);
    if (length < 1)
      length = 0, *width = 0;
    else
      *width = vi_wcwidth(wc);
  }
  return length;
}

static int utf8_dec(char key, char *utf8_scratch, int *sta_p) {
  int len = 0;
  char *c = utf8_scratch;
  c[*sta_p] = key;
  if (!(*sta_p))
    *c = key;
  if (*c < 0x7F)
    return *sta_p = 1;
  if ((*c & 0xE0) == 0xc0)
    len = 2;
  else if ((*c & 0xF0) == 0xE0)
    len = 3;
  else if ((*c & 0xF8) == 0xF0)
    len = 4;
  else
    return *sta_p = 0;

  if (++*sta_p == 1)
    return 0;
  if ((c[*sta_p - 1] & 0xc0) != 0x80)
    return *sta_p = 0;

  if (*sta_p == len)
    return !(c[(*sta_p)] = 0);

  return 0;
}

static char *utf8_last(char *str, int size) {
  char *end = str + size;
  int pos = size, len, width = 0;

  for (; pos >= 0; end--, pos--) {
    len = utf8_lnw(&width, end, size - pos);
    if (len && width)
      return end;
  }
  return 0;
}

struct double_list *dlist_add_before(struct double_list **head,
                                     struct double_list **list, char *data) {
  struct double_list *new = vi_xmalloc(sizeof(struct double_list));
  new->data = data;
  if (*list == *head)
    *head = new;

  dlist_add_nomalloc(list, new);
  return new;
}

struct double_list *dlist_add_after(struct double_list **head,
                                    struct double_list **list, char *data) {
  struct double_list *new = vi_xmalloc(sizeof(struct double_list));
  new->data = data;

  if (*list) {
    new->prev = *list;
    new->next = (*list)->next;
    (*list)->next->prev = new;
    (*list)->next = new;
  } else
    *head = *list = new->next = new->prev = new;
  return new;
}

// str must be already allocated
// ownership of allocated data is moved
// data, pre allocated data
// offset, offset in whole text
// size, data allocation size of given data
// len, length of the string
// type, define allocation type for cleanup purposes at app exit
static int insert_str(const char *data, size_t offset, size_t size, size_t len,
                      enum alloc_flag type) {
  struct mem_block *b = vi_xmalloc(sizeof(struct mem_block));
  struct slice *next = vi_xmalloc(sizeof(struct slice));
  struct slice_list *s = TT.slices;

  b->size = size;
  b->len = len;
  b->alloc = type;
  b->data = data;
  next->len = len;
  next->data = data;

  // mem blocks can be just added unordered
  TT.text = (struct block_list *)dlist_add((struct double_list **)&TT.text,
                                           (char *)b);

  if (!s) {
    TT.slices = (struct slice_list *)dlist_add(
        (struct double_list **)&TT.slices, (char *)next);
  } else {
    size_t pos = 0;
    // search insertation point for slice
    do {
      if (pos <= offset && pos + s->node->len > offset)
        break;
      pos += s->node->len;
      s = s->next;
      if (s == TT.slices)
        return -1; // error out of bounds
    } while (1);
    // need to cut previous slice into 2 since insert is in middle
    if (pos + s->node->len > offset && pos != offset) {
      struct slice *tail = vi_xmalloc(sizeof(struct slice));
      tail->len = s->node->len - (offset - pos);
      tail->data = s->node->data + (offset - pos);
      s->node->len = offset - pos;
      // pos = offset;
      s = (struct slice_list *)dlist_add_after(
          (struct double_list **)&TT.slices, (struct double_list **)&s,
          (char *)tail);

      s = (struct slice_list *)dlist_add_before(
          (struct double_list **)&TT.slices, (struct double_list **)&s,
          (char *)next);
    } else if (pos == offset) {
      // insert before
      s = (struct slice_list *)dlist_add_before(
          (struct double_list **)&TT.slices, (struct double_list **)&s,
          (char *)next);
    } else {
      // insert after
      s = (void *)dlist_add_after((void *)&TT.slices, (void *)&s, (void *)next);
    }
  }
  return 0;
}

// this will not free any memory
// will only create more slices depending on position
static int cut_str(size_t offset, size_t len) {
  struct slice_list *e, *s = TT.slices;
  size_t end = offset + len;
  size_t epos, spos = 0;

  if (!s)
    return -1;

  // find start and end slices
  for (;;) {
    if (spos <= offset && spos + s->node->len > offset)
      break;
    spos += s->node->len;
    s = s->next;

    if (s == TT.slices)
      return -1; // error out of bounds
  }

  for (e = s, epos = spos;;) {
    if (epos <= end && epos + e->node->len > end)
      break;
    epos += e->node->len;
    e = e->next;

    if (e == TT.slices)
      return -1; // error out of bounds
  }

  for (;;) {
    if (spos == offset && (end >= spos + s->node->len)) {
      // cut full
      spos += s->node->len;
      offset += s->node->len;
      s = dlist_pop(&s);
      if (s == TT.slices)
        TT.slices = s->next;

    } else if (spos < offset && (end >= spos + s->node->len)) {
      // cut end
      size_t clip = s->node->len - (offset - spos);
      offset = spos + s->node->len;
      spos += s->node->len;
      s->node->len -= clip;
    } else if (spos == offset && s == e) {
      // cut begin
      size_t clip = end - offset;
      s->node->len -= clip;
      s->node->data += clip;
      break;
    } else {
      // cut middle
      struct slice *tail = vi_xmalloc(sizeof(struct slice));
      size_t clip = end - offset;
      tail->len = s->node->len - (offset - spos) - clip;
      tail->data = s->node->data + (offset - spos) + clip;
      s->node->len = offset - spos; // wrong?
      s = (struct slice_list *)dlist_add_after(
          (struct double_list **)&TT.slices, (struct double_list **)&s,
          (char *)tail);
      break;
    }
    if (s == e)
      break;

    s = s->next;
  }

  return 0;
}
static int modified() {
  if (TT.text->next != TT.text->prev)
    return 1;
  if (TT.slices->next != TT.slices->prev)
    return 1;
  if (!TT.text || !TT.slices)
    return 0;
  if (!TT.text->node || !TT.slices->node)
    return 0;
  if (TT.text->node->alloc != MMAP)
    return 1;
  if (TT.text->node->len != TT.slices->node->len)
    return 1;
  if (!TT.text->node->len)
    return 1;
  return 0;
}

// find offset position in slices
static struct slice_list *slice_offset(size_t *start, size_t offset) {
  struct slice_list *s = TT.slices;
  size_t spos = 0;

  // find start
  for (; s;) {
    if (spos <= offset && spos + s->node->len > offset)
      break;

    spos += s->node->len;
    s = s->next;

    if (s == TT.slices)
      s = 0; // error out of bounds
  }
  if (s)
    *start = spos;
  return s;
}

static size_t text_strchr(size_t offset, char c) {
  struct slice_list *s = TT.slices;
  size_t epos, spos = 0;
  int i = 0;

  // find start
  if (!(s = slice_offset(&spos, offset)))
    return SIZE_MAX;

  i = offset - spos;
  epos = spos + i;
  do {
    for (; i < s->node->len; i++, epos++)
      if (s->node->data[i] == c)
        return epos;
    s = s->next;
    i = 0;
  } while (s != TT.slices);

  return SIZE_MAX;
}

static size_t text_strrchr(size_t offset, char c) {
  struct slice_list *s = TT.slices;
  size_t epos, spos = 0;
  int i = 0;

  // find start
  if (!(s = slice_offset(&spos, offset)))
    return SIZE_MAX;

  i = offset - spos;
  epos = spos + i;
  do {
    for (; i >= 0; i--, epos--)
      if (s->node->data[i] == c)
        return epos;
    s = s->prev;
    i = s->node->len - 1;
  } while (s != TT.slices->prev); // tail

  return SIZE_MAX;
}

static size_t text_filesize() {
  struct slice_list *s = TT.slices;
  size_t pos = 0;

  if (s)
    do {
      pos += s->node->len;
      s = s->next;
    } while (s != TT.slices);

  return pos;
}

static char text_byte(size_t offset) {
  struct slice_list *s = TT.slices;
  size_t spos = 0;

  // find start
  if (!(s = slice_offset(&spos, offset)))
    return 0;
  return s->node->data[offset - spos];
}

// utf-8 codepoint -1 if not valid, 0 if out_of_bounds, len if valid
// copies data to dest if dest is not 0
static int text_codepoint(char *dest, size_t offset) {
  char scratch[8] = {0};
  int state = 0, finished = 0;

  for (; !(finished = utf8_dec(text_byte(offset), scratch, &state)); offset++)
    if (!state)
      return -1;

  if (!finished && !state)
    return -1;
  if (dest)
    memcpy(dest, scratch, 8);

  return strlen(scratch);
}

static size_t text_sol(size_t offset) {
  size_t pos;

  if (!TT.filesize || !offset)
    return 0;
  else if (TT.filesize <= offset)
    return TT.filesize - 1;
  else if ((pos = text_strrchr(offset - 1, '\n')) == SIZE_MAX)
    return 0;
  else if (pos < offset)
    return pos + 1;
  return offset;
}

static size_t text_eol(size_t offset) {
  if (!TT.filesize)
    offset = 1;
  else if (TT.filesize <= offset)
    return TT.filesize - 1;
  else if ((offset = text_strchr(offset, '\n')) == SIZE_MAX)
    return TT.filesize - 1;
  return offset;
}

static size_t text_nsol(size_t offset) {
  offset = text_eol(offset);
  if (text_byte(offset) == '\n')
    offset++;
  if (offset >= TT.filesize)
    offset--;
  return offset;
}

static size_t text_psol(size_t offset) {
  offset = text_sol(offset);
  if (offset)
    offset--;
  if (offset && text_byte(offset - 1) != '\n')
    offset = text_sol(offset - 1);
  return offset;
}

static size_t text_getline(char *dest, size_t offset, size_t max_len) {
  struct slice_list *s = TT.slices;
  size_t end, spos = 0;
  int i, j = 0;

  if (dest)
    *dest = 0;

  if (!s)
    return 0;
  if ((end = text_strchr(offset, '\n')) == SIZE_MAX)
    if ((end = TT.filesize) > offset + max_len)
      return 0;

  // find start
  if (!(s = slice_offset(&spos, offset)))
    return 0;

  i = offset - spos;
  j = end - offset + 1;
  if (dest)
    do {
      for (; i < s->node->len && j; i++, j--, dest++)
        *dest = s->node->data[i];
      s = s->next;
      i = 0;
    } while (s != TT.slices && j);

  if (dest)
    *dest = 0;

  return end - offset;
}

// copying is needed when file has lot of inserts that are
// just few char long, but not always. Advanced search should
// check big slices directly and just copy edge cases.
// Also this is only line based search multiline
// and regexec should be done instead.
static size_t text_strstr(size_t offset, char *str, int dir) {
  size_t bytes, pos = offset;
  char *s = 0;

  do {
    bytes = text_getline(toybuf, pos, ARRAY_LEN(toybuf));
    if (!bytes)
      pos += (dir ? 1 : -1); // empty line
    else if ((s = strstr(toybuf, str)))
      return pos + (s - toybuf);
    else {
      if (!dir)
        pos -= bytes;
      else
        pos += bytes;
    }
  } while (pos < (dir ? 0 : TT.filesize));

  return SIZE_MAX;
}

static void block_list_free(void *node) {
  struct block_list *d = (struct block_list *)node;
  if (!d)
    return;

  if (d->node) {
    // Handle the actual data buffer
    if (d->node->alloc == HEAP) {
      // Memory was allocated via m_new/vi_xmalloc
      if (d->node->data) {
        m_free((void *)d->node->data);
      }
    } else if (d->node->alloc == MMAP) {
// On ESP32, if you aren't using a real mmap bridge,
// this case might be unreachable or need custom VFS handling.
// For now, we treat it as a safety no-op or a standard free.
#ifdef __linux__
      munmap((void *)d->node->data, d->node->size);
#else
      // If you 'mmaped' by just reading into a buffer, free it:
      if (d->node->data)
        m_free((void *)d->node->data);
#endif
    }

    // Free the node structure (metadata)
    m_free(d->node);
  }

  // Free the list wrapper itself
  m_free(d);
}

static void show_error(char *fmt, ...) {
  va_list va;

  xprintf("\a\e[%dH\e[41m\e[37m\e[K\e[1m", TT.screen_height + 1);

  va_start(va, fmt);
  mp_vprintf(&mp_plat_print, fmt, va);
  va_end(va);

  xprintf("\e[0m");

  mp_hal_stdout_tx_strn("\r", 1);

  // Now wait for the key
  int c = -1;
  while (c == -1) {
    uint8_t byte;
    int errcode;
    mp_uint_t n = current_vi_instance->stream_p->read(
        current_vi_instance->stream_obj, &byte, 1, &errcode);

    if (n != (mp_uint_t)-1 && n > 0) {
      c = byte;
    }

    mp_handle_pending(true);
    mp_hal_delay_ms(1);
  }
}

static void linelist_unload() {
  // Free the slice list (metadata about line fragments)
  if (TT.slices) {
    llist_traverse((void *)TT.slices, llist_free_double);
    TT.slices = NULL;
  }

  // Free the actual text blocks (the document data)
  if (TT.text) {
    llist_traverse((void *)TT.text, block_list_free);
    TT.text = NULL;
  }

  // Reset file-state metadata
  TT.filesize = 0;
  TT.cursor = 0;
}

static void linelist_load(char *filename, int ignore_missing) {
  int fd;
  long long size;

  if (!filename)
    filename = TT.filename;
  if (!filename) {
    insert_str(vi_xstrdup("\n"), 0, 1, 1, HEAP);
    return;
  }

  fd = vfs_open(filename, "r");
  if (fd == -1) {
    if (!ignore_missing)
      show_error("Couldn't open \"%s\"", filename);
    insert_str(vi_xstrdup("\n"), 0, 1, 1, HEAP);
    return;
  }

  size = vfs_fdlength(fd);
  if (size > 0) {
    // Use MicroPython's managed allocator
    // m_new_maybe returns NULL instead of raising a fatal exception
    char *buf = m_new_maybe(char, size);

    if (buf) {
      vfs_read(fd, buf, size);
      // HEAP flag here tells vi to 'own' this m_new pointer
      insert_str(buf, 0, size, size, HEAP);
      TT.filesize = text_filesize();
    } else {
      show_error("File too large for available RAM");
      insert_str(vi_xstrdup("\n"), 0, 1, 1, HEAP);
    }
  } else {
    // Empty file case
    insert_str(vi_xstrdup("\n"), 0, 1, 1, HEAP);
  }

  vfs_close_obj((mp_obj_t)fd);
}

static int write_file(char *filename) {
  struct slice_list *s = TT.slices;
  struct stat st;
  int fd = 0;

  if (!modified()) {
    show_error("No changes need to be saved");
    return 0; // Return early! Don't proceed to unload memory.
  }

  if (!filename)
    filename = TT.filename;
  if (!filename) {
    show_error("No file name");
    return -1;
  }

  if (stat(filename, &st) == -1)
    st.st_mode = 0644;

  sprintf(toybuf, "%s.swp", filename);

  if ((fd = vfs_open(toybuf, "wb")) == -1) {
    show_error("Couldn't open \"%s\" for writing: %s", toybuf, strerror(errno));
    return -1;
  }

  if (s) {
    do {
      vi_xwrite(fd, (void *)s->node->data, s->node->len);
      s = s->next;
    } while (s != TT.slices);
  }

  vfs_close_obj((mp_obj_t)fd);
  vfs_remove(filename);

  if (!vfs_rename(toybuf, filename)) {
    linelist_unload();
    linelist_load(filename, 0);
    return 1;
  }

  return 0;
}

// jump into valid offset index
// and valid utf8 codepoint
static void check_cursor_bounds() {
  char buf[8] = {0};
  int len, width = 0;

  if (!TT.filesize)
    TT.cursor = 0;
  for (;;) {
    if (TT.cursor < 1) {
      TT.cursor = 0;
      return;
    } else if (TT.cursor >= TT.filesize - 1) {
      TT.cursor = TT.filesize - 1;
      return;
    }
    // if we are not in valid data try jump over
    if ((len = text_codepoint(buf, TT.cursor)) < 1)
      TT.cursor--;
    else if (utf8_lnw(&width, buf, len) && width)
      break;
    else
      TT.cursor--; // combine char jump over
  }
}

// TT.vi_mov_flag is used for special cases when certain move
// acts differently depending is there DELETE/YANK or NOP
// Also commands such as G does not default to count0=1
// 0x1 = Command needs argument (f,F,r...)
// 0x2 = Move 1 right on yank/delete/insert (e, $...)
// 0x4 = yank/delete last line fully
// 0x10000000 = redraw after cursor needed
// 0x20000000 = full redraw needed
// 0x40000000 = count0 not given
// 0x80000000 = move was reverse

// TODO rewrite the logic, difficulties counting lines
// and with big files scroll should not rely in knowing
// absoluteline numbers

// TODO search yank buffer by register
// TODO yanks could be separate slices so no need to copy data
// now only supports default register
static int vi_yank(char reg, size_t from, int flags) {
  size_t start = from, end = TT.cursor;
  char *str;

  if (TT.vi_mov_flag & 0x80000000)
    start = TT.cursor, end = from;
  else
    TT.cursor = start;

  size_t required = end - start + 1; // +1 for null terminator

  if (TT.yank.alloc < required) {
    // Round up to nearest 512 or 1024 to reduce frequency of reallocs
    size_t new_bounds = (required + 511) & ~511;

    // CORRECT CALL: Pass the pointer, the OLD size, and the NEW size
    void *new_ptr = vi_xrealloc(TT.yank.data, TT.yank.alloc, new_bounds);

    if (!new_ptr) {
      show_error("Yank failed: Out of memory");
      return 0;
    }

    TT.yank.data = new_ptr;
    TT.yank.alloc = new_bounds;
  }

  // Clear buffer safely before copy
  if (TT.yank.data) {
    memset(TT.yank.data, 0, TT.yank.alloc);

    // Performance optimization: Using a loop with text_byte is slow.
    // If your engine supports it, a block copy is better, but for now:
    for (str = TT.yank.data; start < end; start++, str++)
      *str = text_byte(start);

    *str = 0;
  }

  return 1;
}

static int vi_delete(char reg, size_t from, int flags) {
  size_t start = from, end = TT.cursor;

  vi_yank(reg, from, flags);

  if (TT.vi_mov_flag & 0x80000000)
    start = TT.cursor, end = from;

  // pre adjust cursor move one right until at next valid rune
  if (TT.vi_mov_flag & 2) {
    // TODO
  }
  // do slice cut
  cut_str(start, end - start);

  // cursor is at start at after delete
  TT.cursor = start;
  TT.filesize = text_filesize();
  // find line start by strrchr(/n) ++
  // set cur_col with crunch_n_str maybe?
  TT.vi_mov_flag |= 0x30000000;

  return 1;
}

static int vi_change(char reg, size_t to, int flags) {
  vi_delete(reg, to, flags);
  TT.vi_mode = 2;
  return 1;
}

static int cur_left(int count0, int count1, char *unused) {
  int count = count0 * count1;

  TT.vi_mov_flag |= 0x80000000;
  for (; count && TT.cursor; count--) {
    TT.cursor--;
    if (text_byte(TT.cursor) == '\n')
      TT.cursor++;
    check_cursor_bounds();
  }
  return 1;
}

static int cur_right(int count0, int count1, char *unused) {
  int count = count0 * count1, len, width = 0;
  char buf[8] = {0};

  for (; count; count--) {
    len = text_codepoint(buf, TT.cursor);

    if (*buf == '\n')
      break;
    else if (len > 0)
      TT.cursor += len;
    else
      TT.cursor++;

    for (; TT.cursor < TT.filesize;) {
      if ((len = text_codepoint(buf, TT.cursor)) < 1) {
        TT.cursor++; // we are not in valid data try jump over
        continue;
      }

      if (utf8_lnw(&width, buf, len) && width)
        break;
      else
        TT.cursor += len;
    }
  }
  check_cursor_bounds();
  return 1;
}

// TODO column shift
static int cur_up(int count0, int count1, char *unused) {
  int count = count0 * count1;

  for (; count--;)
    TT.cursor = text_psol(TT.cursor);
  TT.vi_mov_flag |= 0x80000000;
  check_cursor_bounds();
  return 1;
}

// TODO column shift
static int cur_down(int count0, int count1, char *unused) {
  int count = count0 * count1;

  for (; count--;)
    TT.cursor = text_nsol(TT.cursor);
  check_cursor_bounds();
  return 1;
}

static int vi_H(int count0, int count1, char *unused) {
  TT.cursor = text_sol(TT.screen);
  return 1;
}

static int vi_L(int count0, int count1, char *unused) {
  TT.cursor = text_sol(TT.screen);
  cur_down(TT.screen_height - 1, 1, 0);
  return 1;
}

static int vi_M(int count0, int count1, char *unused) {
  TT.cursor = text_sol(TT.screen);
  cur_down(TT.screen_height / 2, 1, 0);
  return 1;
}

static int search_str(char *s, int direction) {
  // Perform the search
  size_t pos = text_strstr(TT.cursor + 1, s, direction);

  // Manage the search history buffer
  if (TT.last_search != s) {
    if (TT.last_search) {
      // SWAP: free() -> m_free()
      m_free(TT.last_search);
    }
    // Ensure vi_xstrdup uses m_new internally
    TT.last_search = vi_xstrdup(s);
  }

  // Update cursor if found
  if (pos != SIZE_MAX) {
    TT.cursor = pos;
  }

  check_cursor_bounds();
  return 0;
}

static int vi_yy(char reg, int count0, int count1) {
  size_t history = TT.cursor;
  size_t pos = text_sol(TT.cursor); // go left to first char on line
  TT.vi_mov_flag |= 4;

  for (; count0; count0--)
    TT.cursor = text_nsol(TT.cursor);

  vi_yank(reg, pos, 0);

  TT.cursor = history;
  return 1;
}

static int vi_dd(char reg, int count0, int count1) {
  size_t pos = text_sol(TT.cursor); // go left to first char on line
  TT.vi_mov_flag |= 0x30000000;

  for (; count0; count0--)
    TT.cursor = text_nsol(TT.cursor);

  if (pos == TT.cursor && TT.filesize)
    pos--;
  vi_delete(reg, pos, 0);
  check_cursor_bounds();
  return 1;
}

static int vi_x(char reg, int count0, int count1) {
  size_t from = TT.cursor;

  if (text_byte(TT.cursor) == '\n') {
    cur_left(count0 - 1, 1, 0);
  } else {
    cur_right(count0 - 1, 1, 0);
    if (text_byte(TT.cursor) == '\n')
      TT.vi_mov_flag |= 2;
    else
      cur_right(1, 1, 0);
  }

  vi_delete(reg, from, 0);
  check_cursor_bounds();
  return 1;
}

static int backspace(char reg, int count0, int count1) {
  size_t from = 0;
  size_t to = TT.cursor;
  cur_left(1, 1, 0);
  from = TT.cursor;
  if (from != to)
    vi_delete(reg, to, 0);
  check_cursor_bounds();
  return 1;
}

static int vi_movw(int count0, int count1, char *unused) {
  int count = count0 * count1;
  while (count--) {
    char c = text_byte(TT.cursor);
    do {
      if (TT.cursor > TT.filesize - 1)
        break;
      // if at empty jump to non empty
      if (c == '\n') {
        if (++TT.cursor > TT.filesize - 1)
          break;
        if ((c = text_byte(TT.cursor)) == '\n')
          break;
        continue;
      } else if (strchr(blank, c))
        do {
          if (++TT.cursor > TT.filesize - 1)
            break;
          c = text_byte(TT.cursor);
        } while (strchr(blank, c));
      // if at special jump to non special
      else if (strchr(specials, c))
        do {
          if (++TT.cursor > TT.filesize - 1)
            break;
          c = text_byte(TT.cursor);
        } while (strchr(specials, c));
      // else jump to empty or spesial
      else
        do {
          if (++TT.cursor > TT.filesize - 1)
            break;
          c = text_byte(TT.cursor);
        } while (c && !strchr(blank, c) && !strchr(specials, c));

    } while (strchr(blank, c) && c != '\n'); // never stop at empty
  }
  check_cursor_bounds();
  return 1;
}

static int vi_movb(int count0, int count1, char *unused) {
  int count = count0 * count1;
  int type = 0;
  char c;
  while (count--) {
    c = text_byte(TT.cursor);
    do {
      if (!TT.cursor)
        break;
      // if at empty jump to non empty
      if (strchr(blank, c))
        do {
          if (!--TT.cursor)
            break;
          c = text_byte(TT.cursor);
        } while (strchr(blank, c));
      // if at special jump to non special
      else if (strchr(specials, c))
        do {
          if (!--TT.cursor)
            break;
          type = 0;
          c = text_byte(TT.cursor);
        } while (strchr(specials, c));
      // else jump to empty or spesial
      else
        do {
          if (!--TT.cursor)
            break;
          type = 1;
          c = text_byte(TT.cursor);
        } while (!strchr(blank, c) && !strchr(specials, c));

    } while (strchr(blank, c)); // never stop at empty
  }
  // find first
  for (; TT.cursor; TT.cursor--) {
    c = text_byte(TT.cursor - 1);
    if (type && !strchr(blank, c) && !strchr(specials, c))
      break;
    else if (!type && !strchr(specials, c))
      break;
  }

  TT.vi_mov_flag |= 0x80000000;
  check_cursor_bounds();
  return 1;
}

static int vi_move(int count0, int count1, char *unused) {
  int count = count0 * count1;
  int type = 0;
  char c;

  if (count > 1)
    vi_movw(count - 1, 1, unused);

  c = text_byte(TT.cursor);
  if (strchr(specials, c))
    type = 1;
  TT.cursor++;
  for (; TT.cursor < TT.filesize - 1; TT.cursor++) {
    c = text_byte(TT.cursor + 1);
    if (!type && (strchr(blank, c) || strchr(specials, c)))
      break;
    else if (type && !strchr(specials, c))
      break;
  }

  TT.vi_mov_flag |= 2;
  check_cursor_bounds();
  return 1;
}

static void i_insert(char *str, int len) {
  if (!str || !len)
    return;

  insert_str(vi_xstrdup(str), TT.cursor, len, len, HEAP);
  TT.cursor += len;
  TT.filesize = text_filesize();
  TT.vi_mov_flag |= 0x30000000;
}

static int vi_zero(int count0, int count1, char *unused) {
  TT.cursor = text_sol(TT.cursor);
  TT.cur_col = 0;
  TT.vi_mov_flag |= 0x80000000;
  return 1;
}

static int vi_dollar(int count0, int count1, char *unused) {
  size_t new = text_strchr(TT.cursor, '\n');

  if (new != TT.cursor) {
    TT.cursor = new - 1;
    TT.vi_mov_flag |= 2;
    check_cursor_bounds();
  }
  return 1;
}

static void vi_eol() {
  TT.cursor = text_strchr(TT.cursor, '\n');
  check_cursor_bounds();
}

static void ctrl_b() {
  int i;

  for (i = 0; i < TT.screen_height - 2; ++i) {
    TT.screen = text_psol(TT.screen);
    // TODO: retain x offset.
    TT.cursor = text_psol(TT.screen);
  }
}

static void ctrl_d() {
  int i;

  for (i = 0; i < (TT.screen_height - 2) / 2; ++i)
    TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor)
    TT.cursor = TT.screen;
}

static void ctrl_f() {
  int i;

  for (i = 0; i < TT.screen_height - 2; ++i)
    TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor)
    TT.cursor = TT.screen;
}

static void ctrl_e() {
  TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor)
    TT.cursor = TT.screen;
}

static void ctrl_y() {
  TT.screen = text_psol(TT.screen);
  // TODO: only if we're on the bottom line
  TT.cursor = text_psol(TT.cursor);
  // TODO: real vi keeps the x position.
}

// TODO check register where to push from
static int vi_push(char reg, int count0, int count1) {
  // if row changes during push original cursor position is kept
  // vi inconsistancy
  // if yank ends with \n push is linemode else push in place+1
  size_t history = TT.cursor;
  char *start = TT.yank.data, *eol = strchr(start, '\n');
  if (strlen(start) == 0)
    return 1;

  if (start[strlen(start) - 1] == '\n') {
    if ((TT.cursor = text_strchr(TT.cursor, '\n')) == SIZE_MAX)
      TT.cursor = TT.filesize;
    else
      TT.cursor = text_nsol(TT.cursor);
  } else
    cur_right(1, 1, 0);

  i_insert(start, strlen(start));
  if (eol) {
    TT.vi_mov_flag |= 0x10000000;
    TT.cursor = history;
  }

  return 1;
}

static int vi_find_c(int count0, int count1, char *symbol) {
  ////  int count = count0*count1;
  size_t pos = text_strchr(TT.cursor, *symbol);
  if (pos != SIZE_MAX)
    TT.cursor = pos;
  return 1;
}

static int vi_find_cb(int count0, int count1, char *symbol) {
  // do backward search
  size_t pos = text_strrchr(TT.cursor, *symbol);
  if (pos != SIZE_MAX)
    TT.cursor = pos;
  return 1;
}

// if count is not spesified should go to last line
static int vi_go(int count0, int count1, char *symbol) {
  size_t prev_cursor = TT.cursor;
  int count = count0 * count1 - 1;
  TT.cursor = 0;

  if (TT.vi_mov_flag & 0x40000000 && (TT.cursor = TT.filesize) > 0)
    TT.cursor = text_sol(TT.cursor - 1);
  else if (count) {
    size_t next = 0;
    for (; count && (next = text_strchr(next + 1, '\n')) != SIZE_MAX; count--)
      TT.cursor = next;
    TT.cursor++;
  }

  check_cursor_bounds(); // adjusts cursor column
  if (prev_cursor > TT.cursor)
    TT.vi_mov_flag |= 0x80000000;

  return 1;
}

static int vi_o(char reg, int count0, int count1) {
  TT.cursor = text_eol(TT.cursor);
  insert_str(vi_xstrdup("\n"), TT.cursor++, 1, 1, HEAP);
  TT.vi_mov_flag |= 0x30000000;
  TT.vi_mode = 2;
  return 1;
}

static int vi_O(char reg, int count0, int count1) {
  TT.cursor = text_psol(TT.cursor);
  return vi_o(reg, count0, count1);
}

static int vi_D(char reg, int count0, int count1) {
  size_t pos = TT.cursor;
  if (!count0)
    return 1;
  vi_eol();
  vi_delete(reg, pos, 0);
  if (--count0)
    vi_dd(reg, count0, 1);

  check_cursor_bounds();
  return 1;
}

static int vi_I(char reg, int count0, int count1) {
  TT.cursor = text_sol(TT.cursor);
  TT.vi_mode = 2;
  return 1;
}

static int vi_join(char reg, int count0, int count1) {
  size_t next;
  while (count0--) {
    // just strchr(/n) and cut_str(pos, 1);
    if ((next = text_strchr(TT.cursor, '\n')) == SIZE_MAX)
      break;
    TT.cursor = next + 1;
    vi_delete(reg, TT.cursor - 1, 0);
  }
  return 1;
}

static int vi_find_next(char reg, int count0, int count1) {
  if (TT.last_search)
    search_str(TT.last_search, 1);
  return 1;
}

static int vi_find_prev(char reg, int count0, int count1) {
  if (TT.last_search)
    search_str(TT.last_search, 0);
  return 1;
}

static int vi_ZZ(char reg, int count0, int count1) {
  if (modified() && write_file(0) != 1) {
    return 1; // Write failed, don't exit
  }
  TT.vi_exit = 1;
  return 1;
}

// NOTES
// vi-mode cmd syntax is
//("[REG])[COUNT0]CMD[COUNT1](MOV)
// where:
//-------------------------------------------------------------
//"[REG] is optional buffer where deleted/yanked text goes REG can be
//   atleast 0-9, a-z or default "
//[COUNT] is optional multiplier for cmd execution if there is 2 COUNT
//   operations they are multiplied together
// CMD is operation to be executed
//(MOV) is movement operation, some CMD does not require MOV and some
//   have special cases such as dd, yy, also movements can work without
//   CMD
// ex commands can be even more complicated than this....

// special cases without MOV and such
struct vi_special_param {
  const char *cmd;
  int (*vi_special)(char, int, int); // REG,COUNT0,COUNT1
} vi_special[] = {
    {"D", &vi_D},    {"I", &vi_I},         {"J", &vi_join},      {"O", &vi_O},
    {"ZZ", &vi_ZZ},  {"N", &vi_find_prev}, {"n", &vi_find_next}, {"o", &vi_o},
    {"p", &vi_push}, {"x", &vi_x},         {"dd", &vi_dd},       {"yy", &vi_yy},
};
// there is around ~47 vi moves, some of them need extra params such as f and '
struct vi_mov_param {
  const char *mov;
  unsigned flags;
  int (*vi_mov)(int, int, char *); // COUNT0,COUNT1,params
} vi_movs[] = {
    {"0", 0, &vi_zero},   {"b", 0, &vi_movb},   {"e", 0, &vi_move},
    {"G", 0, &vi_go},     {"H", 0, &vi_H},      {"h", 0, &cur_left},
    {"j", 0, &cur_down},  {"k", 0, &cur_up},    {"L", 0, &vi_L},
    {"l", 0, &cur_right}, {"M", 0, &vi_M},      {"w", 0, &vi_movw},
    {"$", 0, &vi_dollar}, {"f", 1, &vi_find_c}, {"F", 1, &vi_find_cb},
};
// change and delete unfortunately behave different depending on move command,
// such as ce cw are same, but dw and de are not...
// also dw stops at w position and cw seem to stop at e pos+1...
// so after movement we need to possibly set up some flags before executing
// command, and command needs to adjust...
struct vi_cmd_param {
  const char *cmd;
  unsigned flags;
  int (*vi_cmd)(char, size_t, int); // REG,from,FLAGS
} vi_cmds[] = {
    {"c", 1, &vi_change},
    {"d", 1, &vi_delete},
    {"y", 1, &vi_yank},
};

static int run_vi_cmd(char *cmd) {
  int i = 0, val = 0;
  char *cmd_e;
  int (*vi_cmd)(char, size_t, int) = 0, (*vi_mov)(int, int, char *) = 0;

  TT.count0 = 0, TT.count1 = 0, TT.vi_mov_flag = 0;
  TT.vi_reg = '"';

  if (*cmd == '"') {
    cmd++;
    TT.vi_reg = *cmd++; // TODO check validity
  }
  errno = 0;
  val = strtol(cmd, &cmd_e, 10);
  if (errno || val == 0)
    val = 1, TT.vi_mov_flag |= 0x40000000;
  else
    cmd = cmd_e;
  TT.count0 = val;

  for (i = 0; i < ARRAY_LEN(vi_special); i++)
    if (strstr(cmd, vi_special[i].cmd))
      return vi_special[i].vi_special(TT.vi_reg, TT.count0, TT.count1);

  for (i = 0; i < ARRAY_LEN(vi_cmds); i++) {
    if (!strncmp(cmd, vi_cmds[i].cmd, strlen(vi_cmds[i].cmd))) {
      vi_cmd = vi_cmds[i].vi_cmd;
      cmd += strlen(vi_cmds[i].cmd);
      break;
    }
  }
  errno = 0;
  val = strtol(cmd, &cmd_e, 10);
  if (errno || val == 0)
    val = 1;
  else
    cmd = cmd_e;
  TT.count1 = val;

  for (i = 0; i < ARRAY_LEN(vi_movs); i++) {
    if (!strncmp(cmd, vi_movs[i].mov, strlen(vi_movs[i].mov))) {
      vi_mov = vi_movs[i].vi_mov;
      TT.vi_mov_flag |= vi_movs[i].flags;
      cmd++;
      if (TT.vi_mov_flag & 1 && !(*cmd))
        return 0;
      break;
    }
  }
  if (vi_mov) {
    int prev_cursor = TT.cursor;
    if (vi_mov(TT.count0, TT.count1, cmd)) {
      if (vi_cmd)
        return (vi_cmd(TT.vi_reg, prev_cursor, TT.vi_mov_flag));
      else
        return 1;
    } else
      return 0; // return some error
  }
  return 0;
}

static void draw_page();

static int get_endline(void) {
  int cln, rln;

  draw_page();
  cln = TT.cur_row + 1;
  run_vi_cmd("G");
  draw_page();
  rln = TT.cur_row + 1;
  run_vi_cmd(xmprintf("%dG", cln));

  return rln + 1;
}

// Return non-zero to exit.
static int run_ex_cmd(char *cmd) {
  int startline = 1, ofst = 0, endline;

  if (*cmd == '/' || *cmd == '\?')
    search_str(cmd + 1, *cmd == '/' ? 0 : 1);
  else if (*cmd == ':') {
    if (cmd[1] == 'q') {
      if (cmd[2] != '!' && modified())
        show_error("Unsaved changes (\"q!\" to ignore)");
      else
        return 1;
    } else if (!strncmp(cmd + 1, "w ", 2))
      write_file(&cmd[3]);
    else if (!strncmp(cmd + 1, "wq", 2)) {
      if (write_file(0))
        return 1;
      show_error("Unsaved changes (\"q!\" to ignore)");
    } else if (!strncmp(cmd + 1, "w", 1))
      write_file(0);

    else if (!strncmp(cmd + 1, "set list", sizeof("set list"))) {
      TT.list = 1;
      TT.vi_mov_flag |= 0x30000000;
    } else if (!strncmp(cmd + 1, "set nolist", sizeof("set nolist"))) {
      TT.list = 0;
      TT.vi_mov_flag |= 0x30000000;
    }

    else if (cmd[1] == 'd') {
      run_vi_cmd("dd");
      cur_up(1, 1, 0);
    } else if (cmd[1] == 'j')
      run_vi_cmd("J");
    else if (cmd[1] == 'g' || cmd[1] == 'v') {
      char *rgx = vi_xmalloc(strlen(cmd));
      int el = get_endline(), ln = 0, vorg = (cmd[1] == 'v' ? REG_NOMATCH : 0);
      if (sscanf(cmd + 2, "/%[^/]/%[^\ng]", rgx, cmd + 1) == 2) {
        regex_t rgxc;
        if (!regcomp(&rgxc, rgx, 0)) {
          cmd[0] = ':';

          for (; ln < el; ln++) {
            run_vi_cmd("yy");
            if (regexec(&rgxc, TT.yank.data, 0, 0, 0) == vorg)
              run_ex_cmd(cmd);
            cur_down(1, 1, 0);
          }

          // Reset Frame
          TT.vi_mov_flag |= 0x30000000;
        }
        regfree(&rgxc);
      }
      free(rgx);
    }

    // Line Ranges
    else if (cmd[1] >= '0' && cmd[1] <= '9') {
      if (strstr(cmd, ",")) {
        sscanf(cmd, ":%d,%d%[^\n]", &startline, &endline, cmd + 2);
        ofst = 1;
      } else
        run_vi_cmd(xmprintf("%dG", atoi(cmd + 1)));
    } else if (cmd[1] == '$')
      run_vi_cmd("G");
    else if (cmd[1] == '%') {
      endline = get_endline();
      ofst = 1;
    } else
      show_error("unknown command '%s'", cmd + 1);

    if (ofst) {
      int cline = TT.cur_row + 1;

      cmd[ofst] = ':';
      for (; startline <= endline; startline++) {
        run_ex_cmd(cmd + ofst);
        cur_down(1, 1, 0);
      }
      run_vi_cmd(xmprintf("%dG", cline));
      // Screen Reset
      TT.vi_mov_flag |= 0x30000000;
    }
  }
  return 0;
}

static int vi_crunch(FILE *out, int cols, int wc) {
  int ret = 0;
  if (wc < 32 && TT.list) {
    xputsn("\e[1m");
    ret = crunch_escape(out, cols, wc);
    xputsn("\e[m");
  } else if (wc == '\t') {
    if (out) {
      int i = TT.tabstop;
      for (; i--;)
        fputs(" ", out);
    }
    ret = TT.tabstop;
  } else if (wc == '\n')
    return 0;
  return ret;
}

// crunch_str with n bytes restriction for printing substrings or
// non null terminated strings
static int crunch_nstr(char **str, int width, int n, FILE *out, char *escmore,
                       int (*escout)(FILE *out, int cols, int wc)) {
  int columns = 0, col, bytes;
  char *start, *end;
  unsigned wc;

  for (end = start = *str; *end && n > 0;
       columns += col, end += bytes, n -= bytes) {
    if ((bytes = utf8towc(&wc, end, 4)) > 0 && (col = vi_wcwidth(wc)) >= 0) {
      if (!escmore || wc > 255 || !strchr(escmore, wc)) {
        if (width - columns < col)
          break;
        if (out)
          mp_hal_stdout_tx_strn(end, bytes);

        continue;
      }
    }

    if (bytes < 1) {
      bytes = 1;
      wc = *end;
    }
    col = width - columns;
    if (col < 1)
      break;
    if (escout) {
      if ((col = escout(out, col, wc)) < 0)
        break;
    } else if (out)
      mp_hal_stdout_tx_strn(end, 1);
  }
  *str = end;

  return columns;
}

static void draw_page() {

  unsigned y = 0;
  int x = 0, bytes = 0;
  char *line = 0, *end = 0;
  // screen coordinates for cursor
  int cy_scr = 0, cx_scr = 0;
  // variables used only for cursor handling
  int aw = 0, iw = 0, clip = 0, margin = 0, scroll = 0, redraw = 0, SSOL, SOL;

  TT.drawn_col = 0;
  if (TT.cur_col >= TT.screen_width) {
    clip = TT.cur_col - TT.screen_width + 1;
  }

  // adjust_screen_buffer();
  //  redraw = 3; //force full redraw
  redraw = (TT.vi_mov_flag & 0x30000000) >> 28;

  scroll = TT.drawn_row - TT.scr_row;
  if (TT.drawn_row < 0 || TT.cur_row < 0 || TT.scr_row < 0)
    redraw = 3;
  else if (abs(scroll) > TT.screen_height / 2)
    redraw = 3;

  xputsn("\e[H"); // jump to top left
  if (redraw & 2)
    xputsn("\e[2J\e[H"); // clear screen
  else if (scroll > 0)
    xprintf("\e[%dL", scroll); // scroll up
  else if (scroll < 0)
    xprintf("\e[%dM", -scroll); // scroll down

  SOL = text_sol(TT.cursor);
  bytes = text_getline(toybuf, SOL, ARRAY_LEN(toybuf));
  line = toybuf;

  for (SSOL = TT.screen, y = 0; SSOL < SOL; y++)
    SSOL = text_nsol(SSOL);

  cy_scr = y;

  // draw cursor row
  /////////////////////////////////////////////////////////////
  // for long lines line starts to scroll when cursor hits margin
  bytes = TT.cursor - SOL; // TT.cur_col;
  end = line;

  xprintf("\e[%u;0H\e[2K", y + 1);
  // find cursor position
  aw = crunch_nstr(&end, INT_MAX, bytes, 0, "\t\n", vi_crunch);

  // if we need to render text that is not inserted to buffer yet
  if (TT.vi_mode == 2 && TT.il->len) {
    char *iend = TT.il->data; // input end
    x = 0;
    // find insert end position
    iw = crunch_str(&iend, INT_MAX, 0, "\t\n", vi_crunch);
    clip = (aw + iw) - TT.screen_width + margin;

    // if clipped area is bigger than text before insert
    if (clip > aw) {
      clip -= aw;
      iend = TT.il->data;

      iw -= crunch_str(&iend, clip, 0, "\t\n", vi_crunch);
      x = crunch_str(&iend, iw, stdout, "\t\n", vi_crunch);
    } else {
      iend = TT.il->data;
      end = line;

      // if clipped area is substring from cursor row start
      aw -= crunch_nstr(&end, clip, bytes, 0, "\t\n", vi_crunch);
      x = crunch_str(&end, aw, stdout, "\t\n", vi_crunch);
      x += crunch_str(&iend, iw, stdout, "\t\n", vi_crunch);
    }
  }
  // when not inserting but still need to keep cursor inside screen
  // margin area
  else if (aw + margin > TT.screen_width) {
    clip = aw - TT.screen_width + margin;
    end = line;
    aw -= crunch_nstr(&end, clip, bytes, 0, "\t\n", vi_crunch);
    x = crunch_str(&end, aw, stdout, "\t\n", vi_crunch);
  } else {
    end = line;
    x = crunch_nstr(&end, aw, bytes, stdout, "\t\n", vi_crunch);
  }
  cx_scr = x;
  cy_scr = y;
  x += crunch_str(&end, TT.screen_width - x, stdout, "\t\n", vi_crunch);

  // start drawing all other rows that needs update
  ///////////////////////////////////////////////////////////////////
  y = 0, SSOL = TT.screen, line = toybuf;
  bytes = text_getline(toybuf, SSOL, ARRAY_LEN(toybuf));

  // if we moved around in long line might need to redraw everything
  if (clip != TT.drawn_col)
    redraw = 3;

  for (; y < TT.screen_height; y++) {
    int draw_line = 0;
    if (SSOL == SOL) {
      line = toybuf;
      SSOL += bytes + 1;
      bytes = text_getline(line, SSOL, ARRAY_LEN(toybuf));
      continue;
    } else if (redraw)
      draw_line++;
    else if (scroll < 0 && TT.screen_height - y - 1 < -scroll)
      scroll++, draw_line++;
    else if (scroll > 0)
      scroll--, draw_line++;

    xprintf("\e[%u;0H", y + 1);
    if (draw_line) {
      xprintf("\e[2K");
      if (line && strlen(line)) {
        aw = crunch_nstr(&line, clip, bytes, 0, "\t\n", vi_crunch);
        crunch_str(&line, TT.screen_width - 1, stdout, "\t\n", vi_crunch);
        if (*line)
          xprintf("@");
      } else
        xprintf("\e[2m~\e[m");
    }
    if (SSOL + bytes < TT.filesize) {
      line = toybuf;
      SSOL += bytes + 1;
      bytes = text_getline(line, SSOL, ARRAY_LEN(toybuf));
    } else
      line = 0;
  }

  TT.drawn_row = TT.scr_row, TT.drawn_col = clip;

  // Finished updating visual area, show status line.
  xprintf("\e[%u;0H\e[2K", TT.screen_height + 1);
  if (TT.vi_mode == 1)
    xprintf("\e[1mNORMAL\e[m %s", TT.filename);
  if (TT.vi_mode == 2)
    xprintf("\e[1mINSERT\e[m %s", TT.filename);
  if (!TT.vi_mode) {
    cx_scr = xprintf("%s", TT.il->data);
    cy_scr = TT.screen_height;
    *toybuf = 0;
  } else {
    // TODO: the row,col display doesn't show the cursor column
    // TODO: real vi shows the percentage by lines, not bytes
    sprintf(toybuf, "%lu/%luC %lu%% %d,%d", (unsigned long)TT.cursor,
            (unsigned long)TT.filesize,
            (unsigned long)(100 * TT.cursor) / (TT.filesize ?: 1),
            TT.cur_row + 1, TT.cur_col + 1);
    if (TT.cur_col != cx_scr)
      sprintf(toybuf + strlen(toybuf), "-%d", cx_scr + 1);
  }
  xprintf("\e[%u;%uH%s\e[%u;%uH", TT.screen_height + 1,
          (int)(1 + TT.screen_width - strlen(toybuf)), toybuf, cy_scr + 1,
          cx_scr + 1);
}

static struct termios orig_termios;

void set_terminal_raw(void) {

  tcgetattr(0, &orig_termios);
  struct termios raw = orig_termios;

  // Input: Allow Carriage Return to Newline mapping (don't disable ICRNL)
  // We want to keep ICRNL so Enter key works as \n
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON);
  raw.c_iflag |= ICRNL;

  // Output: Ensure Newline is converted to Carriage Return + Newline
  // This prevents the "staircase" effect that looks like prepended spaces.
  raw.c_oflag |= (OPOST | ONLCR);

  // Local: No echo, no line-buffering (Canonical mode)
  raw.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG | IEXTEN);

  // Control: 8-bit characters
  raw.c_cflag |= (CS8);

  // Read behavior: Wait for at least 1 character
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  tcsetattr(0, TCSAFLUSH, &raw);
}

void reset_terminal(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void vi_main(char *filename, int width, int height) {

  mp_hal_set_interrupt_char(-1);
  set_terminal_raw();
  char keybuf[16] = {0}, vi_buf[16] = {0}, utf8_code[8] = {0};
  int utf8_dec_p = 0, vi_buf_pos = 0;
  FILE *script = TT.s ? xfopen(TT.s, "r") : 0;

  TT.il = m_new_obj(struct str_line);

  if (TT.il) {
    memset(TT.il, 0, sizeof(struct str_line));
    TT.il->alloc = 40;
    TT.il->data = m_new0(char, TT.il->alloc);
  }

  TT.yank.alloc = 128;
  TT.yank.data = m_new0(char, TT.yank.alloc);

  TT.filename = filename;
  linelist_load(0, 1);

  TT.vi_mov_flag = 0x20000000;
  TT.vi_mode = 1, TT.tabstop = 8;

  TT.screen_width = width, TT.screen_height = height;
  TT.screen_height -= 1;

  set_terminal(0, 1, 0, 0);
  // writes stdout into different xterm buffer so when we exit
  // we dont get scroll log full of junk
  xputsn("\e[?1049h");

  xprintf("\x1b[4l"); // Disable IRM (Insert Replacement Mode)

  if (TT.c) {
    FILE *cc = xfopen(TT.c, "r");
    char *line;

    while ((line = xgetline(cc)))
      if (run_ex_cmd(TT.il->data))
        goto cleanup_vi;
    vfs_fclose(cc);
  }

  mp_hal_stdout_tx_strn("\e[2J\e[H", 7);

  for (;;) {
    int key = 0;

    draw_page();

    mp_handle_pending(true);
    mp_hal_delay_ms(1);

    // TODO script should handle cursor keys
    if (script && EOF == (key = fgetc(script))) {
      vfs_fclose(script);
      script = 0;
    }
    if (!script)
      key = scan_key(keybuf, -1);

    if (key == -1)
      goto cleanup_vi;
    else if (key == -3) {
      toys.signal = 0;
      TT.screen_height -= 1; // TODO this is hack fix visual alignment
      continue;
    }

    // TODO: support cursor keys in ex mode too.
    if (TT.vi_mode && key >= 256) {
      key -= 256;
      // if handling arrow keys insert what ever is in input buffer before
      // moving
      if (TT.il->len) {
        i_insert(TT.il->data, TT.il->len);
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
      }
      if (key == KEY_UP)
        cur_up(1, 1, 0);
      else if (key == KEY_DOWN)
        cur_down(1, 1, 0);
      else if (key == KEY_LEFT)
        cur_left(1, 1, 0);
      else if (key == KEY_RIGHT)
        cur_right(1, 1, 0);
      else if (key == KEY_HOME)
        vi_zero(1, 1, 0);
      else if (key == KEY_END)
        vi_dollar(1, 1, 0);
      else if (key == KEY_PGDN)
        ctrl_f();
      else if (key == KEY_PGUP)
        ctrl_b();

      continue;
    }

    if (TT.vi_mode == 1) { // NORMAL
      switch (key) {
      case '/':
      case '?':
      case ':':
        TT.vi_mode = 0;
        TT.il->data[0] = key;
        TT.il->len++;
        break;
      case 'A':
        vi_eol();
        TT.vi_mode = 2;
        break;
      case 'a':
        cur_right(1, 1, 0);
        // FALLTHROUGH
      case 'i':
        TT.vi_mode = 2;
        break;
      case CTL('D'):
        ctrl_d();
        break;
      case CTL('B'):
        ctrl_b();
        break;
      case CTL('E'):
        ctrl_e();
        break;
      case CTL('F'):
        ctrl_f();
        break;
      case CTL('Y'):
        ctrl_y();
        break;
      case '\e':
        vi_buf[0] = 0;
        vi_buf_pos = 0;
        break;
      case 0x7F: // FALLTHROUGH
      case '\b':
        backspace(TT.vi_reg, 1, 1);
        break;
      default:
        if (key > ' ' && key < '{') {
          vi_buf[vi_buf_pos] = key; // TODO handle input better
          vi_buf_pos++;
          if (run_vi_cmd(vi_buf)) {
            memset(vi_buf, 0, 16);
            vi_buf_pos = 0;
          } else if (vi_buf_pos == 15) {
            vi_buf_pos = 0;
            memset(vi_buf, 0, 16);
          }
        }

        break;
      }
    } else if (TT.vi_mode == 0) { // EX MODE
      switch (key) {
      case '\x7f':
      case '\b':
        if (TT.il->len > 1) {
          TT.il->data[--TT.il->len] = 0;
          break;
        }
        // FALLTHROUGH
      case '\e':
        TT.vi_mode = 1;
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
        break;
      case '\n':
      case '\r':
        if (run_ex_cmd(TT.il->data))
          goto cleanup_vi;
        TT.vi_mode = 1;
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
        break;
      default:                          // add chars to ex command until ENTER
        if (key >= ' ' && key < 0x7F) { // might be utf?
          if (TT.il->len == TT.il->alloc) {
            TT.il->data = realloc(TT.il->data, TT.il->alloc * 2);
            TT.il->alloc *= 2;
          }
          TT.il->data[TT.il->len] = key;
          TT.il->len++;
        }
        break;
      }
    } else if (TT.vi_mode == 2) { // INSERT MODE
      switch (key) {
      case '\e':
        i_insert(TT.il->data, TT.il->len);
        cur_left(1, 1, 0);
        TT.vi_mode = 1;
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
        break;
      case 0x7F:
      case '\b':
        if (TT.il->len) {
          char *last = utf8_last(TT.il->data, TT.il->len);
          int shrink = strlen(last);
          memset(last, 0, shrink);
          TT.il->len -= shrink;
        } else
          backspace(TT.vi_reg, 1, 1);
        break;
      case '\n':
      case '\r':
        // insert newline
        TT.il->data[TT.il->len++] = '\n';
        i_insert(TT.il->data, TT.il->len);
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
        break;
      default:
        if ((key >= ' ' || key == '\t') &&
            utf8_dec(key, utf8_code, &utf8_dec_p)) {
          if (TT.il->len + utf8_dec_p + 1 >= TT.il->alloc) {
            TT.il->data = realloc(TT.il->data, TT.il->alloc * 2);
            TT.il->alloc *= 2;
          }
          strcpy(TT.il->data + TT.il->len, utf8_code);
          TT.il->len += utf8_dec_p;
          utf8_dec_p = 0;
          *utf8_code = 0;
        }
        break;
      }
    }
    // Check for exit flag (used by ZZ command)
    if (TT.vi_exit)
      goto cleanup_vi;
  }
cleanup_vi:
  linelist_unload();

  // Clear the input buffer and yank (clipboard) buffer
  // These must use m_free to match our GC-safe allocations
  if (TT.il) {
    if (TT.il->data)
      m_free(TT.il->data);
    m_free(TT.il);
    TT.il = NULL;
  }

  if (TT.yank.data) {
    m_free(TT.yank.data);
    TT.yank.data = NULL;
  }

  // Restore terminal state
  tty_reset();

  // Switch back from the alternate screen buffer
  xputsn("\e[?1049l");
}
