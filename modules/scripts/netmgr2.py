import sys
import network

def scan_for_networks():
    """Initializes the Wi-Fi interface and gathers clean metadata targets."""

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
            lock = "[P] " if authmode > 0 else "    "
            display_str = f"{lock}{ssid} ({rssi}dBm)"
            menu_data.append((display_str, ssid))

    return menu_data



def menu(tui):
    lst = None
    ui_state = "SCAN_WIFI"
    tui.cursor_hide()

    win = tui.make_window(
            0, 0,
            width=tui.width, height=tui.height,
            title="NETWORK MANAGER",
            fg=252, bg=18)

    while True:
        if ui_state == "INPUT_PWD" and lst is not None:
            win.invalidate()
            win.draw()

            pwd_label = win.make_label(
                    "Input password: for " + networks[lst.selected][1],
                    0, win.inner_h//2,
                    fg=252, bg=18,
                    align="center")

            pwd_label.draw()
            tui.draw()

            while True:
                char = sys.stdin.read(1)
                if char == "q":
                    ui_state = "SCAN_WIFI"
                    break

        elif ui_state == "SCAN_WIFI":
            win.invalidate()

            scanning = win.make_label(
                    "Scanning...",
                    0, win.inner_h//2,
                    fg=252, bg=18,
                    align="center")

            win.draw()
            scanning.draw()
            tui.draw()

            networks = scan_for_networks()
            if networks:
                labels = [n[0] for n in networks]
                lst = win.make_list(
                        labels,
                        title="Select a Network:",
                        x=0, y=2,
                        width=tui.width-10, height=win.inner_h-6,
                        fg=252, bg=18,
                        arrow=">", left_pad=1,
                        align="center")
            else:
                no_networks = win.make_label(
                        "No networks found.",
                        0, win.inner_h//2,
                        fg=252, bg=18,
                        align="center")

            status = win.make_label(
                    "[w/s] up/down | [r] refresh | [q] exit",
                    0, win.inner_h-1,
                    fg=0, bg=252,
                    width=win.inner_w)

            while True:
                win.draw()
                if lst:
                    lst.draw()
                else:
                    no_networks.draw()
                status.draw()
                tui.draw()

                char = sys.stdin.read(1)
                if char == "w" and lst:
                    lst.up()
                elif char == "s" and lst:
                    lst.down()
                elif (char == '\r' or char == '\n') and lst:
                    ui_state = "INPUT_PWD"
                    break
                elif char == "r":
                    break
                elif char == "q":
                    tui.clear_screen()
                    tui.cursor_show()
                    return

