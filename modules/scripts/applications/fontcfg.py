#
# MicroPython TUI Font Configurator
# Copyright (c) 2026 8bitmcu
# License: MIT
#
import sys

FONTS = [
    "cozette_mpy_13",
    "gohu_mpy_11",
    "gohu_mpy_14",
    "scientifica_mpy_10",
    "spleen_mpy_8",
    "spleen_mpy_12",
    "tamzen_mpy_11",
    "terminus_mpy_12",
    "terminus_mpy_14",
    "unifont_mpy_16",
]

def main(env, *args):
    """ Creates a TUI for browsing changing system font """

    tui = env.tui
    ui_state = "MAIN_MENU"
    tui.enter_altscreen()
    tui.cursor_hide()

    CLR  = "\x1b[0m"
    BOLD = "\x1b[1m"
    CYAN = "\x1b[38;5;45m"

    while True:
        win = tui.make_window(
                0, 0,
                width=env.cols, height=env.rows,
                title="FONT CONFIG",
                fg=252, bg=18)

        if ui_state == "MAIN_MENU":

            blk = win.make_block(f"{BOLD}current font: {CLR}{CYAN}{env.font_name}{CLR}\n",
                                 1, 0,
                                 fg=252, bg=18)

            status = win.make_label("[w/s] nav | [enter] apply | [q]uit",
                                    0, win.inner_h-1,
                                    fg=0, bg=252,
                                    width=win.inner_w)

            lst = win.make_list(FONTS,
                                x=0, y=2,
                                width=env.cols, height=win.inner_h-2,
                                fg=252, bg=18,
                                arrow=">", left_pad=1)

            win.invalidate()
            while True:
                win.draw()
                blk.draw()
                lst.draw()
                status.draw()
                tui.draw()

                char = sys.stdin.read(1)
                if char in ('\r', '\n'): # enter key
                    env.update_font(lst.value)
                    break
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
