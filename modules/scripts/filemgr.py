import sys

def main(tui):
    """ Creates a TUI for browsing and manipulating files """

    ui_state = "MAIN_MENU"
    tui.enter_altscreen()
    tui.cursor_hide()

    win = tui.make_window(
            0, 0,
            width=tui.width, height=tui.height,
            title="FILE MANAGER",
            fg=252, bg=18)

    while True:
        if ui_state == "MAIN_MENU":
            win.invalidate()
            win.draw()
            tui.draw()

            while True:
                char = sys.stdin.read(1)
                if char == "q":
                    ui_state  = "QUIT"
                    break

        if ui_state == "QUIT":
            tui.exit_altscreen()
            tui.cursor_show()
            return






