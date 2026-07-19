/*
 * ring_buf.h
 *
 * Small single-producer/single-consumer byte ring buffer shared by
 * audioplayer.c and audiorecorder.c.
 *
 * audioplayer.c: the MicroPython thread is the producer (feed(), pulling
 * from a VFS file), the background decode task is the consumer.
 * audiorecorder.c: the reverse -- the background capture task is the
 * producer (pulling from I2S RX), the MicroPython thread is the consumer
 * (draining to a VFS file).
 *
 * Either direction, the rule is the same: only one task ever calls
 * rb_write(), only one (possibly different) task ever calls rb_read(); a
 * mutex around each operation makes those two safe to run concurrently
 * with each other.
 *
 * Header-only (static inline) so it can be shared without adding another
 * translation unit to the usermod build.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
  uint8_t *data;
  size_t size;
  volatile size_t head;  // next write index
  volatile size_t tail;  // next read index
  volatile size_t count; // bytes currently queued
  volatile bool eof;     // producer has no more data to add, ever
  SemaphoreHandle_t lock;
} ring_buf_t;

static inline bool rb_init(ring_buf_t *rb, size_t size) {
  memset(rb, 0, sizeof(*rb));
  rb->data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (rb->data == NULL) {
    rb->data = heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
  rb->lock = xSemaphoreCreateMutex();
  rb->size = size;
  return rb->data != NULL && rb->lock != NULL;
}

static inline void rb_deinit(ring_buf_t *rb) {
  if (rb->lock) {
    vSemaphoreDelete(rb->lock);
    rb->lock = NULL;
  }
  if (rb->data) {
    free(rb->data);
    rb->data = NULL;
  }
}

static inline void rb_reset(ring_buf_t *rb) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  rb->head = rb->tail = rb->count = 0;
  rb->eof = false;
  xSemaphoreGive(rb->lock);
}

// Producer side: queue up to len bytes, returns bytes actually queued
// (less than len if the buffer is nearly full).
static inline size_t rb_write(ring_buf_t *rb, const uint8_t *src, size_t len) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  size_t space = rb->size - rb->count;
  size_t n = (len < space) ? len : space;
  size_t first = rb->size - rb->head;
  if (first > n) {
    first = n;
  }
  memcpy(rb->data + rb->head, src, first);
  if (n > first) {
    memcpy(rb->data, src + first, n - first);
  }
  rb->head = (rb->head + n) % rb->size;
  rb->count += n;
  xSemaphoreGive(rb->lock);
  return n;
}

// Consumer side: pop up to len bytes, returns bytes actually read.
static inline size_t rb_read(ring_buf_t *rb, uint8_t *dst, size_t len) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  size_t n = (len < rb->count) ? len : rb->count;
  size_t first = rb->size - rb->tail;
  if (first > n) {
    first = n;
  }
  memcpy(dst, rb->data + rb->tail, first);
  if (n > first) {
    memcpy(dst + first, rb->data, n - first);
  }
  rb->tail = (rb->tail + n) % rb->size;
  rb->count -= n;
  xSemaphoreGive(rb->lock);
  return n;
}

static inline size_t rb_free_space(ring_buf_t *rb) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  size_t n = rb->size - rb->count;
  xSemaphoreGive(rb->lock);
  return n;
}

static inline size_t rb_available(ring_buf_t *rb) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  size_t n = rb->count;
  xSemaphoreGive(rb->lock);
  return n;
}
