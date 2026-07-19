# Add your new C++ module
add_library(usermod_lora INTERFACE)
target_sources(usermod_lora INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/lora.cpp
  ${CMAKE_CURRENT_LIST_DIR}/EspHal.cpp
)
target_include_directories(usermod_lora INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_compile_options(usermod_lora INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions>)

idf_component_get_property(radiolib jgromes__radiolib COMPONENT_DIR)
target_link_libraries(usermod_lora INTERFACE ${radiolib})

# Register the module with MicroPython
target_link_libraries(usermod INTERFACE usermod_lora)
