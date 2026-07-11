import machine
import micropython
import vt
import tdeck_kbd
import tdeck_trk
import tdeck_kvm
import os
import sys
import time
import st7789
import time
import statusbar
import shell

# Must be called before initializing LCD / Keyboard
machine.Pin(10, machine.Pin.OUT, value=1)
time.sleep(0.1)

# Pull all SPI CS pins HIGH before touching the bus (prevents bus conflicts)
machine.Pin(12, machine.Pin.OUT, value=1)  # display CS
machine.Pin(9, machine.Pin.OUT, value=1)   # LoRa CS
machine.Pin(39, machine.Pin.OUT, value=1)  # SD CS
machine.Pin(38, machine.Pin.IN, machine.Pin.PULL_UP)  # MISO pull-up
time.sleep(0.1)

# Mount SD Card (Experimental; doesn't seem to always work)
try:
    import sdcard
    sd_spi = machine.SoftSPI(baudrate=400000, sck=machine.Pin(40), mosi=machine.Pin(41), miso=machine.Pin(38))
    sd = sdcard.SDCard(sd_spi, machine.Pin(39, machine.Pin.OUT))

    os.mount(os.VfsFat(sd), '/sd')
except Exception as e:
    sd = None

# Create hardware SPI; this configures the GPIO matrix for pins 40/41.
# SoftSPI is no longer used after this point.
spi = machine.SPI(1, baudrate=40000000, sck=machine.Pin(40), mosi=machine.Pin(41), miso=machine.Pin(38))

# The card is already initialized. Only the transport changes.
# Gives the already-mounted SD card faster hardware SPI for data transfers.
if sd is not None:
    sd.spi = spi

class Environment:
    def __init__(self, font):
        # Screen dimensions in pixel
        self.screen_width = 320
        self.screen_height = 240
        self.status_height = 0
        self.cols = 0
        self.rows = 0
        self.font = None
        self.font_name = ""
        self.kvm = None
        self.tui = None
        self.term = None
        self.sts = None
        self.shell = None
        self.update_font(font)

    def update_font(self, font_name):
        new_font = __import__(f"fonts.{font_name}", None, None, [font_name])
        self.font_name = font_name
        self.font = new_font

        # Reserve 1 row for a topbar
        self.status_height = self.font.HEIGHT

        # How many characters can we fit on the screen?
        self.cols = self.screen_width // self.font.WIDTH
        usable_height = self.screen_height - self.status_height
        self.rows = usable_height // self.font.HEIGHT


        if self.term:
            self.term.update_layout(self.font, self.cols, self.rows)
            self.term.top_offset(self.status_height)
            env.sts.update_width(self.cols)

env = Environment("terminus_mpy_14")

# Initialze LCD
tft = st7789.ST7789(spi,
                    env.screen_height,
                    env.screen_width,
                    reset=machine.Pin(1, machine.Pin.OUT),
                    dc=machine.Pin(11, machine.Pin.OUT),
                    cs=machine.Pin(12, machine.Pin.OUT),
                    backlight=machine.Pin(42, machine.Pin.OUT),
                    rotation=1,
                    buffer_size=env.screen_width*env.font.HEIGHT*2)
tft.init()

# Initialize ST engine
env.term = vt.VT(tft, env)
env.term.top_offset(env.status_height)

# Initialize keyboard
kbd = tdeck_kbd.Keyboard(sda=18, scl=8)

# Initialize Trackball
tdeck_trk.init()

# Combine ST & keyboard into one stream object
env.kvm = tdeck_kvm.KVM(env.term, kbd)

# Redirect to REPL
os.dupterm(env.kvm)

# Status bar component
env.sts = statusbar.StatusBar(env.term, width=env.cols)
env.sts.refresh()

# The FAST loop (30ms)
def scheduled_fast(_):

    # Trackball horizontal movement translates into going up/down the shell command history
    h_delta = tdeck_trk.get_scroll_horiz()
    if abs(h_delta) > 1:
        if h_delta < 0:
            env.kvm.inject("\x1b[A") # Injects 'Up' key into REPL
        else:
            env.kvm.inject("\x1b[B") # Injects 'Down' key into REPL

    # Trackball vertical movement translates into showing history
    # Default history is 100 lines defined as HISTSIZE in st.h
    v_delta = tdeck_trk.get_scroll_vert()
    if abs(v_delta) > 1:
        if v_delta < 0:
            env.term.scrolldown()
        else:
            env.term.scrollup()

    # Long clicking will raise KeyboardInterrupt (internally to tdeck_trk)
    # Short click will inject escape
    if tdeck_trk.get_click():
        env.kvm.inject("\x1b")

    env.term.draw()

def fast_loop(_):
    # Use schedule to keep the ISR (Interrupt Service Routine) light
    micropython.schedule(scheduled_fast, 0)

# The SLOW loop (1000ms)
def scheduled_slow(_):
    env.sts.refresh()

def slow_loop(_):
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

# "REPL" into a custom, simple shell
env.shell = shell.Shell(env)
env.shell.run()

class Cmd:
    def __init__(self, func):
        self.func = func
    def __repr__(self):
        self.func()
        return ""

# quick way to return to the shell from MicroPython
# just type `sh` into MicroPython to return to our shell
sh = Cmd(env.shell.run)

