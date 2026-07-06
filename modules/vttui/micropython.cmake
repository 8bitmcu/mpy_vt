
# Create an INTERFACE library for our C module.
add_library(usermod_vttui INTERFACE)

# Add our source files to the lib
target_sources(usermod_vttui INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/vttui.c)

# Add the current directory as an include directory.
target_include_directories(usermod_vttui INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../vt
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_vttui)


