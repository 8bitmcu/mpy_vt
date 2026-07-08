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

    with open("example.md", "x") as f:
        f.write("Hello, T-Deck!\n")
        f.write("==============\n")
        f.write("Lorem ipsum dolor sit amet, consectetur\n")
        f.write("adipiscing elit, sed do eiusmod tempor\n")
        f.write("incididunt ut labore et dolore magna\n")
        f.write("aliqua. Ut enim ad minim veniam, quis\n")
        f.write("nostrud exercitation ullamco laboris\n")
        f.write("nisi ut aliquip ex ea commodo consequat.\n")
except:
    pass

