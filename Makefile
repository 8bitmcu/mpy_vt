IMAGE_NAME = micropython-builder
CONTAINER_WORKDIR = /opt/micropython
USER_MODS_DIR = $(shell pwd)/modules
BUILD_DIR = $(shell pwd)/build_output

# Define the ESP32 port path inside the container
ESP32_PORT_DIR = $(CONTAINER_WORKDIR)/ports/esp32
ESP_IMAGE = espressif/idf:v5.5.1
MPY_SOURCE_DIR = $(shell pwd)/../micropython

USER_MODS_DIR = $(shell pwd)/modules

PORT ?= /dev/ttyACM0
BAUD ?= 460800
BOARD = ESP32_GENERIC_S3

.PHONY: all image build shell clean run

image:
	docker build -t $(IMAGE_NAME) .

build:
	@mkdir -p $(BUILD_DIR)
	docker run --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/ports/unix/build-standard \
		$(IMAGE_NAME) \
		make USER_C_MODULES=/opt/my_modules CFLAGS_EXTRA="-DMODULE_TERM_ENABLED=1 -DMICROPY_PY_OS_DUPTERM=1"

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
	docker run --rm \
			-v $(MPY_SOURCE_DIR):/opt/micropython \
			-v $(USER_MODS_DIR):/opt/all_modules \
			-v $(BUILD_DIR):/opt/micropython/ports/esp32/build-$(BOARD) \
			$(ESP_IMAGE) \
			/bin/bash -c "source /opt/esp/idf/export.sh && \
				cd /opt/micropython/mpy-cross && \
				make && \
				cd /opt/micropython/ports/esp32 && \
				make BOARD=$(BOARD) \
						 FROZEN_MANIFEST=/opt/all_modules/manifest.py \
						 USER_C_MODULES=/opt/all_modules"

flash:
	docker run --rm \
		--device=$(PORT):$(PORT) \
		-v $(BUILD_DIR):/opt/micropython/ports/esp32/build-$(BOARD) \
		$(ESP_IMAGE) \
		/bin/bash -c "source /opt/esp/idf/export.sh && \
		esptool.py --port $(PORT) erase_flash && \
		esptool.py --chip esp32s3 --port $(PORT) --baud $(BAUD) \
			--before default_reset --after hard_reset write_flash \
			-z --flash_mode dout --flash_freq 80m --flash_size 16MB \
			0x0 /opt/micropython/ports/esp32/build-$(BOARD)/firmware.bin"


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
	rm -rf $(BUILD_DIR)
