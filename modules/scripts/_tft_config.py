from machine import Pin, SPI
import st7789

# tft_config for "Cheap Yellow Display" (CYD) also known as ESP32-2432S024

def config(rotation=0):
    # Using Polarity 0, Phase 0 (Standard for ILI9341/ST7789 on ESP32)
    spi = SPI(2, baudrate=20000000, polarity=0, phase=0, sck=Pin(14), mosi=Pin(13))

    return st7789.ST7789(
        spi,
        240,
        320,
        reset=Pin(12, Pin.OUT),
        cs=Pin(15, Pin.OUT),
        dc=Pin(2, Pin.OUT),
        backlight=Pin(21, Pin.OUT), # note: if 21 doesn't work try 27
        rotation=rotation,
        color_order=st7789.BGR,
        inversion=False,
        options=0x80
    )
