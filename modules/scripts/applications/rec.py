#
# MicroPython Audio Recording utility
# supports streaming to some extent
# Copyright (c) 2026 8bitmcu
# License: MIT
#

import sys
import time

def fmt_size(size):
    if size < 1024:
        return f"{size}B"
    elif size < 1024 * 1024:
        return f"{size / 1024:.1f}K"
    elif size < 1024 * 1024 * 1024:
        return f"{size / (1024 * 1024):.1f}M"
    else:
        return f"{size / (1024 * 1024 * 1024):.1f}G"

def main(env, args):

    if not args:
        print("Usage: rec <file> [length]")
        return

    file_name = args[0]
    length = int(args[1]) if len(args) > 1 else 0

    print("Recording...")
    if length > 0:
        env.rec.record(file_name, seconds=length)
        while env.rec.is_recording():
            time.sleep_ms(200)
    else:
        print("Press any key to stop recording.")
        env.rec.record(file_name)
        sys.stdin.read(1)

    print(f"Saving to {file_name}...")
    bytes_written = env.rec.stop()

    print(f"Recorded {fmt_size(bytes_written)}!")
