#
# MicroPython TUI Audio Player
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import os
import sys
import time

import applications.c2

def fmt_size(size):
    if size < 1024:
        return f"{size}B"
    elif size < 1024 * 1024:
        return f"{size / 1024:.1f}K"
    elif size < 1024 * 1024 * 1024:
        return f"{size / (1024 * 1024):.1f}M"

def main(env, args):
    """ Creates a TUI for playing mp3/wav/c2 audio files """

    file = args[0] if len(args) > 0 else None

    if not file:
        print("Usage: play <file>")
        return

    file_sz = os.stat(file)[6]

    # If it's a codec2 container (magic-sniffed, not extension-based -- see
    # c2.py, encode() output isn't required to be named ".c2")
    display_name = file
    c2_tmp_file = None
    with open(file, "rb") as f:
        magic = f.read(len(applications.c2.C2_MAGIC))
    if magic == applications.c2.C2_MAGIC:
        c2_tmp_file = file + ".wav"
        print(f"Decoding {file}...")
        applications.c2.decode(file, c2_tmp_file)
        file = c2_tmp_file

    vol = 50

    tui = env.tui
    ui_state = "MAIN_MENU"
    tui.enter_altscreen()
    tui.cursor_hide()

    CLR  = "\x1b[0m"
    BOLD = "\x1b[1m"
    CYAN = "\x1b[38;5;45m"

    env.audio.volume(vol)
    env.audio.play(file)

    # works but is slow...
    #duration = env.audio.duration(file)

    time.sleep_ms(100)

    while env.audio.is_playing():
        win = tui.make_window(
                0, 0,
                width=env.cols, height=env.rows,
                title="AUDIO PLAYER",
                fg=252, bg=18)

        if ui_state == "MAIN_MENU":

            info = env.audio.tags()
            blk = win.make_block(f"{BOLD}file: {CLR}{CYAN}{display_name}{CLR}\n"
                                 f"{fmt_size(file_sz)}\n"
                                 f"\n"
                                 f"{BOLD}{info["title"]}{CLR}\n"
                                 f"{info["artist"]}\n"
                                 f"\n"
                                 #f"Duration {duration}\n"
                                 f"Volume: {vol}%",
                                 0, 0,
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
                    if vol + 10 <= 100:
                        vol = vol + 10
                        env.audio.volume(vol)
                        break
                elif char == "s":
                    if vol - 10 >= 0:
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

            if c2_tmp_file:
                try:
                    os.remove(c2_tmp_file)
                except OSError:
                    pass

            # env.audio.deinit()

            return

