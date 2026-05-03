cmake_minimum_required(VERSION 3.20)

set(TARGET $ENV{TARGET})
set(CMAKE_TOOLCHAIN_FILE $ENV{IDF_PATH}/tools/cmake/toolchain-${TARGET}.cmake)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

project(esp_mcu_server C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

idf_build_set_property(MINIMAL_BUILD ON)
idf_build_process(
    ${TARGET}
    COMPONENTS nvs_flash esp_netif esp_http_server esp_wifi esp-tls console log
    SDKCONFIG ${CMAKE_CURRENT_LIST_DIR}/sdkconfig
    SDKCONFIG_DEFAULTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults
    BUILD_DIR ${CMAKE_BINARY_DIR}
)

set(NANOIPC_LINUX_FEATURES OFF)
set(NANOIPC_TESTS OFF)
include(FetchContent)
FetchContent_Declare(
    nanoipc
    GIT_REPOSITORY  https://github.com/fedddot/nanoipc.git
    GIT_TAG         main
)
FetchContent_MakeAvailable(nanoipc)

set(GENERATED_API_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
	OUTPUT ${GENERATED_API_DIR}/service.pb.c ${GENERATED_API_DIR}/service.pb.h
	DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/service.proto
	COMMAND ${Python_EXECUTABLE} ${nanopb_SOURCE_DIR}/generator/nanopb_generator.py -I ${CMAKE_CURRENT_LIST_DIR}/src -D ${GENERATED_API_DIR} service.proto
)

add_custom_target(
	generate_service_api
	DEPENDS ${GENERATED_API_DIR}/service.pb.c ${GENERATED_API_DIR}/service.pb.h
)

add_library(service_api INTERFACE)
add_dependencies(service_api generate_service_api)
target_sources(service_api INTERFACE ${GENERATED_API_DIR}/service.pb.c)
target_include_directories(service_api INTERFACE ${GENERATED_API_DIR})

add_library(http_request_data_reader INTERFACE)
target_include_directories(http_request_data_reader INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(
    http_request_data_reader
    INTERFACE
    idf::esp_http_server
    reader
)

add_library(http_request_data_writer INTERFACE)
target_include_directories(http_request_data_writer INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(
    http_request_data_writer
    INTERFACE
    idf::esp_http_server
    writer
)

add_library(nvs_flash_guard INTERFACE)
target_include_directories(nvs_flash_guard INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(
    nvs_flash_guard
    INTERFACE
    idf::nvs_flash
)

add_library(wifi_access_point_guard INTERFACE)
target_include_directories(wifi_access_point_guard INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(
    wifi_access_point_guard
    INTERFACE
    nvs_flash_guard
    idf::esp_netif
    idf::esp_wifi
)

add_library(wifi_station_guard INTERFACE)
target_include_directories(wifi_station_guard INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(
    wifi_station_guard
    INTERFACE
    nvs_flash_guard
    idf::esp_netif
    idf::esp_wifi
)

add_executable(${CMAKE_PROJECT_NAME}.elf ${CMAKE_CURRENT_LIST_DIR}/src/esp-server.cpp)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}.elf
    PRIVATE
    idf::esp_driver_gpio
    idf::esp_http_server
    idf::esp_driver_gpio

    wifi_access_point_guard
    wifi_station_guard

    http_request_data_reader
    pb_message_reader

    http_request_data_writer
    pb_message_writer

    service_api
)

if(NOT DEFINED NETWORK_SSID)
    message(FATAL_ERROR "NETWORK_SSID is not defined. Please pass it to the cmake command")
endif()

if(NOT DEFINED NETWORK_PASSWORD)
    message(FATAL_ERROR "NETWORK_PASSWORD is not defined. Please pass it to the cmake command")
endif()
if(NOT DEFINED SERVICE_PORT)
    message(FATAL_ERROR "SERVICE_PORT is not defined.")
endif()

idf_build_executable(${CMAKE_PROJECT_NAME}.elf)
target_compile_definitions(
    ${CMAKE_PROJECT_NAME}.elf
    PRIVATE
    NETWORK_SSID=\"${NETWORK_SSID}\"
    NETWORK_PASSWORD=\"${NETWORK_PASSWORD}\"
    SERVICE_PORT=${SERVICE_PORT}
)