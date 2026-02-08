import st7789
import tft_config
import terminus_8x14r
import terminus_8x14b
import vt
import os
import machine
import time
import random

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()


rows = 240 // terminus_8x14r.HEIGHT
cols = 320 // terminus_8x14r.WIDTH

t = vt.VT(tft, cols, rows, terminus_8x14r, terminus_8x14b)


os.dupterm(t)

def refresh_loop(timer):
    t.draw()

# 30ms = ~33 FPS.
refresh_timer = machine.Timer(0)
refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)







COLS = cols
ROWS = rows
LOG_HEIGHT = 4 # How many lines for the scrolling log
LOG_START_ROW = 4 # Where the log starts
CSI = "\033["
RESET = f"{CSI}0m"
HIDE_CURSOR = f"{CSI}?25l"
SHOW_CURSOR = f"{CSI}?25h"
CLEAR = f"{CSI}2J"
HOME = f"{CSI}H"

def move(y, x):
    return f"{CSI}{y};{x}H"

def color(fg=None, bg=None):
    s = ""
    if fg is not None: s += f"38;5;{fg};"
    if bg is not None: s += f"48;5;{bg};"
    return f"{CSI}{s[:-1]}m"

def style(b=False, u=False, r=False):
    s = ""
    if b: s += "1;"
    if u: s += "4;"
    if r: s += "7;"
    return f"{CSI}{s[:-1]}m" if s else ""

# --- Components ---

def draw_header(start_time):
    uptime = int(time.time() - start_time)
    mins = uptime // 60
    secs = uptime % 60
    time_str = f"UP: {mins:02}:{secs:02}"
    sys_str = "SYSTEM"
    
    # Pad the middle with spaces to push "SYSTEM" to the right
    padding = COLS - len(time_str) - len(sys_str) - 2
    
    # Blue Background (44), White Text (15), Bold
    print(f"{move(1,1)}{CSI}44;1;37m {time_str}{' ' * padding}{sys_str} {RESET}", end="")

def draw_progress(percent):
    # Draw at Row 2
    width = COLS - 2
    filled = int((percent / 100) * width)
    bar = "#" * filled + "-" * (width - filled)
    
    # Color gradient for the bar based on %
    col = 196 # Red
    if percent > 30: col = 208 # Orange
    if percent > 60: col = 226 # Yellow
    if percent > 80: col = 46  # Green
    
    print(f"{move(2,1)} {color(fg=col)}{bar}{RESET} ", end="")

def draw_log(log_buffer):
    # Draw the log lines in the middle section
    for i, line in enumerate(log_buffer):
        # Clear the line first (using spaces or \033[K) to avoid artifacts
        # Using specific colors for the log data (Matrix green)
        print(f"{move(LOG_START_ROW + i, 1)}{CSI}38;5;34m{line:<{COLS}}{RESET}", end="")

def draw_static_tests():
    # Header 2
    r = LOG_START_ROW + LOG_HEIGHT + 1
    msg = "TESTING COLORS"
    pad = (COLS - len(msg)) // 2
    print(f"{move(r, 1)}{' ' * pad}{CSI}1;4;37m{msg}{RESET}", end="")

    # Attribute Test
    r += 2
    # Bold Red, Regular Green, Underline Blue
    print(f"{move(r, 2)}{CSI}1;31mBold Red{RESET}   ", end="")
    print(f"{CSI}32mReg Green{RESET}", end="")
    
    r += 1
    print(f"{move(r, 2)}{CSI}4;36mUndr Cyan{RESET}  ", end="")
    print(f"{CSI}35mMagenta{RESET}", end="")

def draw_greyscale():
    # Draw at the very bottom
    # Greyscale ramp in 256 colors starts at 232 and ends at 255
    r = ROWS
    width = 24 # Use 24 steps
    start_idx = 232
    
    print(move(r, 2), end="")
    for i in range(width):
        # Draw a block with background color
        print(f"{CSI}48;5;{start_idx + i}m ", end="")
    print(RESET, end="")

# --- Main Loop ---

def run():
    print(f"{CLEAR}{HIDE_CURSOR}", end="")
    
    log_buffer = [""] * LOG_HEIGHT
    chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%^&*"
    percent = 0
    start_time = time.time()
    
    # Draw static elements once
    draw_static_tests()
    draw_greyscale()

    try:
        while True:
            # 1. Update Header
            draw_header(start_time)
            
            # 2. Update Progress
            percent = (percent + 2) % 101
            draw_progress(percent)
            
            # 3. Update Log (Fake scrolling)
            # Generate a random hex-like string
            new_line = "".join(random.choice(chars) for _ in range(COLS-2))
            log_buffer.pop(0)
            log_buffer.append(new_line)
            draw_log(log_buffer)
            
            # Speed control
            time.sleep(0.05) # 20 FPS
            
    except KeyboardInterrupt:
        print(f"{RESET}{CLEAR}{move(1,1)}Demo Stopped.")
        print(SHOW_CURSOR)

if __name__ == "__main__":
    run()
