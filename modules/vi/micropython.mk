
USERMOD_DIR := $(USERMOD_DIR)
# Add our C file to the build
SRC_USERMOD += $(USERMOD_DIR)/vi_module.c
SRC_USERMOD += $(USERMOD_DIR)/vi.c
# Link it to the build system
CFLAGS_USERMOD += -I$(USERMOD_DIR)


# Disable the "Stop on Warning" behavior
CFLAGS_USERMOD += -Wno-error

# Silence common Suckless/Legacy C warnings
CFLAGS_USERMOD += -Wno-unused-parameter
CFLAGS_USERMOD += -Wno-sign-compare
CFLAGS_USERMOD += -Wno-unused-result
CFLAGS_USERMOD += -Wno-type-limits

