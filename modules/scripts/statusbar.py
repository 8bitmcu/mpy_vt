import machine
import time
import network
import gc

class StatusBar:

    def __init__(self, terminal, width=40):
        self.term = terminal
        self.width = width
        self.start_ticks = time.ticks_ms()
        self.bat_pin = machine.ADC(machine.Pin(4))
        self.bat_pin.atten(machine.ADC.ATTN_11DB)
        self.wlan = network.WLAN(network.STA_IF)

        self.style = b"\033[38;5;0m\033[48;5;252m"
        self.clear = b"\033[0m"

        # THE TEMPLATE (Exactly 40 chars if width=40)
        # We use a literal string so we can count the spaces easily.
        # Index:  0123456789012345678901234567890123456789
        # Text:   UP 0:00 | MEM 000K | WiFi OFF | BAT 000%

        # Build the middle padding dynamically
        left = b"UP 0:00 | MEM      | " # 21 chars
        right = b"WiFi OFF | BAT  00%" # 20 chars

        # If width is 40, mid_pad will be -1. We need to shrink the labels or widen the bar.
        # Let's assume width is 53 (standard for T-Deck with this font)
        mid_pad = b" " * (width - len(left) - len(right))
        template = left + mid_pad + right

        self.buffer = bytearray(self.style + template + self.clear + b"\x00")

        # EXACT OFFSETS (Calculated relative to style length)
        s_len = len(self.style)

        self.off_up   = s_len + 3   # Index of the first '0' in 0:00
        self.off_mem  = s_len + 13  # Index of the first '0' in 000K

        # We calculate from the end of the buffer (before CLEAR and NULL)
        # The last character of the buffer (excluding CLEAR/NULL) is (len - 5)
        content_end = len(self.buffer) - 5

        # "BAT 000% " is 9 chars. The '0' starts 5 chars from the end.
        self.off_bat  = content_end - 4 

        # "WiFi OFF | " is 11 chars. The 'O' starts 11 chars from the BAT start.
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
        # Equivalent to ((volts - 3.2) * 100) using integer scaling
        pct = ((raw * 660) // 65535) - 320
        if pct < 0: pct = 0
        if pct > 100: pct = 100

        # 1. Memory Calculation
        mem_free = gc.mem_free()
        
        if mem_free >= 1048576: # 1024 * 1024
            # Use MB mode (e.g., 7.4M)
            # We scale by 10 to get one decimal place without floats
            mem_scaled = (mem_free * 10) // 1048576 
            val = mem_scaled // 10    # Whole MB
            dec = mem_scaled % 10     # First decimal
            unit = 77                 # ASCII 'M'
            
            # Poke: [Space][Digit][.][Digit][M]
            # Since MB is usually < 10 on T-Deck (8MB total), 
            # we use the first slot for a space or a 1 if you overclock/expand.
            self.buffer[self.off_mem]     = 48 + (val // 10) if val >= 10 else 32
            self.buffer[self.off_mem + 1] = 48 + (val % 10)
            self.buffer[self.off_mem + 2] = 46 # ASCII '.'
            self.buffer[self.off_mem + 3] = 48 + dec
            self.buffer[self.off_mem + 4] = unit
        else:
            # Use KB mode (Original 000K logic)
            mem_k = mem_free // 1024
            mh = (mem_k // 100) % 10
            mt = (mem_k // 10) % 10
            mu = mem_k % 10
            
            self.buffer[self.off_mem]     = 48 + mh if mh > 0 else 32
            self.buffer[self.off_mem + 1] = 48 + mt if (mt > 0 or mh > 0) else 32
            self.buffer[self.off_mem + 2] = 48 + mu
            self.buffer[self.off_mem + 3] = 75 # ASCII 'K'
            self.buffer[self.off_mem + 4] = 32 # Extra space to clear old 'M'

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
