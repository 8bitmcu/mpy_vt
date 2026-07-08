# Path to the `modules` folder, which contains C modules and the python `scripts` folder.
USER_MODS_DIR = $(shell pwd)/modules

# Output folder for the compiled binary files
BUILD_DIR = $(shell pwd)/build_output

# Name of the docker image used for mpremote
MP_REMOTE = micropython-mpremote

# Volume used to store the mpy code
MPY_VOLUME = micropython_src_vol

# Version of microython to compile against (must be a valid github branch)
MPY_BRANCH = v1.28.0

# ESP-IDF Docker image and version
IDF_IMAGE = espressif/idf:v5.5.1

# Used for flashing and debugging, this is the device USB port config
PORT ?= /dev/ttyACM0
BAUD ?= 460800

# Defines which board and it's folder we're targetting
BOARD = LILYGO_T_DECK
BOARD_DIR = $(shell pwd)/boards

.PHONY: init build flash sync_files clean repl core_dump

init:
	docker build -t $(MP_REMOTE) .
	-docker volume rm -f $(MPY_VOLUME)
	docker volume create $(MPY_VOLUME)
	docker run --rm -v $(MPY_VOLUME):/opt/micropython alpine \
		sh -c "apk add --no-cache git && \
			cd /opt/micropython && \
			git clone --depth 1 --branch $(MPY_BRANCH) https://github.com/micropython/micropython.git . && \
			git submodule update --init --recursive || (rm -rf * && exit 1)"

build:
	@mkdir -p $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/*
	docker run --rm \
		-v $(MPY_VOLUME):/opt/micropython \
		-v $(USER_MODS_DIR):/opt/all_modules \
		-v $(BUILD_DIR):/opt/external_build \
		-v $(BOARD_DIR):/opt/boards \
		$(IDF_IMAGE) \
		/bin/bash -c "cp -r /opt/boards/* /opt/micropython/ports/esp32/boards/ && \
			source /opt/esp/idf/export.sh && \
			make -C /opt/micropython/mpy-cross && \
			make -C /opt/micropython/ports/esp32 \
				BOARD=$(BOARD) \
				USER_C_MODULES=/opt/all_modules \
				FROZEN_MANIFEST=/opt/all_modules/manifest.py && \
			cp /opt/micropython/ports/esp32/build-$(BOARD)/firmware.bin /opt/external_build/ && \
			cp /opt/micropython/ports/esp32/build-$(BOARD)/micropython.bin /opt/external_build/ && \
			chown -R $(shell id -u):$(shell id -g) /opt/external_build/."

flash:
	docker run --rm --privileged \
		--device=$(PORT):$(PORT) \
		-v $(BUILD_DIR):/flash_dir \
		$(IDF_IMAGE) \
		/bin/bash -c "esptool.py -p $(PORT) -b $(BAUD) --chip esp32s3 erase_flash && \
			esptool.py -p $(PORT) -b $(BAUD) --chip esp32s3 write_flash 0x0 /flash_dir/firmware.bin"

sync_files:
	docker run --rm -it \
		--privileged \
		-v /dev/bus/usb:/dev/bus/usb \
		-v $(USER_MODS_DIR):/opt/all_modules \
		--device=$(PORT):$(PORT) \
		$(MP_REMOTE) \
		mpremote connect $(PORT) cp -r /opt/all_modules/scripts/ :

sync_file:
	docker run --rm -it \
		--privileged \
		-v /dev/bus/usb:/dev/bus/usb \
		-v $(USER_MODS_DIR):/opt/all_modules \
		--device=$(PORT):$(PORT) \
		$(MP_REMOTE) \
		mpremote connect $(PORT) cp -r /opt/all_modules/scripts/filemgr.py :filemgr.py

clean:
	docker run --rm -v $(MPY_VOLUME):/opt/micropython $(MP_REMOTE) \
		/bin/bash -c "make -C /opt/micropython/mpy-cross clean"

	docker run --rm -v $(MPY_VOLUME):/opt/micropython $(MP_REMOTE) \
		/bin/bash -c "rm -rf /opt/micropython/ports/esp32/build-* && \
			rm -rf /opt/micropython/ports/esp32/boards/$(BOARD)"

	rm -rf $(BUILD_DIR)/*

repl:
	docker run --rm -it \
		--privileged \
		-v /dev/bus/usb:/dev/bus/usb \
		--device=$(PORT):$(PORT) \
		$(MP_REMOTE) \
		mpremote connect $(PORT) repl

core_dump:
	docker run --rm -it --privileged \
		-v /dev/bus/usb:/dev/bus/usb \
		--device=$(PORT):$(PORT) \
		-v $(MPY_VOLUME):/opt/micropython \
		$(IDF_IMAGE) \
		/bin/bash -c "source /opt/esp/idf/export.sh && \
			espcoredump.py --chip esp32s3 --port $(PORT) \
			info_corefile --core-format elf \
			/opt/micropython/ports/esp32/build-$(BOARD)/micropython.elf"
