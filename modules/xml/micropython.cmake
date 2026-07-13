# Create an INTERFACE library for our C module.
add_library(usermod_xml INTERFACE)

# Add our source files to the lib
target_sources(usermod_xml INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/xml.c
    ${CMAKE_CURRENT_LIST_DIR}/yxml.c)

# Add the current directory as an include directory.
target_include_directories(usermod_xml INTERFACE
    ${CMAKE_CURRENT_LIST_DIR})

# Disable the "Stop on Warning"
target_compile_options(usermod_xml INTERFACE -Wno-error=char-subscripts)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_xml)

