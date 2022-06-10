set(DEVICE_SOURCES
)

add_executable(device
    device.cpp
    ${DEVICE_SOURCES}
)


target_include_directories(device
    PRIVATE
    ${pico_sdk_SOURCE_DIR}/src/common/pico_stdlib/include
)


target_link_libraries(device
    PRIVATE
    pico_stdlib
    pico_multicore
)


pico_enable_stdio_usb(device 1)
pico_enable_stdio_uart(device 0)

# pico_generate_pio_header(device ${CMAKE_CURRENT_LIST_DIR}/controller.pio)

pico_add_extra_outputs(device)


target_compile_options(device 
    PRIVATE

    #FEATURES
    $<$<COMPILE_LANGUAGE:CXX>:
    -std=c++20
    -fcoroutines  
    -fconcepts 
    -fconcepts-diagnostics-depth=2 
    -Woverloaded-virtual 
    -Wstrict-null-sentinel 
    -Wctor-dtor-privacy 
    -Wsign-promo  
    -Wstrict-overflow=5 
    -Wcast-qual 
    -Wsign-conversion 
    -Wswitch-default 
    >

    -O2 
    -ggdb

    # WARNINGS
    -Wpedantic  
    -Wall 
    -Wextra 
    -Werror 
    -Winit-self 
    -Wcast-align 
    -Wdisabled-optimization 
    -Wlogical-op 
    -Wmissing-include-dirs 
    -Wfloat-equal 
    -Wredundant-decls   
    -Wno-pedantic 
)


target_link_options(device
    PRIVATE
)


target_compile_definitions(device 
    PRIVATE 
)