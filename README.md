# mpy_vt: Optimized ANSI Terminal Engine for ESP32

This project implements a high-performance, attribute-aware terminal emulator for MicroPython. By wrapping the [st](https://st.suckless.org/) (suckless terminal) engine in a custom C module, it achieves desktop-class terminal features on embedded hardware.

![animation](screen.gif)

## 🚀 Features

### **Core Terminal Capabilities**
* **Full `st` (Suckless Terminal) Core:** Leveraging a battle-tested terminal engine for industrial-grade ANSI escape sequence parsing.
* **Dynamic VT100/ANSI Support:** Support for standard terminal commands, including cursor positioning, screen clearing, and scrolling regions.
* **Hardware-Accelerated Rendering:** C-based scanline buffering that maximizes SPI throughput and eliminates screen tearing.

### **Advanced Styling & Attributes**
* **Dual-Font Engine:** Native support for **Bold** (`\033[1m`) and **Regular** font weights with seamless mid-line switching.
* **Rich Text Attributes:**
    * **Underline:** Standard ANSI underlining logic for text emphasis.
    * **Inverse Video:** Full support for reverse-video (`\033[7m`), perfect for highlighting and selection.

### **Full Xterm 256-Color Support**
The engine supports the complete 256-color ANSI palette, including:
* **0-15:** Standard and High-Intensity ANSI colors.
* **16-231:** 6x6x6 RGB Color Cube.
* **232-255:** 24-step grayscale ramp.

All colors are mathematically mapped to **RGB565** and pre-swapped for **Big-Endian SPI** displays (ST7789), ensuring zero-overhead rendering during the draw cycle.

### **User Interface & Ergonomics**
* **Modern "Beam" Cursor:** A sleek, 2-pixel vertical bar cursor providing a modern "code editor" aesthetic.
* **Visibility Control:** Integrated support for terminal modes to show/hide the cursor (`MODE_HIDE`) and toggle behavior based on focus.
* **Smart Space Handling:** Intelligent rendering of trailing spaces ensures that underscores and cursors appear correctly even at the end of a line.

### **Developer-Friendly Architecture**
* **Zero-Copy Font Access:** Utilizes the MicroPython buffer protocol to read font data directly from Flash memory, saving precious RAM.
* **MicroPython Integrated:** Simple Pythonic API to write data, change styles, and interface with ESP32 hardware.
* **Scalable Scanline Buffer:** Optimized memory footprint that allows for high-resolution terminal grids on resource-constrained microcontrollers.

### **Dirty-Line Tracking & Optimization**
Unlike standard display drivers that refresh the entire screen for every character, `mpy_vt` utilizes the `st` engine's internal dirty-line bitmask:
* **Selective Redrawing:** Only modified rows are sent over SPI. Typing a single character updates only **1/20th** (or less) of the screen.
* **Atomic Windowing:** Uses hardware-level address windowing (`CASET`/`RASET`) to update specific horizontal slices, significantly reducing bus contention.

## 🔌 MicroPython REPL Integration

### **Native `os.dupterm` Support**
The engine implements the MicroPython stream protocol, allowing it to act as a secondary system console. By redirecting the REPL, you can view real-time logs, tracebacks, and interactive prompts directly on your hardware.

* **Non-Blocking I/O:** Implements `MP_EAGAIN` logic to ensure the physical USB/UART connection remains active while the display mirrors the output.
* **Seamless Redirection:** Fully compatible with standard MicroPython redirection workflows.

### **Usage Example**
```python
# buffer_size MUST be at a minimum: lcd_width * font_height * 2
tft = tft_config.config(rotation=1, buffer_size=14*320*2)
tft.init()

# Calculate the number of columns/rows based on font size and LCD size
rows = 240 // font_regular.HEIGHT
cols = 320 // font_regular.WIDTH

# Optionally pass a bold font as the 4th parameter
#t = vt.VT(tft, cols, rows, font_regular, font_bold)
t = vt.VT(tft, cols, rows, font_regular)

# Optionally redirect REPL to this terminal
os.dupterm(t)


# Set up a timer to update the display
def refresh_loop(timer):
    t.draw()

# 30ms = ~33 FPS.
refresh_timer = machine.Timer(0)
refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)
```

## 🛠️ API Reference

| Method | Description |
| :--- | :--- |
| `v.write(data)` | Manually feed ANSI strings or raw bytes into the engine. |
| `v.draw()` | Triggers a refresh of all "dirty" lines to the hardware. |
| `ioctl / read / write` | Internal stream protocol methods for `os.dupterm` compatibility. |

## ⚖️ License & Attribution

This project is licensed under the **MIT License**.

### Third-Party Components:
* **st License:** MIT (c) st engineers.
* **st7789_mpy:** (c) Russ Hughes. [MIT License]

### Fonts & Assets:
* **Terminus Font:** (c) 2020 Dimitar Zhekov. Licensed under the [SIL Open Font License 1.1](https://scripts.sil.org/OFL).
