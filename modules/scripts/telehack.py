import tft_config
import terminus_mpy_regular as rfont
import terminus_mpy_bold as bfont
import vt
import machine
import time
import telnet
import network
from secrets import secrets
import tdeck

tft = tft_config.config(rotation=1, buffer_size=14*320*2)

tft.init()

i2c = machine.I2C(0, sda=machine.Pin(18), scl=machine.Pin(8), freq=400000)

rows = 240 // rfont.HEIGHT
cols = 320 // rfont.WIDTH

t = vt.VT(tft, cols, rows, rfont, bfont)


def refresh_loop(timer):
    t.draw()

# 30ms = ~33 FPS.
#refresh_timer = machine.Timer(0)
#refresh_timer.init(period=30, mode=machine.Timer.PERIODIC, callback=refresh_loop)


# Apply the magic
#console = TDeckConsole(t, i2c)
#os.dupterm(console)



print("Connecting to wifi...")
wlan = network.WLAN(network.STA_IF)
wlan.active(True)
if not wlan.isconnected():
    wlan.connect(secrets['ssid'], secrets['pw'])
    while not wlan.isconnected():
        time.sleep(1)



kbd = tdeck.Keyboard(i2c)

def handle_telnet_output(text):
    t.write(text)

# Example: Telehack (great for testing text formatting)
# Host: telehack.com, Port: 23
client = telnet.TelnetClient("telehack.com", 23, cols=cols, rows=rows, on_receive_callback=handle_telnet_output)
client.connect()

try:
    while client.connected:
        client.process(input_device=kbd)
        t.draw()
        time.sleep_ms(2)

except KeyboardInterrupt:
    print("\nDisconnected.")
    client.socket.close()

