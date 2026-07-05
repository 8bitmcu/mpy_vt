
# Create an INTERFACE library for our C module.
add_library(usermod_tu INTERFACE)

# Add our source files to the lib
target_sources(usermod_tu INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tu.c)

# Add the current directory as an include directory.
target_include_directories(usermod_tu INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../vt
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_tu)


