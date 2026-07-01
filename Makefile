IMAGE_NAME = micropython-builder
CONTAINER_WORKDIR = /opt/micropython
USER_MODS_DIR = $(shell pwd)/modules
BUILD_DIR = $(shell pwd)/build_output

# Define the ESP32 port path inside the container
ESP32_PORT_DIR = $(CONTAINER_WORKDIR)/ports/esp32
ESP_IMAGE = espressif/idf:v5.5.1
MPY_SOURCE_DIR = $(shell pwd)/../micropython

PORT ?= /dev/ttyACM0
BAUD ?= 460800
BOARD = LILYGO_T_DECK
BOARD_DIR = $(shell pwd)/boards/$(BOARD)

.PHONY: all image build shell clean run

image:
	docker build -t $(IMAGE_NAME) .

build:
	@mkdir -p $(BUILD_DIR)
	docker run --rm \
		-v $(USER_MODS_DIR):/opt/all_modules \
		-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/ports/unix/build-standard \
		$(IMAGE_NAME) \
		make USER_C_MODULES=/opt/all_modules CFLAGS_EXTRA="-DMODULE_TERM_ENABLED=1 -DMICROPY_PY_OS_DUPTERM=1"

build_unix:
	@mkdir -p $(BUILD_DIR)_unix
	docker run --rm \
			-v $(MPY_SOURCE_DIR):/opt/micropython \
			-v $(USER_MODS_DIR):/opt/manifest \
			-v $(USER_MODS_DIR)/vi:/opt/all_modules/vi \
			-v $(USER_MODS_DIR)/scripts:/opt/all_modules/scripts \
			-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/ports/unix/build-standard \
			$(IMAGE_NAME) \
			/bin/bash -c "cd /opt/micropython && \
			make -C mpy-cross && \
			make -C ports/unix USER_C_MODULES=/opt/all_modules FROZEN_MANIFEST=/opt/manifest/manifest.py"

build_esp32_base:
	@mkdir -p $(BUILD_DIR)_base
	docker run --rm \
		-v $(MPY_SOURCE_DIR):/opt/micropython \
		-v $(BUILD_DIR)_base:/opt/micropython/ports/esp32/build-$(BOARD) \
		$(ESP_IMAGE) \
		/bin/bash -c "git config --global --add safe.directory '*' && \
		cd /opt/micropython && \
		source /opt/esp/idf/export.sh && \
		make -C mpy-cross && \
		make -C ports/esp32 BOARD=$(BOARD)"

build_esp32:
	@mkdir -p $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/*
	cp -r $(BOARD_DIR) $(MPY_SOURCE_DIR)/ports/esp32/boards/
	docker run --rm \
		-v $(MPY_SOURCE_DIR):/opt/micropython \
		-v $(USER_MODS_DIR):/opt/all_modules \
		-v $(BUILD_DIR):/opt/external_build \
		$(ESP_IMAGE) \
		/bin/bash -c "source /opt/esp/idf/export.sh && \
			make -C /opt/micropython/mpy-cross clean && \
			make -C /opt/micropython/mpy-cross && \
			make -C /opt/micropython/ports/esp32 \
				BOARD=$(BOARD) \
				USER_C_MODULES=/opt/all_modules \
				FROZEN_MANIFEST=/opt/all_modules/manifest.py && \
			cp /opt/micropython/ports/esp32/build-$(BOARD)/firmware.bin /opt/external_build/ && \
			cp /opt/micropython/ports/esp32/build-$(BOARD)/micropython.bin /opt/external_build/ && \
			cp /opt/micropython/ports/esp32/build-$(BOARD)/micropython.elf /opt/external_build/ && \
			chown -R $(shell id -u):$(shell id -g) /opt/external_build/."

flash:
	docker run --rm --privileged \
		--device=$(PORT):$(PORT) \
		-v $(BUILD_DIR):/flash_dir \
		$(ESP_IMAGE) \
		/bin/bash -c "source /opt/esp/idf/export.sh && \
			esptool.py --chip esp32s3 --port $(PORT) --baud $(BAUD) erase_flash && \
			esptool.py --chip esp32s3 --no-stub --port $(PORT) --baud $(BAUD) \
				--before default_reset --after hard_reset write_flash \
				-z --flash_mode dio --flash_freq 80m --flash_size 16MB \
				0x0 /flash_dir/firmware.bin"


copy-files:
	mpremote connect $(PORT) cp ./modules/scripts/main.py :main.py
	mpremote connect $(PORT) cp ./modules/scripts/status.py :status.py

shell:
	@mkdir -p $(BUILD_DIR)
	docker run -it --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/ports/unix/build-standard \
		$(IMAGE_NAME)

repl:
	docker run -it --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):/opt/micropython/ports/unix/build-standard \
		$(IMAGE_NAME) \
		./build-standard/micropython

clean:
	docker run --rm \
		-v $(MPY_SOURCE_DIR):/opt/micropython \
		$(ESP_IMAGE) \
		/bin/bash -c "cd /opt/micropython/mpy-cross && make clean"
	
	docker run --rm \
		-v $(MPY_SOURCE_DIR):/opt/micropython \
		$(ESP_IMAGE) \
		/bin/bash -c "rm -rf /opt/micropython/ports/esp32/build-*"

	rm -rf $(MPY_SOURCE_DIR)/ports/esp32/boards/$(BOARD)

	docker run --rm \
		-v $(MPY_SOURCE_DIR):/opt/micropython \
		-v $(BUILD_DIR):/opt/external_build \
		$(ESP_IMAGE) \
		/bin/bash -c "make -C /opt/micropython/mpy-cross clean && \
									rm -rf /opt/micropython/ports/esp32/build* && \
									rm -rf /opt/external_build/*"

core_dump:
	docker run --rm \
		--device=$(PORT):$(PORT) \
		-v $(BUILD_DIR):/opt/external_build \
		$(ESP_IMAGE) \
		/bin/bash -c "esp-coredump --chip esp32s3 --port $(PORT) info_corefile /opt/external_build/micropython.elf"
