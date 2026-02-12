import terminus_mpy_regular as rfont
import terminus_mpy_bold as bfont
import machine
import micropython
import vt
import tdeck_kbd
import tdeck_trk
import tdeck_kvm
import os
import network
import sys
import time
import telnet
from secrets import secrets
import st7789
import time
import status

# Screen dimensions in pixel
screen_width = 320
screen_height = 240

# Reserve 1 row for a topbar
status_height = rfont.HEIGHT

# How many characters can we fit on the screen?
cols = screen_width // rfont.WIDTH
usable_height = screen_height - status_height
rows = usable_height // rfont.HEIGHT

# Must be called before initializing LCD / Keyboard
pwr_en = machine.Pin(10, machine.Pin.OUT)
pwr_en.value(1)
time.sleep(0.1)

# Initialze LCD
spi = machine.SPI(1, baudrate=40000000, sck=machine.Pin(40), mosi=machine.Pin(41))
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
term.top_offset(status_height)

# Initialize keyboard
kbd = tdeck_kbd.Keyboard(sda=18, scl=8)

# Initialize Trackball
tdeck_trk.init()

# Combine ST & keyboard into one stream object
kvm = tdeck_kvm.KVM(term , kbd)

# Redirect to REPL
os.dupterm(kvm)

# status bar component
sts = status.StatusBar(term, width=cols)
sts.refresh()

# The FAST loop (30ms)
def scheduled_fast(_):

    # Trackball horizontal movement translates into going up/down the shell command history
    h_delta = tdeck_trk.get_scroll_horiz()
    if abs(h_delta) > 1:
        if h_delta < 0:
            kvm.inject("\x1b[A") # Injects 'Up' key into REPL
        else:
            kvm.inject("\x1b[B") # Injects 'Down' key into REPL

    # Trackball vertical movement translates into showing history
    # Default history is 100 lines defined as HISTSIZE in st.h
    v_delta = tdeck_trk.get_scroll_vert()
    if abs(v_delta) > 1:
        if v_delta < 0:
            term.scrolldown()
        else:
            term.scrollup()

    # Long clicking will raise KeyboardInterrupt (internally to tdeck_trk)
    tdeck_trk.get_click()

    term.draw()

def fast_loop(t):
    # Use schedule to keep the ISR (Interrupt Service Routine) light
    micropython.schedule(scheduled_fast, 0)

# The SLOW loop (1000ms)
def scheduled_slow(_):
    sts.refresh()

def slow_loop(t):
    # Update the status bar string (ANSI parsing happens here)
    micropython.schedule(scheduled_slow, 0)

# 30ms = ~33 FPS.
draw_timer = machine.Timer(0)
draw_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=fast_loop)

statusbar_timer = machine.Timer(1)
statusbar_timer.init(period=1000, mode=machine.Timer.PERIODIC, callback=slow_loop)




# Choose your cursor:

# Beam Cursor
sys.stdout.write("\x1b[ 6 q")

# Underline Cursor
#sys.stdout.write("\x1b[ 4 q")

# Block Cursor (default)
#sys.stdout.write("\x1b[ 2 q")



# Overwrite prompt (optional)
sys.ps1 = "\033[1;37m$ \033[0m"
sys.ps2 = "\033[1;37m. \033[0m"








class Command:
    def __init__(self, func):
        self.func = func
    def __repr__(self):
        self.func()
        return ""

def check_leaks():
    import gc
    for i in range(10):
        before = gc.mem_free()
        time.sleep(1)
        after = gc.mem_free()
        print(f"Leak: {before - after} bytes/sec")

leak = Command(check_leaks)

def connect_wlan():
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

wlan = Command(connect_wlan)

def clear_screen():
    print("\033[2J\033[H", end="")

clear = Command(clear_screen)

# Example: Telehack (great for testing text formatting)
# Host: telehack.com, Port: 23
def quick_telnet(name, port):
    def handle_telnet_output(text):
        term.write(text)

    client = telnet.TelnetClient(name, port, cols=cols, rows=rows, on_receive_callback=handle_telnet_output)
    client.connect()

    try:
        while client.connected:
            client.process(input_device=kvm)
            time.sleep_ms(2)
    except KeyboardInterrupt:
        print("\nDisconnected.")
        client.socket.close()


def telehack():
    quick_telnet("telehack.com", 23)

def retrocampus():
    quick_telnet("bbs.retrocampus.com", 23)

def thekeep():
    quick_telnet("thekeep.net", 23)

def baud300():
    quick_telnet("300baud.dynu.net", 2525)

def amis86():
    quick_telnet("amis86.ddns.net", 9000)

def cbbs():
    quick_telnet("cbbsoutpost.servebbs.com", 23)





# type wifi() to connect to wifi
# type telehack() for telnet telehack
