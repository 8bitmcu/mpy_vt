USERMOD_DIR := $(USERMOD_DIR)
# Add our C file to the build
SRC_USERMOD += $(USERMOD_DIR)/lora.cpp
# Link it to the build system
CFLAGS_USERMOD += -I$(USERMOD_DIR)


