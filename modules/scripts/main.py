import st7789
import tft_config
import terminus_8x14r
import vt
import time

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()


rows = 240 // terminus_8x14r.HEIGHT
cols = 320 // terminus_8x14r.WIDTH

t = vt.VT(tft, terminus_8x14r, cols, rows)


def demo_colors(t):
    t.write("\033[2J\033[H") # Clear screen
    t.write("--- ANSI COLOR PALETTE ---\r\n\r\n")
    for i in range(30, 38): # Foreground colors
        for j in range(40, 48): # Background colors
            t.write(f"\033[{i};{j}m {i-30}{j-40} \033[0m")
        t.write("\r\n")
    t.write("\r\nStandard 16-color mode test complete.\r\n")
    time.sleep(10)

def demo_split_screen(t):
    # Set scrolling region: rows 3 to 10 (ANSI is 1-indexed)
    # Sequence: \033[top;bottomr
    t.write("\033[2J\033[H")
    t.write("\033[1;44m      FIXED HEADER: SYSTEM MONITOR      \033[0m\r\n")
    t.write("-" * 40 + "\r\n")
    
    t.write("\033[3;15r") # Define scrolling area (lines 3 to 15)
    t.write("\033[3;1H")  # Move cursor to start of scroll area
    
    for i in range(50):
        t.write(f"Log entry {i}: System is nominal...\r\n")
        time.sleep(0.1)
        # Watch how the blue header stays at the top!
    
    t.write("\033[r") # Reset scrolling region to full screen
    t.write("\033[17;1H\033[1;42m FOOTER: Status OK \033[0m")
    time.sleep(5)

import random, time

def demo_matrix(t):
    t.write("\033[2J\033[?25l") # Clear screen and hide cursor
    cols = 40
    rows = 17
    while True:
        try:
            x = random.randint(1, cols)
            y = random.randint(1, rows)
            # Move cursor, set color to green, print random char
            char = chr(random.randint(33, 126))
            t.write(f"\033[{y};{x}H\033[32m{char}\033[0m")
            if random.random() > 0.9: # Occasionally clear a spot
                 t.write(f"\033[{y};{x}H ")
        except KeyboardInterrupt:
            t.write("\033[?25h\033[0m") # Show cursor and reset
            break


def demo_progress(t):
    t.write("\033[2J\033[H")
    t.write("Downloading Firmware Update...\r\n")
    for i in range(101):
        bar = "#" * (i // 4) + "-" * (25 - (i // 4))
        # \r moves cursor to start of line, does not feed a new line
        t.write(f"\rProgress [{bar}] {i}%")
        time.sleep(0.05)
    t.write("\r\n\r\nUpdate Complete! Rebooting...\r\n")
    time.sleep(3)

def demo_attributes(t):
    t.write("\033[2J\033[H")
    t.write("NORMAL TEXT\r\n")
    t.write("\033[1mBOLD TEXT\033[0m\r\n")
    t.write("\033[2mDIM TEXT\033[0m\r\n")
    t.write("\033[3mITALIC TEXT\033[0m\r\n")
    t.write("\033[4mUNDERLINE TEXT\033[0m\r\n")
    t.write("\033[7mINVERTED TEXT\033[0m\r\n")
    t.write("\033[5mBLINKING TEXT (If supported)\033[0m\r\n")
    time.sleep(5)


while True:
    demo_colors(t)
    demo_split_screen(t)
#demo_matrix(t)
    demo_progress(t)
    #demo_attributes(t)
