# SPDX-License-Identifier: Apache-2.0

# Built into the app library: zephyr_library() only works in Zephyr modules
target_sources(app PRIVATE fsdb01.c)
