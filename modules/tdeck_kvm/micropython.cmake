# Create an INTERFACE library for our C module.
add_library(usermod_tdeck_kvm INTERFACE)

# Add our source files to the lib
target_sources(usermod_tdeck_kvm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/tdeck_kvm.c)

# Add the current directory as an include directory.
target_include_directories(usermod_tdeck_kvm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR})

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_tdeck_kvm)

