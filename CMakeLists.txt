cmake_minimum_required(VERSION 3.12)

# Project information
project(PaumIoT
    VERSION 1.0.0
    DESCRIPTION "IoT Protocol Middleware with MQTT/CoAP Adapters"
    LANGUAGES C CXX
)

# C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# CXX standard for tests
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Architecture detection
if(NOT DEFINED ARCH)
    set(ARCH "x86_64")
endif()

# Compiler flags
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG -O0")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific configurations
if(ARCH STREQUAL "esp32")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain/esp32.cmake)
    add_definitions(-DESP32_TARGET)
elseif(ARCH STREQUAL "stm32")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain/stm32.cmake)
    add_definitions(-DSTM32_TARGET)
elseif(ARCH STREQUAL "arm")
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon")
endif()

# Optional dependencies
option(USE_PAHO_MQTT "Use Eclipse Paho MQTT library" OFF)
option(USE_LIBCOAP "Use libcoap library" OFF)
option(USE_MBEDTLS "Use mbedTLS for security" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build example applications" ON)

# Find packages
find_package(Threads REQUIRED)

if(USE_PAHO_MQTT)
    find_package(PahoMqttC REQUIRED)
    add_definitions(-DUSE_PAHO_MQTT)
endif()

if(USE_LIBCOAP)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(COAP REQUIRED libcoap-2)
    add_definitions(-DUSE_LIBCOAP)
endif()

if(USE_MBEDTLS)
    find_package(MbedTLS REQUIRED)
    add_definitions(-DUSE_MBEDTLS)
endif()

# Source files by layer
file(GLOB_RECURSE SMP_SOURCES "src/smp/*/*.c")
file(GLOB_RECURSE PDL_SOURCES "src/pdl/*/*.c")
file(GLOB_RECURSE PAL_SOURCES "src/pal/*/*.c")
file(GLOB_RECURSE BLL_SOURCES "src/bll/*/*.c")
file(GLOB_RECURSE DAL_SOURCES "src/dal/*/*.c")

set(ALL_SOURCES 
    ${SMP_SOURCES}
    ${PDL_SOURCES}
    ${PAL_SOURCES}
    ${BLL_SOURCES}
    ${DAL_SOURCES}
)

# Create libraries for each layer
add_library(smp_layer STATIC ${SMP_SOURCES})
add_library(pdl_layer STATIC ${PDL_SOURCES})
add_library(pal_layer STATIC ${PAL_SOURCES})
add_library(bll_layer STATIC ${BLL_SOURCES})
add_library(dal_layer STATIC ${DAL_SOURCES})

# Main executable
add_executable(${PROJECT_NAME} src/main.c)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    smp_layer
    pdl_layer
    pal_layer
    bll_layer
    dal_layer
    Threads::Threads
    m
)

# Optional library linking
if(USE_PAHO_MQTT)
    target_link_libraries(${PROJECT_NAME} PahoMqttC::paho-mqtt3c)
endif()

if(USE_LIBCOAP)
    target_link_libraries(${PROJECT_NAME} ${COAP_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${COAP_INCLUDE_DIRS})
endif()

if(USE_MBEDTLS)
    target_link_libraries(${PROJECT_NAME} 
        MbedTLS::mbedtls 
        MbedTLS::mbedx509 
        MbedTLS::mbedcrypto
    )
endif()

# Testing
if(BUILD_TESTS)
    enable_testing()
    
    file(GLOB TEST_SOURCES "tests/*.c")
    
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME}
            smp_layer
            pdl_layer
            pal_layer
            bll_layer
            dal_layer
            Threads::Threads
            m
        )
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.h"
)

# CPack configuration
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "PaumIoT Project")
set(CPACK_PACKAGE_CONTACT "contact@paumiot.org")

include(CPack)

# Custom targets
add_custom_target(format
    COMMAND find ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests -name "*.c" -o -name "*.h" | xargs clang-format -i
    COMMENT "Formatting source code"
)

add_custom_target(lint
    COMMAND find ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests -name "*.c" -o -name "*.h" | xargs cpplint
    COMMENT "Running linter"
)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_SOURCE_DIR}/config/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project:      ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Architecture: ${ARCH}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags:      ${CMAKE_C_FLAGS}")
message(STATUS "  Use Paho:     ${USE_PAHO_MQTT}")
message(STATUS "  Use libCoAP:  ${USE_LIBCOAP}")
message(STATUS "  Use mbedTLS:  ${USE_MBEDTLS}")
message(STATUS "  Build Tests:  ${BUILD_TESTS}")
message(STATUS "")