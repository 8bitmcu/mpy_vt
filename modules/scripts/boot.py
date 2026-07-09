import os
import esp32

try:
    p = esp32.Partition.find(esp32.Partition.TYPE_DATA, label='vfs')[0]
except IndexError:
    print("Error: 'vfs' partition not found in partition table.")

try:
    os.mount(p, '/flash')
    print("Filesystem mounted at /flash")
except OSError as e:
    # Error 19: ENODEV (Unformatted or corrupt)
    # Error 22: EINVAL (Invalid filesystem)
    if e.args[0] in (19, 22):
        print("Filesystem not found. Formatting partition... (Please wait)")
        try:
            os.VfsLfs2.mkfs(p)
            os.mount(p, '/flash')
            print("Format successful and mounted at /flash")
        except Exception as format_err:
            print(f"Format failed: {format_err}")
    else:
        print(f"Unexpected mount error: {e}")

try:
    os.chdir('/flash')

    with open("WELCOME.md", "x") as f:
        f.write(f"# Welcome to MPY_VT!\n"
                f"This is your pocket hackable terminal.\n"
                f"A few things to get you started:\n"
                f" \n"
                f"## Navigation\n"
                f"Trackball up/down  - scroll terminal history\n"
                f"Trackball left/right  - command history (up/down arrow)\n"
                f"Trackball click  - send Escape\n"
                f" \n"
                f"## Tips\n"
                f"- Type any command name and press Enter to run it\n"
                f"- Files live in /flash (internal) and /sd (SD card, if inserted)\n"
                f"- Type exit to get out of the shell, into Micropython repl\n"
                f" \n"
                f"Happy hacking!")

except:
    pass

