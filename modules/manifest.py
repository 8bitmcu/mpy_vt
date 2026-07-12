import os

# Read the path passed by the build system.
# Fall back to your local path if the environment variable isn't found.

mpy_dir = os.getenv("MPY_USER_DIR", "/opt/micropython")
include(mpy_dir + "/extmod/asyncio/manifest.py")

base_path = os.getenv("MPY_USER_MODULES_DIR", "/opt/all_modules")
freeze(base_path + "/scripts")
