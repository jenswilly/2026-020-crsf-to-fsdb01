/*
 * Reboot into the STM32 system bootloader (USB DFU)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Reboot into ST's built-in system bootloader. It enumerates on USB as
 * 0483:df11 (DFU), so new firmware can be loaded with dfu-util. Does not
 * return.
 */
[[noreturn]] void bootloader_reboot();
