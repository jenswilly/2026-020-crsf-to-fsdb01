# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Custom board definitions live in ./boards/<vendor>/<board>
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Build mode, set by the CMake presets: adds debug.conf or release.conf on top
# of prj.conf, for every board
set(APP_BUILD_MODE release CACHE STRING "Build mode: debug or release")
set_property(CACHE APP_BUILD_MODE PROPERTY STRINGS debug release)
if(NOT APP_BUILD_MODE MATCHES "^(debug|release)$")
  message(FATAL_ERROR "APP_BUILD_MODE must be debug or release, not '${APP_BUILD_MODE}'")
endif()
list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${APP_BUILD_MODE}.conf)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crsf_to_fsdb01)

target_sources(app PRIVATE 
    src/main.cpp
    src/bootloader.cpp
    src/shell_dfu.cpp
    src/shell_leds.cpp
    src/shell_voltage.cpp
)

# In-tree drivers; bindings are in dts/bindings, Kconfig in drivers/
add_subdirectory(drivers)
target_include_directories(app PRIVATE drivers/fsdb01)
