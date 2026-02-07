import st7789
import tft_config
import terminus
import term
import time

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()


print("Initializing Terminal...")
t = term.Terminal(tft, terminus)

print("Writing Text...")

# 1. Basic Text
t.write("System Boot...\r\n")
time.sleep(0.5)
t.write("Kernel loaded.\r\n")

print("Writing Text...")
# 2. ANSI Colors (The real test of st_term.c logic)
# \033[31m = Red, \033[32m = Green, \033[34m = Blue, \033[0m = Reset
t.write("\033[32m[OK]\033[0m Filesystem mounted.\r\n")
t.write("\033[31m[ERR]\033[0m No keyboard found.\r\n")
t.write("\033[34m[INFO]\033[0m Network initializing...\r\n")

# 3. Cursor Movement (lines 10-12)
# \033[H = Home, \033[5B = Down 5 lines
t.write("\nTesting wrap:\r\n")
t.write("This is a very long line that should verify if the terminal automatically wraps to the next line correctly.\r\n")

print("Writing Text...")
# 4. Stress Loop
# Keep the object alive!
i = 0
while True:
    # \r overwrites the current line
    t.write(f"\rUptime: {i} seconds")
    i += 1
    time.sleep(1)
