import st7789
import tft_config
import terminus_8x14r
import terminus_8x14b
import vt
import time

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()


rows = 240 // terminus_8x14r.HEIGHT
cols = 320 // terminus_8x14r.WIDTH

t = vt.VT(tft, cols, rows, terminus_8x14r, terminus_8x14b)

def demo_underlines(t):
    t.write("\033[2J\033[H") # Clear and Home
    t.write("--- ANSI UNDERLINE TEST ---\r\n\r\n")

    # 1. Standard Colors with Underline
    colors = {
        "31": "Red",
        "32": "Green",
        "33": "Yellow",
        "34": "Blue",
        "35": "Magenta",
        "36": "Cyan",
        "37": "White"
    }

    for code, name in colors.items():
        # \033[4;{code}m -> 4 is Underline, {code} is color
        t.write(f"\033[4;{code}mUnderlined {name}\033[0m\r\n")

    t.write("\r\n")

    # 2. The Complex Combo: Underline + Invert
    # The underline should be the BACKGROUND color now!
    t.write("Inverted Test:\r\n")
    t.write("\033[4;7;32mUnderlined Inverted Green\033[0m\r\n")
    
    # 3. Trailing Spaces
    # This checks if your 'else' block in C correctly underlines empty space
    t.write("\r\nSpace Test:\r\n")
    t.write("\033[4mFixed Width Underline    \033[0m[End]\r\n")

demo_underlines(t)
