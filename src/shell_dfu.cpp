/*
 * Shell command to reboot into the USB DFU bootloader
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "bootloader.h"

namespace {

int cmd_dfu(const shell* sh, size_t, char**) {
    shell_print(sh, "Rebooting into the USB DFU bootloader (0483:df11)");
    /* Give the message time to reach the host before USB disconnects */
    k_msleep(100);
    bootloader_reboot();
}

}  // namespace

SHELL_CMD_REGISTER(dfu, NULL, "Reboot into the USB DFU bootloader", cmd_dfu);
