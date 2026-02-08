# Optimized ANSI Terminal Engine for ESP32

This project implements a high-performance, attribute-aware terminal emulator for MicroPython. By wrapping the [st](https://st.suckless.org/) (suckless terminal) engine in a custom C module, it achieves desktop-class terminal features on embedded hardware.

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

TODO:
* **ANSI Color Palette:** Support for the standard 8 foreground and background colors, mapped to vibrant **16-bit RGB565** values.

### **User Interface & Ergonomics**
* **Modern "Beam" Cursor:** A sleek, 2-pixel vertical bar cursor providing a modern "code editor" aesthetic.
* **Visibility Control:** Integrated support for terminal modes to show/hide the cursor (`MODE_HIDE`) and toggle behavior based on focus.
* **Smart Space Handling:** Intelligent rendering of trailing spaces ensures that underscores and cursors appear correctly even at the end of a line.

### **Developer-Friendly Architecture**
* **Zero-Copy Font Access:** Utilizes the MicroPython buffer protocol to read font data directly from Flash memory, saving precious RAM.
* **MicroPython Integrated:** Simple Pythonic API to write data, change styles, and interface with ESP32 hardware.
* **Scalable Scanline Buffer:** Optimized memory footprint that allows for high-resolution terminal grids on resource-constrained microcontrollers.


## ⚖️ License & Attribution

This project is licensed under the **MIT License**.

This work incorporates code from the [st (Suckless Terminal)](https://st.suckless.org/) project.
* **st License:** MIT (c) st engineers.
