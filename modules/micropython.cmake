# Include the vt module
include(${CMAKE_CURRENT_LIST_DIR}/vt/micropython.cmake)

# Include the display driver module
include(${CMAKE_CURRENT_LIST_DIR}/st7789/micropython.cmake)

# include the keyboard module
include(${CMAKE_CURRENT_LIST_DIR}/tdeck_kbd/micropython.cmake)

# include the keyboard/video module
include(${CMAKE_CURRENT_LIST_DIR}/tdeck_kvm/micropython.cmake)

# include the trackball module
include(${CMAKE_CURRENT_LIST_DIR}/tdeck_trk/micropython.cmake)


