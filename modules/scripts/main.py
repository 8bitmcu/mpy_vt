import terminus_mpy_regular as rfont
import terminus_mpy_bold as bfont
import machine
import vt
import tdeck_kbd
import tdeck_kv
import os
import network
import sys
import time
import telnet
from secrets import secrets
import st7789
import time

# Screen dimensions in pixel
screen_width = 320
screen_height = 240

# How many characters can we fit on the screen
rows = screen_height // rfont.HEIGHT
cols = screen_width // rfont.WIDTH

# Must be called before initializing LCD / Keyboard
pwr_en = machine.Pin(10, machine.Pin.OUT)
pwr_en.value(1)
time.sleep(0.1)

# Initialze LCD
spi = machine.SPI(2, baudrate=40000000, sck=machine.Pin(40), mosi=machine.Pin(41))
tft = st7789.ST7789(spi,
    screen_height,
    screen_width,
    reset=machine.Pin(1, machine.Pin.OUT),
    dc=machine.Pin(11, machine.Pin.OUT),
    cs=machine.Pin(12, machine.Pin.OUT),
    backlight=machine.Pin(42, machine.Pin.OUT),
    rotation=1,
    buffer_size=screen_width*rfont.HEIGHT*2)
tft.init()

# Initialize ST engine
term = vt.VT(tft, cols, rows, rfont, bfont)

# Initialize keyboard
kbd = tdeck_kbd.Keyboard(sda=18, scl=8)

# Combine ST & keyboard into one stream object
kv = tdeck_kv.KV(term , kbd)

# Redirect to REPL
os.dupterm(kv)

# Update LCD periodically
def refresh_loop(timer):
    term.draw()

# 30ms = ~33 FPS.
refresh_timer = machine.Timer(0)
refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)


# Choose your cursor:

# Beam Cursor
sys.stdout.write("\x1b[ 6 q")

# Underline Cursor
#sys.stdout.write("\x1b[ 4 q")

# Block Cursor
#sys.stdout.write("\x1b[ 2 q")






def wifi():
    # ANSI Escape Codes
    CLR = "\x1b[0m"      # Reset all
    BOLD = "\x1b[1m"     # Bold (Swaps to your bfont)
    CYAN = "\x1b[36m"    # Cyan text
    GREEN = "\x1b[32m"   # Green text
    YELLOW = "\x1b[33m"  # Yellow text
    WHITE_ON_BLUE = "\x1b[37;44m" # White text on Blue background

    print(f"{CYAN}Connecting to WiFi...{CLR}")

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(secrets['ssid'], secrets['pw'])
        while not wlan.isconnected():
            sys.stdout.write(f"{YELLOW}.{CLR}") 
            time.sleep(1)

    print(f"\n{BOLD}{GREEN}WiFi Connected!{CLR}")
    ip, subnet, gateway, dns = wlan.ifconfig()

    print(f"{WHITE_ON_BLUE}      NETWORK CONFIGURATION      {CLR}")
    print(f"{BOLD}IP Address:{CLR}  {GREEN}{ip}{CLR}")
    print(f"{BOLD}Subnet:{CLR}      {subnet}")
    print(f"{BOLD}Gateway:{CLR}     {gateway}")
    print(f"{BOLD}DNS Server:{CLR}  {dns}")
    print(f"{CYAN}{'-' * 33}{CLR}")


# Example: Telehack (great for testing text formatting)
# Host: telehack.com, Port: 23
def telehack():
    def handle_telnet_output(text):
        term.write(text)

    client = telnet.TelnetClient("telehack.com", 23, cols=cols, rows=rows, on_receive_callback=handle_telnet_output)
    client.connect()

    try:
        while client.connected:
            client.process(input_device=kbd)
            time.sleep_ms(2)
    except KeyboardInterrupt:
        print("\nDisconnected.")
        client.socket.close()


# type wifi() to connect to wifi
# type telehack() for telnet telehack
