import machine
import time
import network

class StatusBar:
    # Black text on Light Gray background
    STYLE = "\033[38;5;0m\033[48;5;252m"
    CLEAR = "\033[0m"

    def __init__(self, terminal, width=40):
        self.term = terminal
        self.width = width
        self.start_ticks = time.ticks_ms()
        self.bat_pin = machine.ADC(machine.Pin(4))
        self.bat_pin.atten(machine.ADC.ATTN_11DB)

    def get_stats(self):
        # Uptime M:SS
        total_sec = time.ticks_diff(time.ticks_ms(), self.start_ticks) // 1000
        uptime_str = f"{total_sec // 60}:{total_sec % 60:02d}"

        # Battery with Dynamic Coloring
        raw = self.bat_pin.read_u16()
        volts = (raw * 6.6) / 65535
        pct = min(max(int((volts - 3.2) / (4.2 - 3.2) * 100), 0), 100)
        
        # Use dark variants for better contrast on light gray
        if pct > 50:
            bat_color = "\033[38;5;22m"  # Dark Green
        elif pct > 20:
            bat_color = "\033[38;5;130m" # Dark Orange/Yellow
        else:
            bat_color = "\033[38;5;88m"  # Dark Red
        
        bat_str = f"{bat_color}{pct}%{self.STYLE}"

        # WiFi with Dynamic Coloring
        wlan = network.WLAN(network.STA_IF)
        if not wlan.active():
            wifi_str = f"\033[38;5;88mOFF{self.STYLE}" # Dark Red
        elif wlan.isconnected():
            wifi_str = f"\033[38;5;22mON{self.STYLE}"  # Dark Green
        else:
            wifi_str = f"\033[38;5;18m...{self.STYLE}" # Dark Blue
            
        return uptime_str, bat_str, wifi_str

    def refresh(self):
        uptime, battery, wifi = self.get_stats()

        # Build blocks
        left_block = f"UPTIME {uptime}"
        
        # Strip ANSI to calculate visual width of the right side
        # (WiFi ON | BAT 100%)
        # Extract the "ON/OFF" part and the "100%" part
        wifi_vis = "OFF" if "OFF" in wifi else ("ON" if "ON" in wifi else "...")
        bat_vis = battery.split('%')[0].split('m')[-1] + "%"
        
        right_vis_len = 6 + len(wifi_vis) + 3 + 5 + len(bat_vis) + 1
        
        padding = " " * max(0, self.width - len(left_block) - right_vis_len)

        # Re-inject self.STYLE after variables to keep the gray background bar solid
        bar_text = (
            f"{self.STYLE}{left_block}{padding}   "
            f"WiFi {wifi} | "
            f"BAT {battery} "
            f"{self.CLEAR}"
        )
        self.term.top_bar(bar_text)
