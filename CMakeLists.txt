# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Custom board definitions live in ./boards/<vendor>/<board>
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crsf_to_fsdb01)

target_sources(app PRIVATE src/main.cpp)

# In-tree drivers; bindings are in dts/bindings, Kconfig in drivers/
add_subdirectory(drivers)
target_include_directories(app PRIVATE drivers/fsdb01)
