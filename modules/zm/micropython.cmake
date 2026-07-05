# Create an INTERFACE library for our C module.
add_library(usermod_zm INTERFACE)

# Add our source files to the lib
target_sources(usermod_zm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/zm.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/blorb/blorblib.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/buffer.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/err.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/fastmem.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/files.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/getopt.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/hotkey.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/input.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/main.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/math.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/missing.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/object.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/process.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/quetzal.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/random.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/redirect.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/screen.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/sound.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/stream.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/table.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/text.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/common/variable.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/dumb/dblorb.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/dumb/dinit.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/dumb/dinput.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/dumb/doutput.c
    ${CMAKE_CURRENT_LIST_DIR}/frotz/dumb/dpic.c)


# Add the current directory as an include directory.
target_include_directories(usermod_zm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR})

# Disable the "Stop on Warning"
target_compile_options(usermod_zm INTERFACE -Wno-error=char-subscripts)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_zm)

