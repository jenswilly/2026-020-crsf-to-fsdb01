/*
 * Reboot into the STM32 system bootloader (USB DFU)
 *
 * Jumping straight from the running application would leave clocks, USB and
 * interrupts configured, which the bootloader doesn't expect. Instead, set a
 * marker in RAM that survives a reset, reboot, and jump very early in the next
 * boot, while the chip is still close to its reset state.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bootloader.h"

#include <cmsis_core.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_system.h>
#include <zephyr/init.h>
#include <zephyr/linker/section_tags.h>
#include <zephyr/sys/reboot.h>

#include <cstdint>

namespace {

/* System memory (built-in bootloader) of the STM32G0B1, see RM0444 and AN2606 */
constexpr uintptr_t system_memory_base = 0x1FFF0000;

constexpr uint32_t bootloader_magic = 0xB007'1DF0;

/* Not cleared at startup, so the value survives the reboot */
__noinit uint32_t boot_request;

[[noreturn]] void jump_to_system_bootloader() {
    const auto* vectors = reinterpret_cast<const volatile uint32_t*>(system_memory_base);
    uint32_t stack = vectors[0];
    auto entry = reinterpret_cast<void (*)()>(vectors[1]);

    /*
     * Zephyr enables the MPU before PRE_KERNEL_1. Its static regions make flash
     * read-only and SRAM non-executable, so the bootloader would fault on its
     * first flash write.
     */
    MPU->CTRL = 0;
    __DSB();
    __ISB();

    /* Map system memory at address 0 and point the vector table at it */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_SYSCFG_SetRemapMemory(LL_SYSCFG_REMAP_SYSTEMFLASH);
    SCB->VTOR = system_memory_base;

    /*
     * Restore the reset state of the core: Zephyr runs on PSP with interrupts
     * locked by now. No interrupts are enabled in the NVIC and SysTick isn't
     * running yet, so unmasking is safe.
     */
    __set_MSP(stack);
    __set_CONTROL(0);
    __ISB();
    __enable_irq();

    entry();
    __builtin_unreachable();
}

/* Runs before the clock driver (PRE_KERNEL_1, priority 1) reconfigures the clocks */
int check_boot_request() {
    if (boot_request == bootloader_magic) {
        boot_request = 0;
        jump_to_system_bootloader();
    }
    return 0;
}

SYS_INIT(check_boot_request, PRE_KERNEL_1, 0);

}  // namespace

void bootloader_reboot() {
    boot_request = bootloader_magic;
    sys_reboot(SYS_REBOOT_WARM);
}
