TERM_MOD_DIR := $(USERMOD_DIR)

# Add all C files to SRC_USERMOD.
SRC_USERMOD += $(TERM_MOD_DIR)/term_module.c
SRC_USERMOD += $(TERM_MOD_DIR)/st_term.c
SRC_USERMOD += $(TERM_MOD_DIR)/stub.c

# We can add our module folder to include paths if needed
# This is not actually needed in this example.
CFLAGS_USERMOD += -I$(TERM_MOD_DIR)

# Disable the "Stop on Warning" behavior
CFLAGS_USERMOD += -Wno-error

# Silence common Suckless/Legacy C warnings
CFLAGS_USERMOD += -Wno-unused-parameter
CFLAGS_USERMOD += -Wno-sign-compare
CFLAGS_USERMOD += -Wno-unused-result
CFLAGS_USERMOD += -Wno-type-limits
