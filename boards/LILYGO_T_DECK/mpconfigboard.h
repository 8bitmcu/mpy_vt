#define MICROPY_HW_BOARD_NAME "LILYGO T-Deck"
#define MICROPY_HW_MCU_NAME "ESP32S3"

#define MICROPY_HW_ENABLE_UART_REPL (1)

// Default is 16*1024 (mpconfigport.h) -- too small for codec2_create()'s
// deep DSP init (FFT/LPC/quantiser table setup). Overflowing mp_task's
// stack there didn't fault directly (task stacks are heap-backed on
// ESP-IDF, so an overflow just scribbles into adjacent heap memory);
// instead it corrupted heap metadata that only crashed later, in an
// unrelated task's free() call, which is what a coredump caught. Doubling
// this gives codec2_create() enough headroom to stop overflowing.
#define MICROPY_TASK_STACK_SIZE (32 * 1024)
