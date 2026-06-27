set(IDF_TARGET esp32s3)

set(SDKCONFIG_DEFAULTS
    ${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.base
    ${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.ble
    ${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.spiram_sx
    ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.board
    ${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.240mhz
    ${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.spiram_oct
)


