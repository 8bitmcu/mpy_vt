IMAGE_NAME = micropython-builder
CONTAINER_WORKDIR = /opt/micropython/ports/unix
USER_MODS_DIR = $(shell pwd)/modules
BUILD_DIR = $(shell pwd)/build_output

CFLAGS = "-DMODULE_TERM_ENABLED=1 -DMICROPY_PY_OS_DUPTERM=1"

.PHONY: all image build shell clean run

image:
	docker build -t $(IMAGE_NAME) .

build:
	@mkdir -p $(BUILD_DIR)
	docker run --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/build-standard \
		$(IMAGE_NAME) \
		make USER_C_MODULES=/opt/my_modules CFLAGS_EXTRA="-DMODULE_TERM_ENABLED=1 -DMICROPY_PY_OS_DUPTERM=1"


run:
	@./build_output/micropython

shell:
	@mkdir -p $(BUILD_DIR)
	docker run -it --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):$(CONTAINER_WORKDIR)/build-standard \
		$(IMAGE_NAME)

repl:
	docker run -it --rm \
		-v $(USER_MODS_DIR):/opt/my_modules \
		-v $(BUILD_DIR):/opt/micropython/ports/unix/build-standard \
		$(IMAGE_NAME) \
		./build-standard/micropython

clean:
	rm -rf $(BUILD_DIR)
