# Define the module and its source files
add_library(usermod_term INTERFACE)

target_sources(usermod_term INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/st_term.c
    ${CMAKE_CURRENT_LIST_DIR}/stub.c
    ${CMAKE_CURRENT_LIST_DIR}/term_module.c
)

# Add the include directory so headers are found
target_include_directories(usermod_term INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/../st7789
)

# Link it to the usermod target
target_link_libraries(usermod INTERFACE usermod_term)
