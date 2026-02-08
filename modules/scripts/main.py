import tft_config
import terminus_mpy_regular as rfont
import terminus_mpy_bold as bfont
import vt
import os
import machine
import time
import telnet
import network
from secrets import secrets

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()


rows = 240 // rfont.HEIGHT
cols = 320 // rfont.WIDTH

t = vt.VT(tft, cols, rows, rfont, bfont)


os.dupterm(t)

def refresh_loop(timer):
    t.draw()

# 30ms = ~33 FPS.
refresh_timer = machine.Timer(0)
refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)







print("Connecting to wifi...")
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
if not wlan.isconnected():
    wlan.connect(secrets['ssid'], secrets['pw'])
    while not wlan.isconnected():
        time.sleep(1)

# Example: Telehack (great for testing text formatting)
# Host: telehack.com, Port: 23
client = telnet.TelnetClient("telehack.com", 23, cols=cols, rows=rows)
client.connect()

try:
    while client.connected:
        client.process()
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\nDisconnected.")
    client.socket.close()

