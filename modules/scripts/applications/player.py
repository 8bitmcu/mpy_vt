#
# MicroPython TUI Audio Player
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import os
import sys
import time
import audioplayer

def fmt_size(size):
    if size < 1024:
        return f"{size}B"
    elif size < 1024 * 1024:
        return f"{size / 1024:.1f}K"
    elif size < 1024 * 1024 * 1024:
        return f"{size / (1024 * 1024):.1f}M"

def main(env, args):
    """ Creates a TUI for playing mp3/wav audio files """

    file = args[0] if len(args) > 0 else None

    if not file:
        print("Usage: play <file>")
        return

    file_sz = os.stat(file)[6]

    vol = 50

    tui = env.tui
    ui_state = "MAIN_MENU"
    tui.enter_altscreen()
    tui.cursor_hide()

    CLR  = "\x1b[0m"
    BOLD = "\x1b[1m"
    CYAN = "\x1b[38;5;45m"

    if not file:
        print("Usage: play <file>")
        return

    env.audio.volume(vol)
    env.audio.play(file)

    time.sleep_ms(100)

    while env.audio.is_playing():
        win = tui.make_window(
                0, 0,
                width=env.cols, height=env.rows,
                title="AUDIO PLAYER",
                fg=252, bg=18)

        if ui_state == "MAIN_MENU":

            info = env.audio.tags()
            blk = win.make_block(f"{BOLD}file: {CLR}{CYAN}{file}{CLR}\n"
                                 f"{fmt_size(file_sz)} - {info["bitrate"]}kbps\n"
                                 f"\n"
                                 f"{BOLD}{info["title"]}{CLR}\n"
                                 f"{info["artist"]}\n"
                                 f"\n"
                                 f"Duration {info["duration"]}\n"
                                 f"Volume {vol}%",
                                 1, 0,
                                 fg=252, bg=18,
                                 wrap=True)

            status = win.make_label("[w/s] vol | [p]ause/resume | [q]uit",
                                    0, win.inner_h-1,
                                    fg=0, bg=252,
                                    width=win.inner_w)

            win.invalidate()
            while True:
                win.draw()
                blk.draw()
                status.draw()
                tui.draw()

                char = sys.stdin.read(1)
                if char == "w":
                    vol = vol + 10
                    env.audio.volume(vol)
                    break
                elif char == "s":
                    vol = vol - 10
                    env.audio.volume(vol)
                    break
                elif char == "p":
                    if env.audio.is_paused():
                        env.audio.resume()
                    else:
                        env.audio.pause()
                    break
                elif char == "q":
                    ui_state = "QUIT"
                    break

        elif ui_state == "QUIT":
            tui.exit_altscreen()
            tui.cursor_show()

            env.audio.stop()
            err = env.audio.last_error()
            if err != 0:
                print("playback ended with error code", err)

            # env.audio.deinit()

            return

