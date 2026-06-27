set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    ${MICROPY_PORT_DIR}/boards/sdkconfig.base
    ${MICROPY_PORT_DIR}/boards/sdkconfig.ble
    ${MICROPY_PORT_DIR}/boards/sdkconfig.spiram_sx
    ${MICROPY_BOARD_DIR}/sdkconfig.board
    ${MICROPY_PORT_DIR}/boards/sdkconfig.240mhz
    ${MICROPY_PORT_DIR}/boards/sdkconfig.spiram_oct
)


