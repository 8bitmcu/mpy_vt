from machine import Pin, SPI
import st7789
import time

pwr_en = Pin(10, Pin.OUT)
pwr_en.value(1)
time.sleep(0.1) # Let the LCD wake up

spi = SPI(2, baudrate=40000000, sck=Pin(40), mosi=Pin(41))

def config(rotation=0, buffer_size=0):
    return st7789.ST7789(spi,
                        240,
                        320,
                        reset=Pin(1, Pin.OUT),
                        dc=Pin(11, Pin.OUT),
                        cs=Pin(12, Pin.OUT),
                        backlight=Pin(42, Pin.OUT),
                        rotation=1,
                        buffer_size=buffer_size)

