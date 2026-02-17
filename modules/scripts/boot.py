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
except:
    pass

