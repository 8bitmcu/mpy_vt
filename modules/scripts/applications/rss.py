#
# MicroPython TUI RSS Reader
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import sys
import XML
import requests

def main(env, args):
    """ Creates a TUI for browsing RSS feeds """

    host = args[0] if len(args) > 0 else None
    if not host:
        print("Usage: rss <host>")
        return

    tui = env.tui
    ui_state = "MAIN_MENU"
    tui.enter_altscreen()
    tui.cursor_hide()

    win = tui.make_window(
            0, 0,
            width=env.cols, height=env.rows,
            title="RSS READER",
            fg=252, bg=18)

    win.invalidate()
    win.draw()
    tui.draw()
    win.draw_label(f"Loading...",
                   0, win.inner_h // 2,
                   fg=252, bg=18,
                   align="center")

    titles = []
    try:
        response = requests.get(host)
        rss_data = response.text
        parser = XML.XML()
        items = parser.findall(rss_data, "item")

        for item_xml in items:
            # Extract specific elements from within each item block
            title = parser.find(item_xml, "title")

            # Strip CDATA tags if the RSS feed uses them
            if title and title.startswith("<![CDATA["):
                title = title[9:-3]
            titles.append("- " + title)

    except Exception as e:
        tui.exit_altscreen()
        tui.cursor_show()
        print(f"Error: {e}")
        return

    while True:

        if ui_state == "MAIN_MENU":

            status = win.make_label("[w/s] nav | [q]uit",
                                    0, win.inner_h-1,
                                    fg=0, bg=252,
                                    width=win.inner_w)

            lst = win.make_list(titles,
                                x=0, y=0,
                                width=env.cols, height=win.inner_h-1,
                                fg=252, bg=18,
                                arrow=">", left_pad=1,
                                multiline=True, wrap=True)

            win.invalidate()
            while True:
                win.draw()
                lst.draw()
                status.draw()
                tui.draw()

                char = sys.stdin.read(1)
                if char == "w":
                    lst.up()
                elif char == "s":
                    lst.down()
                elif char == "q":
                    ui_state = "QUIT"
                    break

        elif ui_state == "QUIT":
            tui.exit_altscreen()
            tui.cursor_show()
            return

