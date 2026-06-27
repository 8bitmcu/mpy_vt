import os

MODULE_DIR = os.path.dirname(__file__)

SCRIPTS_DIR = os.path.join(MODULE_DIR, "scripts")

freeze(SCRIPTS_DIR)
