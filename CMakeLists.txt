cmake_minimum_required(VERSION 3.20)

set(TARGET $ENV{TARGET})
set(CMAKE_TOOLCHAIN_FILE $ENV{IDF_PATH}/tools/cmake/toolchain-${TARGET}.cmake)
include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

project(esp_mcu_server C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

idf_build_set_property(MINIMAL_BUILD ON)
idf_build_process(
    ${TARGET}
    COMPONENTS freertos esp-tls nvs_flash esp_netif esp_http_server esp_wifi esp_eth esp_adc
    SDKCONFIG ${CMAKE_CURRENT_LIST_DIR}/sdkconfig
    BUILD_DIR ${CMAKE_BINARY_DIR}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${CMAKE_PROJECT_NAME}.elf ${CMAKE_CURRENT_LIST_DIR}/src/esp-server.cpp)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}.elf
    PRIVATE
    idf::esp_driver_gpio
)
idf_build_executable(${CMAKE_PROJECT_NAME}.elf)
target_compile_definitions(
    ${CMAKE_PROJECT_NAME}.elf
    PRIVATE
)