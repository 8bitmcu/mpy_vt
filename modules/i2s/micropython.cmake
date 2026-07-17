
add_library(usermod_audioplayer INTERFACE)

target_sources(usermod_audioplayer INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/audioplayer.c
)

target_include_directories(usermod_audioplayer INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

idf_component_get_property(mp3player_helix_lib chmorgan__esp-libhelix-mp3 COMPONENT_DIR)
target_link_libraries(usermod_audioplayer INTERFACE ${mp3player_helix_lib})

target_link_libraries(usermod INTERFACE usermod_audioplayer)
