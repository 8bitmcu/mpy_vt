#
# MicroPython StatusBar
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import machine
import time
import network
import gc

class StatusBar:

    def __init__(self, terminal, width=40):
        self.term = terminal
        self.start_ticks = time.ticks_ms()
        self.bat_pin = machine.ADC(machine.Pin(4))
        self.bat_pin.atten(machine.ADC.ATTN_11DB)
        self.wlan = network.WLAN(network.STA_IF)

        self.style = b"\033[38;5;0m\033[48;5;252m"
        self.clear = b"\033[0m"

        # Initialize the layout and calculate offsets dynamically via the new method
        self.update_width(width)

    def update_width(self, width):
        """Regenerates the layout template and recalculates internal offsets dynamically."""
        self.width = width

        # Fixed structural blocks matching original template rules
        left = b"UP 0:00 | MEM      | " # 21 chars
        right = b"WiFi OFF | BAT  00%" # 19 chars

        # Build the middle padding dynamically to fill up to the new target width
        pad_len = width - len(left) - len(right)
        if pad_len < 0:
            pad_len = 0
            
        mid_pad = b" " * pad_len
        template = left + mid_pad + right

        # Reallocate the bytearray buffer matching the new dimensions
        self.buffer = bytearray(self.style + template + self.clear + b"\x00")

        # Recalculate fixed positions relative to current styling sequence length
        s_len = len(self.style)
        self.off_up   = s_len + 3   # Index of the first '0' in 0:00
        self.off_mem  = s_len + 13  # Index of the first '0' in 000K

        # Recalculate right-aligned offsets relative to the updated end of the buffer
        content_end = len(self.buffer) - 5
        self.off_bat  = content_end - 4 
        self.off_wifi = self.off_bat - 11

    def _write_at(self, offset, data):
        """Helper to overwrite a slice of the buffer"""
        self.buffer[offset : offset + len(data)] = data

    def refresh(self):
        # Gather raw data (Integer math only to avoid float allocations)
        total_sec = time.ticks_diff(time.ticks_ms(), self.start_ticks) // 1000
        mins = total_sec // 60
        secs = total_sec % 60

        # Scaled integer math for battery percentage
        raw = self.bat_pin.read_u16()
        pct = ((raw * 660) // 65535) - 320
        if pct < 0: pct = 0
        if pct > 100: pct = 100

        # 1. Memory Calculation
        mem_free = gc.mem_free()
        
        if mem_free >= 1048576: # 1024 * 1024
            mem_scaled = (mem_free * 10) // 1048576 
            val = mem_scaled // 10    
            dec = mem_scaled % 10     
            unit = 77                 # ASCII 'M'
            
            self.buffer[self.off_mem]     = 48 + (val // 10) if val >= 10 else 32
            self.buffer[self.off_mem + 1] = 48 + (val % 10)
            self.buffer[self.off_mem + 2] = 46 
            self.buffer[self.off_mem + 3] = 48 + dec
            self.buffer[self.off_mem + 4] = unit
        else:
            mem_k = mem_free // 1024
            mh = (mem_k // 100) % 10
            mt = (mem_k // 10) % 10
            mu = mem_k % 10
            
            self.buffer[self.off_mem]     = 48 + mh if mh > 0 else 32
            self.buffer[self.off_mem + 1] = 48 + mt if (mt > 0 or mh > 0) else 32
            self.buffer[self.off_mem + 2] = 48 + mu
            self.buffer[self.off_mem + 3] = 75 # ASCII 'K'
            self.buffer[self.off_mem + 4] = 32 

        # Poke digits into the buffer (ASCII 48 is '0')
        # Uptime M:SS
        self.buffer[self.off_up]     = 48 + (mins % 10)
        self.buffer[self.off_up + 2] = 48 + (secs // 10)
        self.buffer[self.off_up + 3] = 48 + (secs % 10)

        # Battery 000%
        bh = (pct // 100) % 10
        bt = (pct // 10) % 10
        bu = pct % 10

        self.buffer[self.off_bat]     = 48 + bh if bh > 0 else 32
        self.buffer[self.off_bat + 1] = 48 + bt if (bt > 0 or bh > 0) else 32
        self.buffer[self.off_bat + 2] = 48 + bu

        # WiFi Status
        if self.wlan.isconnected():
            self.buffer[self.off_wifi : self.off_wifi + 4] = b" ON "
        else:
            self.buffer[self.off_wifi : self.off_wifi + 4] = b" OFF"

        # Push to Terminal
        self.term.top_bar(self.buffer)
