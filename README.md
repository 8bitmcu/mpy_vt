# mpy_vt: Optimized ANSI Terminal Engine for MicroPython

This project implements a high-performance, attribute-aware terminal emulator for MicroPython. By wrapping the [st](https://st.suckless.org/) (suckless terminal) engine in a custom C module, it achieves desktop-class terminal features on embedded hardware, including a **zero-allocation status bar** for real-time telemetry without heap fragmentation. Unlike basic serial monitors, it handles complex escape sequences, colors, and text attributes with the efficiency and speed characteristic of suckless software.

This project features first-class support for the [LILYGO T-Deck](https://s.click.aliexpress.com/e/_c4agv9Wd), transforming it into a standalone portable terminal. The integration leverages the T-Deck’s hardware keyboard, trackball and 320x240 display, utilizing the ESP32-S3's PSRAM to manage the terminal's backbuffer and state.

As a showcase of the engine's capabilities, this project includes a fully functional, VFS-aware C port of the [vi](https://en.wikipedia.org/wiki/Vi_(text_editor)) **text editor** and [frotz](https://davidgriffith.gitlab.io/frotz/) **ZMachine interpreter** that supports playing classic text games like [Zork](https://en.wikipedia.org/wiki/Zork). The firmware provides a Python-based **Telnet client**, **FTP server**, a TUI based **File Manager**, **IRC Client**, **RSS Reader** and  **Network Manager** demonstrating how the terminal engine can be easily extended.

| ASCII demo (running on CYD) | vi app |
| :---: | :---: |
| <img src="assets/screen.gif" alt="ascii demo" width="400"> | <img src="assets/screen2.jpg" alt="vi app" width="400"> |

| Minesweeper (telnet) | Zork (telnet) |
| :---: | :---: |
| <img src="assets/screen3.jpg" alt="minesweeper" width="400"> | <img src="assets/screen4.jpg" alt="zork" width="400"> |


## 📟 T-Deck Hardware Integration

This project is optimized for the **LilyGO T-Deck**, leveraging MicroPython to interface with the ESP32-S3 and its integrated peripherals.

### 🛠 Supported Components

| Component | Specification | Driver / Status |
| :--- | :--- | :--- |
| **Memory** | 8MB PSRAM / 16MB Flash | Enabled for Large Buffer Handling |
| **SD Card** | SPI | Experimental |
| **Display** | 2.4" ST7789 LCD (320x240) | Optimized SPI bus (Full Color) |
| **Keyboard** | LILYGO Keyboard | Mapped I2C Interface |
| **Trackball** | LILYGO Trackball | Mapped I2C Interface |
| **Microphone** | I2S, ES7210 ADC | todo |
| **Speaker** | I2S | todo |
| **Touchscreen** | GT911 | todo |
| **LoRa Radio** | SX1262 | todo |


## ⚡ Quick Install (Pre-compiled Binaries)

You can download and flash the latest pre-compiled firmware directly to your T-Deck.

### Option A: Flash from command line

1. **Download the Firmware**: Go to the [Releases Page](https://github.com/8bitmcu/mpy_vt/releases) and download the latest *.bin asset.

2. **Flash to the T-Deck**: Make sure your T-Deck is plugged in via USB, then use esptool.py to write the firmware to the device. You may need to install esptool first (`pip install esptool`).

```Bash

esptool.py -p /dev/ttyACM0 -b 460800 --chip esp32s3 write_flash 0x0 firmware.bin

```

### Option B: Bootloader (Launcher)

This firmware is fully compatible with [Launcher](https://bmorcelli.github.io/Launcher), an on-device application launcher and bootloader for ESP32 devices. This method is perfect if you want to seamlessly swap between `mpy_vt` and other firmware on the go without needing a PC.

1. **Install Launcher**: Open a Web Serial compatible browser (like Chrome or Edge) and navigate to the Launcher website. Select Web Flasher and follow the prompts to install the bootloader directly to your T-Deck.
2. **Prepare your SD Card**: Download the latest *.bin from the Releases Page and copy it to a MicroSD card.
3. **Boot and Install**: Insert the MicroSD card into your T-Deck and power it on. Using the Launcher interface on the device's screen, navigate to your SD card and select the `mpy_vt` binary to flash it.


## 📝 How to use

### Trackball Usage
- **Short Press**: Sends `Esc`
- **Long Press**: Sends `KeyboardInterrupt`
- **Scroll up**: Scroll up st terminal history
- **Scroll down**: Scroll down st terminal history
- **Scroll left**: Scroll up st command history
- **Scroll right**: Scroll down st command history

### Available commands

You can execute the following commands from the built-in shell:

| Command | Description |
| :---   | :--- |
| `clear` | Clears the screen |
| `fav` | Built-in shell aliases |
| `fc` | Font Configuration Utility |
| `fm` | Starts the TUI File Manager |
| `ftpd` | Launches a FTP Server on `/` with user `admin` and pwd `admin` |
| `irc` | Connects to an IRC channel given a server, port, nickname and channel |
| `menu` | An interactive shortcut menu for commands |
| `ms` | Opens the minesweeper clone |
| `nm` | Starts the TUI Network Manager |
| `rss` | RSS Reader; connect to an http or https rss endpoint and retreives the titles |
| `telnet` | Connects to a telnet server |
| `vi` | Opens the vi port |
| `zm` | Launches `dfrotz`, the ZMachine interpreter |

To get out of the shell, type `exit`. This will bring you to the MicroPython shell, where you can type in python expressions. To get back to the built-in shell, type `sh` in the MicroPython shell.

## 🔨 How to Build (T-Deck)

### Option A: Building on Host machine

Building this project requires a cross-compiler for the ESP32-S3 and the MicroPython source tree. Ensure you have the ESP-IDF (Espressif IoT Development Framework) installed. This project is verified using **MicroPython v1.28.0** and **ESP-IDF v5.5.1**.

```bash
# Clone this repository
git clone https://github.com/8bitmcu/mpy_vt.git

# Copy the T-Deck board definition into your micropython source directory:
cp -r /path/to/mpy_vt/boards/LILYGO_T_DECK /path/to/micropython/ports/esp32/boards/

# Initialize the ESP-IDF environment
source $HOME/esp/esp-idf/export.sh

# Build the MicroPython Cross-Compiler
make -C /path/to/micropython/mpy-cross

# Navigate to the T-Deck port directory.
cd /path/to/micropython/ports/esp32

# Specify the BOARD as LILYGO_T_DECK to enable PSRAM / Flash support
# Specify USER_C_MODULES and FROZEN_MANIFEST for C modules and python scripts
make BOARD=LILYGO_T_DECK USER_C_MODULES=/path/to/mpy_vt/modules FROZEN_MANIFEST=/path/to/mpy_vt/modules/manifest.py

# Flash the firmware to the device
esptool.py -p /dev/ttyACM0 -b 460800 --chip esp32s3 write_flash 0x0 firmware.bin
```

### Option B: Building using Docker and Makefile

You do not need to install the ESP-IDF, toolchains, or MicroPython source code on your host machine if you have `Docker` and `Make` installed.

#### Prerequisites:

- Docker installed and running.
- Make installed on your host system.
- A Linux environment (or WSL2 on Windows) that allows USB device passthrough to Docker.

If you just want to build and flash the firmware, run these commands in order:


```Bash

# 1. Clone this repository and enter in it
git clone https://github.com/8bitmcu/mpy_vt.git && cd mpy_vt

# 2. Initialize the environment (pulls MicroPython source)
make init

# 3. Compile the firmware
make build

# 4. Flash to the device (ensure your T-Deck is plugged in)
make flash
```
#### Makefile Reference

`make init` sets up the pristine build environment. Builds the necessary local Docker image (micropython-build) and creates a persistent Docker volume. It then clones MicroPython and its submodules directly into that volume. Run this once when setting up the project, or to force a fresh pull of the MicroPython source.

`make build` compiles the MicroPython firmware. Mounts your local boards/ and modules/ directories into the ESP-IDF container. It compiles the C-level modules, freezes your Python manifests, and builds the target specifically for the LILYGO_T_DECK. Outputs: The compiled binaries (firmware.bin, micropython.bin) will appear in your local `build_output/` folder.

`make flash` flashes the compiled firmware to the ESP32-S3. Uses `esptool.py` inside the container to erase the flash and write the new firmware.bin to address 0x0.

`make sync_files` transfers your Python application code to the device. Uses `mpremote` to recursively copy everything inside ./modules/scripts/ into the root of the T-Deck's internal flash filesystem.

`make sync_file FILE=filename.py` transfers a single Python script file to the device. Uses `mpremote` to copy from ./modules/scripts/ into the root of the T-Deck's internal flash filesystem.

`make repl` opens the MicroPython interactive prompt. Connects your terminal to the device's serial output via mpremote. Press Ctrl+D inside the REPL to trigger a soft reboot, or Ctrl+] to exit back to your host terminal.

`make core_dump` analyzes fatal crashes. If your device crashes and enters a bootloop or halts, this command reads the raw coredump partition directly from the flash and maps it against the .elf file in your build volume to provide a human-readable C-level stack trace.

`make clean` cleans the build artifacts. Removes the compiled object files from the Docker volume and deletes the local build_output/ folder to ensure your next make build starts completely fresh.

#### Overriding Variables

The Makefile defaults to /dev/ttyACM0 for the USB connection. If your OS assigns a different port (e.g., /dev/ttyUSB0), you can override it inline without editing the Makefile:

```Bash
make flash PORT=/dev/ttyUSB0
make repl PORT=/dev/ttyUSB0
```


## 🧩 C Modules

This project is composed of the following C modules:

| Module | Role | Stream Type | Description |
| :---   | :--- | :--- | :--- |
| `st7789` | Display Driver | N/A | Modified version of the standard driver. Exposes internal frame buffer pointers to vt for direct-memory access (DMA) rendering. |
| `tdeck_kbd` | Input Driver | Readable | Low-level driver for the T-Deck I2C keyboard/trackball. Handles key scanning and interrupt flags. |
| `tdeck_trk` | Motion Engine | Interrupts | Low-level driver for the T-Deck trackball. Uses GPIO interrupts to track relative motion (deltas) and supports edge-detection for short/long click durations. |
| `tdeck_kvm` | Stream Glue | Read/Write | A composite Keyboard-Video-Mouse (trackball) object. It binds vt (Output) and tdeck_kbd (Input) into a single stream object compatible with os.dupterm. |
| `vt` | Terminal Engine | Writable | The core emulator. Receives ANSI text, updates internal state, and renders changes to the st7789 display. |
| `vttui` | User Interface | Read/Write | A simple to use curses-like text user interface library. |
| `vi` | Text Editor | Read/Write | A C-integrated port of the classic `vi` editor. |
| `xml` | Parsing Library | N/A | A C-port of the `yxml` library. |
| `zm` | ZMachine Interpreter | Read/Write | A port of the `frotz` Zmachine interpreter. |


## ⚖️ License & Attribution

This project's source code is licensed under the **MIT License**. However, if you compile the firmware with the optional `frotz` module enabled, the resulting compiled binary is distributed under the **GPLv2 License**.

### Third-Party Components:
* **st License:** MIT (c) st engineers.
* **st7789_mpy:** (c) Russ Hughes. MIT License
* **vi** (Toybox): (c) Rob Landley, Jarno Mäkipää. 0BSD License (Zero-Clause BSD).
* **frotz**: (c) Stefan Jokisch, David Griffith. GPLv2 License
* **yxml**: Copyright (c) 2013-2014 Yoran Heling. MIT License
* **MicroPython**: (c) Damien P. George. MIT License

### Fonts & Assets:
* **Terminus Font:** (c) 2020 Dimitar Zhekov. Licensed under the [SIL Open Font License 1.1](https://scripts.sil.org/OFL).
* **Cozette Font:** Copyright (c) 2020 Samhain <samhain@moonwit.ch> & contributors <https://github.com/the-moonwitch/Cozette/contributors>. Distributed under the terms of the MIT License.
* **Tamzen Font:** Copyright 2011 Suraj N. Kurapati <https://github.com/sunaku/tamzen-font>. Tamzen font is free. You are hereby granted permission to use, copy, modify, and distribute it as you see fit. Tamzen font is provided "as is" without any express or implied warranty. The author makes no representations about the suitability of this font for a particular purpose. In no event will the author be held liable for damages arising from the use of this font.
* **Gohu Font:** Copyright 2015 by Hugo Chargois. Distributed under the terms of the [WTFPL version 2](https://www.wtfpl.net/about/).
