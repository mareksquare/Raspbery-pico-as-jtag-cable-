# This is a standard import file for Pico SDK

include(FetchContent)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk")

if (NOT EXISTS ${PICO_SDK_PATH})
    message(FATAL_ERROR "Pico SDK not found at: ${PICO_SDK_PATH}")
endif()

set(PICO_SDK_FETCH_FROM_GIT_PATH ${PICO_SDK_PATH})

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
