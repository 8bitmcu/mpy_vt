/* vi.h - You can't spell "evil" without "vi".
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 * Copyright 2019 Jarno Mäkipää <jmakip87@gmail.com>
 * Copyright 2026 Vincent (8bitmcu) (MicroPython/VFS Port)
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/vi.html
 * Licensed under 0BSD.
 */

#ifndef VI_H
#define VI_H

void vi_main(char *filename, int width, int height);
void vi_init();

struct vi_data {
  char *c, *s;
  char *filename;
  int vi_mode, tabstop, list, cur_col, cur_row, scr_row, drawn_row, drawn_col,
      count0, count1, vi_mov_flag, vi_exit;
  unsigned screen_height, screen_width;
  char vi_reg, *last_search;
  char *toybuf;

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

static struct vi_data *ptrTT;
#define TT (*ptrTT)
#define TOYBUF_SIZE 4096

static mp_obj_t vi_state_obj;

#endif
