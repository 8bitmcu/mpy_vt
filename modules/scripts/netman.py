import sys
import network
import time

class TerminalMenu:
    def __init__(self, items, max_visible=10):
        self.items = items  # List of tuples: (display_string, ssid, authmode)
        self.max_visible = min(max_visible, len(items))
        self.current_sel = 0  # Absolute index of selected item
        self.top_view = 0     # Index of the first item currently visible

    def draw(self):
        """Renders the scrollable menu using pure ASCII characters."""
        sys.stdout.write("\x1b[?25l") # Hide cursor
        sys.stdout.write("\x1b[s")     # Save cursor position

        for i in range(self.max_visible):
            item_idx = self.top_view + i
            display_text, _, _ = self.items[item_idx]

            # Leave exactly 3 spaces for structural borders and scroll track
            truncated_text = display_text[:35]
            sys.stdout.write("\r\x1b[K") # Clear current line

            # --- Calculate Sidebar Scrollbar Layout (ASCII style) ---
            num_items = len(self.items)
            scrollbar_char = "|"  # Sleek ASCII line track
            if num_items > self.max_visible:
                handle_pos = int((self.current_sel / (num_items - 1)) * (self.max_visible - 1))
                if i == handle_pos:
                    scrollbar_char = "#" # Standard ASCII block handle

            # --- Custom Padding Workaround for MicroPython ---
            padding_spaces = " " * (35 - len(truncated_text))
            padded_text = truncated_text + padding_spaces

            if item_idx == self.current_sel:
                # Active Selection Bar: High-contrast Dark Charcoal Strip + Cyan Highlight Accent Text
                sys.stdout.write("\x1b[1;38;5;51;48;5;236m > " + padded_text + "\x1b[0;38;5;244;48;5;236m" + scrollbar_char + "\x1b[0m\n")
            else:
                # Inactive Rows: Subtle Slate-grey Text on baseline Terminal Canvas
                sys.stdout.write("\x1b[38;5;246m   " + padded_text + "\x1b[38;5;240m" + scrollbar_char + "\x1b[0m\n")

        sys.stdout.write("\x1b[u\x1b[?25h") # Restore cursor / show

    def move_up(self):
        if self.current_sel > 0:
            self.current_sel -= 1
            if self.current_sel < self.top_view:
                self.top_view = self.current_sel
            self.draw()

    def move_down(self):
        if self.current_sel < len(self.items) - 1:
            self.current_sel += 1
            if self.current_sel >= self.top_view + self.max_visible:
                self.top_view = self.current_sel - self.max_visible + 1
            self.draw()

    def get_selection(self):
        return self.items[self.current_sel]


def scan_for_networks():
    """Initializes the Wi-Fi interface and gathers clean metadata targets."""
    sys.stdout.write("\x1b[2J\x1b[H")

    # Unified Loading Notification matching style guide
    sys.stdout.write("\x1b[1;37;48;5;18m           NETWORK MANAGER            \x1b[0m\n")
    sys.stdout.write("\x1b[36m\n  [*] Initializing hardware radio...\n")
    sys.stdout.write("  [*] Scanning regional spectrum...\x1b[0m")

    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    raw_scan = wlan.scan()

    sorted_scan = sorted(raw_scan, key=lambda x: x[3], reverse=True)

    menu_data = []
    for ap in sorted_scan:
        ssid = ap[0].decode('utf-8').strip()
        rssi = ap[3]
        authmode = ap[4]

        if ssid:
            lock = " [P]" if authmode > 0 else ""
            display_str = f"{ssid}{lock} ({rssi}dBm)"
            menu_data.append((display_str, ssid, authmode))

    if not menu_data:
        menu_data.append(("No networks found. Rescan.", "", 0))

    return menu_data


def get_password_input():
    """Renders an input dialog block matching the terminal styling rule frameworks."""
    # Place password layout window perfectly starting at row coordinate line 14 using standard ASCII
    sys.stdout.write("\x1b[14;1H\x1b[K")
    sys.stdout.write("\x1b[1;38;5;214m+--------------------------------------+\x1b[0m\n")
    sys.stdout.write("\x1b[1;38;5;214m| Password: \x1b[0m                           \x1b[1;38;5;214m|\x1b[0m\n")
    sys.stdout.write("\x1b[1;38;5;214m+--------------------------------------+\x1b[0m")

    # Target character window space index box boundaries (Row 15, Column 13)
    sys.stdout.write("\x1b[15;13H\x1b[?25h")

    password = ""
    while True:
        char = sys.stdin.read(1)

        if char == '\r' or char == '\n':  # Submit
            break
        elif char == '\x7f' or char == '\x08':  # Backspace
            if len(password) > 0:
                password = password[:-1]
                sys.stdout.write("\b \b")
        elif len(password) < 25:  # Length validation clamp
            password += char
            sys.stdout.write("*")

    return password


def connect_wifi():
    options = scan_for_networks()

    # Base Terminal Theme Layout Painting Block
    sys.stdout.write("\x1b[2J\x1b[H")
    header_text = "           NETWORK MANAGER            "
    sys.stdout.write(f"\x1b[1;37;48;5;18m{header_text}\x1b[0m\n")
    sys.stdout.write("\x1b[2;38;5;244m W/S: Navigate   |   Enter: Connect\x1b[0m\n\n")

    menu = TerminalMenu(options, max_visible=10)
    menu.draw()

    while True:
        key = sys.stdin.read(1)

        if key == 'w':
            menu.move_up()
        elif key == 's':
            menu.move_down()
        elif key == '\r' or key == '\n':
            display_str, ssid, authmode = menu.get_selection()

            if not ssid:
                break

            password = ""
            if authmode > 0:
                password = get_password_input()

            # Wipe the operational interaction window canvas cleanly
            sys.stdout.write("\x1b[14;1H\x1b[J")

            # ANSI Palettes definitions
            CLR = "\x1b[0m"
            BOLD = "\x1b[1m"
            CYAN = "\x1b[38;5;51m"
            GREEN = "\x1b[38;5;82m"
            YELLOW = "\x1b[38;5;220m"
            RED = "\x1b[38;5;196m"
            WHITE_ON_BLUE = "\x1b[37;48;5;18m"

            sys.stdout.write(f"{CYAN} [*] Initializing handshake with: {BOLD}{ssid}{CLR}\n")
            sys.stdout.write(f" [*] Authenticating ")

            wlan = network.WLAN(network.STA_IF)
            wlan.active(True)

            if wlan.isconnected():
                wlan.disconnect()
                time.sleep(0.5)

            if password:
                wlan.connect(ssid, password)
            else:
                wlan.connect(ssid)

            timeout = 15
            connected = True

            while not wlan.isconnected():
                sys.stdout.write(f"{YELLOW}.{CLR}")
                time.sleep(1)
                timeout -= 1
                if timeout <= 0:
                    connected = False
                    break

            if connected:
                # Render clean, structured reporting metrics output card
                sys.stdout.write(f"\n\n{BOLD}{GREEN} [V] Wi-Fi Successfully Connected!{CLR}\n\n")
                ip, subnet, gateway, dns = wlan.ifconfig()

                sys.stdout.write(f"{WHITE_ON_BLUE}         NETWORK CONFIGURATION          {CLR}\n")
                sys.stdout.write(f" {BOLD}IP Address :{CLR}  {GREEN}{ip}{CLR}\n")
                sys.stdout.write(f" {BOLD}Subnet     :{CLR}  {subnet}\n")
                sys.stdout.write(f" {BOLD}Gateway    :{CLR}  {gateway}\n")
                sys.stdout.write(f" {BOLD}DNS Server :{CLR}  {dns}\n")
                sys.stdout.write(f"\x1b[38;5;240m{'-' * 40}{CLR}\n")
            else:
                sys.stdout.write(f"\n\n{BOLD}{RED} [X] Connection Fault: Handshake Timed Out.{CLR}\n")

            break
