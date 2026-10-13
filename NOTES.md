# Handoff notes

Working notes for picking up development, on any machine or in a new Claude Code session. `README.md` covers the hardware, pins, protocol and build; this file covers where things stand and why. Update it when an item is settled.

Last updated: 2026-10-13.

## Status

- **Builds:** all five presets (`nucleo`, `nucleo-debug`, `nucleo-vcp`, `custom`, `custom-debug`) build with zero warnings. Release builds use ~80 KB flash, debug builds ~250 KB flash and ~31 KB RAM.
- **Debugging works on the Nucleo** (`Debug: nucleo-debug` in VS Code): the debug build reaches `main()`, stops at breakpoints and runs without faults, and USB enumerates.
- **FS-DB01 driver** (`drivers/fsdb01/`) is functional but **its output is not yet verified on hardware**. Check with a logic analyser on the Nucleo's PA10 (Arduino D2). With the no-signal frame (bits 6 and 7), each 30 ms frame should be six short pulses (1 ms high / 2 ms low), two long ones (2 ms / 1 ms), one short, then 3 ms low.
- **`main()`** checks devices, **disables FS-DB01 output at boot**, and blinks `led0`. Run `leds enable` to start output.
- **Shell** (`src/shell_*.cpp`): `leds` drives the driver by hand, `voltage` reads the two voltage sense inputs, `dfu` reboots into the USB DFU bootloader; see README.
  - Release builds (`prj.conf`) turn off `kernel`, `device`, `devmem`, `date`, `retval` and the VT100 commands (`clear`); prompt `> `.
  - Debug builds (`debug.conf`) turn `kernel`, `device`, `devmem` and VT100 back on; prompt `DBG > `.
- **Logging** goes through the shell's log backend (`CONFIG_SHELL_LOG_BACKEND`, no UART log backend), so it always appears where the shell is: CDC ACM, or USART2 in `nucleo-vcp`. `printk()` goes into the log too.
- **DFU flashing works on hardware**, apart from the `EMPTY` gotcha (see below). The MPU fix in the `dfu` jump is in place, but the full `dfu` → `dfu-util` flow hasn't been re-run since.
- **CRSF:** UART is set up (alias `crsf-uart`, 420000 baud), but nothing reads or parses it yet.

## Decisions and why

### FS-DB01 driver

- **Lives in the app, not a Zephyr module** (`drivers/`, binding in `dts/bindings/`, top-level `Kconfig` sources `drivers/Kconfig`). It's only used by this app.
  - Sources are added with `target_sources(app …)`. `zephyr_library()` does **not** work from an app: Zephyr warns and leaves the library out of the link, which shows up as `undefined reference to __device_dts_ord_N`.
- **API:** `fsdb01_set_frame()` (atomic, callable from anywhere, latched at the next frame start), `fsdb01_get_frame()`, `fsdb01_enable()`/`fsdb01_disable()` (mutex, not ISR-safe), `fsdb01_is_enabled()`.
  - No queue or dedicated thread for the driver: only the latest frame matters, so an atomic value is enough.
- **Bit timing:** one-shot `k_timer` re-armed at each edge (ISR context), scheduled with **absolute** deadlines (`K_TIMEOUT_ABS_TICKS`). Relative re-arms add 1 tick per edge in Zephyr 4.4 (`kernel/timeout.c`, `timeout.ticks + 1`), which would stretch bits to ~3.2 ms. The Kconfig selects `TIMEOUT_64BIT` for this. Tick rate is 10 kHz.
  - Fallback if timing proves too loose: a hardware timer in PWM mode (3 ms period, 1 or 2 ms pulse).
- **`invert` DT property** inverts every level (frames, gap, disabled state, level at boot) for an inverting level shifter. Don't combine it with `GPIO_ACTIVE_LOW` in `out-gpios`; the two cancel out.

### Voltage sensing

- **PA0/PA1 (ADC1 IN0/IN1) on both boards**, behind 100k/33k dividers. Described in devicetree as `voltage-divider` nodes (aliases `vsense0`, `vsense1`) and read through Zephyr's voltage-divider sensor driver, so the divider ratio lives in DT rather than in code. Needs `CONFIG_ADC` and `CONFIG_SENSOR`.
- With `CONFIG_SENSOR` on, the stock Nucleo board's `die_temp` and `vref` sensors (ADC1 channels 12/13) are built in too. Harmless; disable them in the overlay if the space matters.
- Accuracy depends on VDDA being 3.3 V. If that's not good enough, measure VREFINT and correct for it. **Not verified on hardware yet.**

### Build presets and modes

- **Presets combine a board with a mode:** hidden `nucleo-board`/`custom-board` presets set `BOARD`, hidden `debug`/`release` presets set `APP_BUILD_MODE`. `CMakeLists.txt` appends `debug.conf`/`release.conf` to `EXTRA_CONF_FILE` before `find_package(Zephyr)`, so every board gets them. A preset's own `EXTRA_CONF_FILE` is merged after the mode file.
  - A preset-only approach doesn't work: with multiple inheritance, only one preset's `EXTRA_CONF_FILE` survives. `FILE_SUFFIX` doesn't fit either: `prj_debug.conf` replaces `prj.conf` instead of adding to it.
- **`nucleo-vcp`** is a debug build with console/shell on the ST-LINK virtual COM port (USART2, 115200) via `boards/nucleo_vcp_console.overlay`. Avoids wiring a USB connector to PA11/PA12. Production stays CDC-ACM-only.
- **Debug builds use `-O0`, not `-Og`.** The Cortex-M0+ has 4 hardware breakpoints, and at `-Og` single source lines resolved to 2–6 addresses each, so Cortex-Debug failed with `Can not find free FPB Comparator` and stayed in `reset.S`. At `-O0` every line is one address. Stepping (`next`/`finish`) also uses a breakpoint briefly, so keep to about 3 breakpoints.
- **Debug builds need bigger stacks.** At `-O0`, default stacks overflowed (HardFault at `0xaaaaaaaa`, then `arch_system_halt`). `debug.conf` therefore sets `CONFIG_HW_STACK_PROTECTION` (MPU stack guard) and raises `UDC_STM32_STACK_SIZE` to 1024, `MAIN_STACK_SIZE` to 2048, `USBD_THREAD_STACK_SIZE` to 2048 and `SHELL_STACK_SIZE` to 3072. Release builds keep the defaults.
  - Peak use measured on the Nucleo by scanning the `0xaa` stack fill (boot, USB enumeration, idle): `usbd` 1348/2048, `shell_uart` 1740/3072, `main` 1156/2048, `sysworkq` 748/1024, `usb@40005c00` (UDC) 700/1024, `logging` 716/4096, `idle` 128/256. Re-measure (`kernel thread stacks` in debug builds) when adding threads or heavier code. A stop in `arch_system_halt()` most likely means a stack overflow.
- **Launch configs** (`Debug: nucleo-debug`, `Debug: nucleo-vcp`, `Debug: custom-debug`, `Attach: custom-debug`) use the debug builds.

### Flashing

- **OpenOCD on both boards.** The stock Nucleo board defaults to the STM32CubeProgrammer runner, which isn't in the container (ST's download is login-gated, so the Dockerfile can't fetch it). `nucleo-board` sets `BOARD_FLASH_RUNNER=openocd`. Adding CubeProgrammer to the image is deferred; it would need the Linux zip placed in `.devcontainer/` by hand.
- **Firmware update via the `dfu` shell command**, not the BOOT0 pin. Option bytes stay at their factory defaults (`nBOOT_SEL=1`), so PA14-BOOT0 is ignored.
  - `dfu` sets a `.noinit` marker and reboots. A `PRE_KERNEL_1` priority-0 `SYS_INIT` (before the clock driver at priority 1) then disables the MPU and jumps to the system bootloader at `0x1FFF0000`.
  - The MPU must be disabled: Zephyr enables it in `arch_kernel_init()`, before `PRE_KERNEL_1`, and its regions make flash read-only, so the bootloader erased fine and then hung on its first write.
  - The bootloader enumerates as `0483:df11` with three alt settings, all reported as `@Internal Flash /0x08000000/256*02Kg`. Use alt 0, and `-d 0483:df11` so other DFU devices (e.g. the laptop webcam) aren't picked.
  - **Gotcha:** the bootloader leaves `FLASH_ACR.EMPTY` (bit 16) set after the download, so the chip boots into the bootloader again until a real power-on reset (`:leave`, NRST and system resets don't clear it). On the Nucleo, the ST-LINK USB keeps the MCU powered, so unplug that too. Clearing the bit over SWD (`mmw 0x40022000 0 0x00010000`) and resetting also works.
  - Power-cycling from DFU mode without a download starts the existing firmware; only an erase sets `EMPTY`.

## Open items

- **FS-DB01 output pin:** placeholders, PA10 on the Nucleo, PA8 on the custom board.
- **CRSF channel → FS-DB01 bit mapping:** not decided.
- **Failsafe behaviour:** not decided. This includes link-loss detection (timeout, and/or the link-statistics frame), which frame to send, and whether output should stay disabled until the link is up (currently `main()` disables it at boot and nothing enables it).
- **Application structure:** planned as one control thread that receives CRSF frames from the UART (message queue or ring buffer from the ISR), maps them, handles failsafe and calls `fsdb01_set_frame()`. Not started.
- **Hardware verification:** FS-DB01 output timing (see Status), and the full `dfu` → `dfu-util` flow with the MPU fix.
- **`release.conf`** is empty. Release-only Kconfig currently lives in `prj.conf` and is undone in `debug.conf`.
- **USB VID/PID** is still Zephyr's test ID.
- **PCB:** the schematic (`pcb/2026-020-crft-to-fsdb01/`) is being built from `pcb/INSTRUCTIONS.md` (see `pcb/PLAN.md`). When it's done, bring the firmware in line with it:
  - Custom board LEDs (PB0/PB1) are active low: switch the DTS to `GPIO_ACTIVE_LOW`, and relabel them blue (`led0`, PB0) and red-orange (`led1`, PB1) in the DTS and README.
  - Finalize the FS-DB01 output pin (PA8) in the custom board DTS and README.
  - Check the `vsense0`/`vsense1` divider ratio (100k/33k) and ADC pins (PA0/PA1) against the schematic, on both boards.

## Environment notes

- The `devcontainer` CLI on the original host is at `~/.devcontainers/bin/devcontainer`, which isn't on the PATH of non-interactive shells.
- Without the CLI, `docker exec` into the running container also works (workspace at `/workspaces/2026-020-crsf-to-fsdb01`).
- The STM32G0 reference manual (RM0444) is in `Documentation/`.
