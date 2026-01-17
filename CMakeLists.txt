cmake_minimum_required(VERSION 3.13)

include(cmake/pico_sdk_import.cmake)

project(pico_jtag_cable C CXX ASM)

pico_sdk_init()

add_executable(pico_jtag_cable
    src/main.c
    src/cmsis_dap.c
    src/usb_descriptors.c
)

target_include_directories(pico_jtag_cable PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(pico_jtag_cable
    pico_stdlib
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(pico_jtag_cable)
