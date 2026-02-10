import tft_config
import terminus_mpy_regular as rfont
import terminus_mpy_bold as bfont
import machine
import vt
import tdeck_kbd
import tdeck_kv
import os




rows = 240 // rfont.HEIGHT
cols = 320 // rfont.WIDTH

tft = tft_config.config(rotation=1, buffer_size=14*320*2)
tft.init()

t = vt.VT(tft, cols, rows, rfont, bfont)
kbd = tdeck_kbd.Keyboard(sda=18, scl=8)

kv = tdeck_kv.KV(t, kbd)


os.dupterm(kv)

def refresh_loop(timer):
    t.draw()

# 30ms = ~33 FPS.
refresh_timer = machine.Timer(0)
refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)
