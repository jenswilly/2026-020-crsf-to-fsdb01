# Project guidelines for Claude

See `README.md` for the project overview, hardware, pin assignments, protocol details and build commands.

## What this project is

A bridge that reads **CRSF** from an **ExpressLRS receiver** (UART, 420000 baud) and drives a **FlySky FS-DB01** LED control module over its proprietary single-wire, pulse-width-coded protocol. That protocol is a 9-bit frame with 3 ms bit cells plus a 3 ms low gap.

- The FS-DB01 protocol is known only from the reverse-engineered reference implementation https://github.com/osos11-Git/F401_FMS_FCX10_LED_REVERSE (`Core/Src/main.c`, STM32F401 HAL). Treat it as the source of truth, and check the code there before changing timing or bit meanings. The README's protocol section is a summary of it.
- The FS-DB01 output pin is not assigned yet. When it is, expose it as a devicetree alias (e.g. `fsdb01-out`) on both boards.
- The CRSF channel → FS-DB01 bit mapping and the failsafe behavior are still open design decisions. Ask the user rather than inventing them.

## Toolchain and building

- Zephyr v4.4.2, Zephyr SDK 1.0.1 (GCC 14.3), and all tools live **only in the devcontainer**, not on the host.
- From the host, run commands in the running devcontainer:
  `devcontainer exec --workspace-folder . bash -c 'cmake --preset custom && cmake --build --preset custom'`
  If no container is running, ask the user to start it rather than building an image yourself.
- **Use CMake presets, not the nRF Connect extension or `west build`.** Presets `nucleo` and `custom` in `CMakePresets.json` build into `build_nucleo/` and `build_custom/` (gitignored via `build*/`). Use Zephyr targets such as `flash`, `menuconfig` and `ram_report` through `cmake --build --preset <name> --target <target>`.
- Board-specific build settings (board, extra overlays/conf files, variants) go in `CMakePresets.json` as cache variables. Keep `.vscode/tasks.json` and `.vscode/launch.json` in sync when adding presets.
- Board targets are fully qualified: `nucleo_g0b1re/stm32g0b1xx` and `crsf_fsdb01/stm32g0b1xx`.
- **Always build both boards** after changes to code, Kconfig or devicetree. Report errors and warnings. The goal is zero warnings.
- Look up Zephyr APIs, Kconfig symbols and bindings in the container's `/opt/zephyrproject` tree. Don't rely on memory; Zephyr changes between releases.

## Code

- Application code is **C++23** (`CONFIG_STD_CPP23`) with full libstdc++ (`CONFIG_REQUIRES_FULL_LIBCPP`). Exceptions and RTTI are off; don't use them.
- Sources go in `src/` as `.cpp` and must be listed in `CMakeLists.txt`.
- Zephyr's own code stays C. Use Zephyr C APIs directly from C++. Don't add wrappers unless they earn their keep.
- Tabs for indentation, matching Zephyr style.
- Avoid dynamic allocation in steady-state code paths.

## Boards and devicetree

- Two targets: `nucleo_g0b1re` (stock Zephyr board plus `boards/nucleo_g0b1re.overlay`) and `crsf_fsdb01` (custom board in `boards/custom/crsf_fsdb01/`).
- **No board `#ifdef`s in application code.** Get hardware through devicetree aliases (`crsf-uart`, `led0`, `led1`, …). Every alias the app uses must be defined for **both** boards.
- Hardware description belongs in the board DTS (`crsf_fsdb01.dts`). App-level software choices (console routing, CDC ACM) belong in `boards/*.overlay` or `boards/usb_console.dtsi`. Kconfig common to both boards goes in `prj.conf`.
- Custom board pin assignments are **provisional**. Keep the README pin table in sync when they change.
- No crystal on either board: SYSCLK is 64 MHz from the HSI, and USB uses HSI48 with `crs-usb-sof`.
- On the Nucleo, PA11/PA12 are used for USB. I2C2 and FDCAN1 are disabled in the overlay because the stock board config assigns them to the same pins. Watch for similar pin conflicts when enabling peripherals.

## Console / CLI

- The console and shell are USB CDC ACM (`cdc_acm_uart0`) on both boards. There is **no UART console**, and the production board has none.
- Keep `CONFIG_USBD_CDC_ACM_LOG_LEVEL_OFF=y`, because CDC ACM can't log to itself.
- The VID/PID is currently Zephyr's test ID (`0x2fe3:0x0004`) and must be replaced before production.

## Housekeeping

- Keep `README.md` up to date when hardware, pins, build steps or the environment change.
- Don't commit unless asked.
