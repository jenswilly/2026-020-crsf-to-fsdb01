# CRSF to FS-DB01

Firmware that takes RC input from an **ExpressLRS (ELRS) receiver** in **CRSF** format and converts it to the proprietary single-wire protocol of the **FlySky FS-DB01** LED control module. This lets an ELRS radio control the module's lighting functions: turn signals, illumination, reverse/brake, and the no-signal indication.

The FS-DB01 protocol is documented from the reverse-engineered reference implementation in [osos11-Git/F401_FMS_FCX10_LED_REVERSE](https://github.com/osos11-Git/F401_FMS_FCX10_LED_REVERSE), an STM32F401 CubeIDE project. See `Core/Src/main.c` there.

Built on [Zephyr RTOS](https://zephyrproject.org/) 4.4 for the STM32G0B1. The application code is C++23.

## Hardware targets

| Board target    | Hardware                          | MCU                         |
|-----------------|-----------------------------------|-----------------------------|
| `nucleo_g0b1re` | NUCLEO-G0B1RE development board   | STM32G0B1RET6 (LQFP64)      |
| `crsf_fsdb01`   | Custom production board           | STM32G0B1CET6 (LQFP48)      |

Both MCUs have 512 KiB flash and 144 KiB RAM. Both run at 64 MHz from the internal 16 MHz HSI. Neither has a crystal; USB runs from the HSI48 oscillator, trimmed by the CRS against USB SOF.

### Pin assignments

Custom board pins are **provisional** until the schematic is final.

| Function            | `crsf_fsdb01`        | `nucleo_g0b1re`                      |
|---------------------|----------------------|--------------------------------------|
| CRSF UART (USART1)  | PB6 TX, PB7 RX       | PC4 TX (Arduino D1), PC5 RX (Arduino D0) |
| USB D− / D+         | PA11 / PA12          | PA11 (CN10-14) / PA12 (CN10-12)      |
| `led0`              | PB0 (green)          | PA5 (LD4, green)                     |
| `led1`              | PB1 (red)            | –                                    |
| FS-DB01 signal out  | TBD                  | TBD                                  |
| SWD                 | PA13 SWDIO, PA14 SWCLK | on-board ST-LINK                   |

The CRSF link runs at 420000 baud, 8N1, full duplex.

### Console / CLI

The Zephyr console and shell use **USB CDC ACM** on the MCU's own USB pins on both boards. There is no UART console, except for the optional `nucleo-vcp` build below.

On the Nucleo this needs an **external USB connector**, wired to PA12 (D+), PA11 (D−) and GND on the morpho header. Don't connect its VBUS while the board is also powered from the ST-LINK USB. The ST-LINK virtual COM port (USART2) is not used by default.

For development without the extra connector, the `nucleo-vcp` preset adds `boards/nucleo_vcp_console.overlay`, which moves the console and shell to USART2, and `boards/nucleo_vcp.conf` for extra development Kconfig (e.g. `CONFIG_DEBUG_OPTIMIZATIONS`). Output then appears on the ST-LINK virtual COM port (usually `/dev/ttyACM0`) at 115200 baud. CDC ACM is still built in but isn't the console.

The device currently uses Zephyr's test USB VID/PID (`0x2fe3:0x0004`). Production needs its own.

## Protocols

### Input: CRSF from an ELRS receiver

- UART, **420000 baud, 8N1**, non-inverted, full duplex: receiver TX → MCU RX. MCU TX → receiver RX is only needed for telemetry.
- Frame layout: `[sync 0xC8] [len] [type] [payload …] [CRC8]`. `len` counts type + payload + CRC. The CRC8 uses the DVB-S2 polynomial `0xD5` and covers type + payload.
- `0x16` *RC channels packed*: 16 channels × 11 bits, little-endian bit-packed. Nominal values run 172–1811 (≈ 988–2012 µs), with 992 at center.
- `0x14` *Link statistics*: RSSI, LQ, SNR, etc. Useful for failsafe / no-signal detection.

### Output: FS-DB01 LED module

All of the following comes from the reverse-engineered reference implementation; it isn't an official specification.

- A single GPIO line, idle **low**, driven from a 1 ms time base.
- A frame is **9 bits**, sent bit 0 first, followed by **3 ms low**. Each bit takes 3 ms, so a frame takes 30 ms and repeats continuously.
  - `1` = 2 ms high, then 1 ms low
  - `0` = 1 ms high, then 2 ms low

| Bit | Function                                         |
|-----|--------------------------------------------------|
| 0   | Right turn signal                                |
| 1   | Left turn signal                                 |
| 2   | Illumination, step 1                             |
| 3   | Illumination, step 2                             |
| 4   | Unknown; probably unused                         |
| 5   | Reverse / brake                                  |
| 6   | No-signal fast blink (low priority)              |
| 7   | No-signal slow blink (high priority)             |
| 8   | Always 0                                         |

The reference firmware's power-on state is bits 6 and 7 set: the no-signal indication.

The mapping from CRSF channels to FS-DB01 bits, and the failsafe behavior, are **not decided yet**.

## Development environment

Everything is built in a VS Code devcontainer (`.devcontainer/`), based on Ubuntu 24.04:

- Zephyr **v4.4.2** west workspace in `/opt/zephyrproject` (`ZEPHYR_BASE` is set). Only the `cmsis`, `cmsis_6` and `hal_stm32` modules are fetched.
- Zephyr SDK **1.0.1** in `/opt/zephyr-sdk-1.0.1` (ARM GCC 14.3 + host tools)
- Python venv with west in `/opt/zephyr-venv` (on `PATH`)
- CMake **4.4.3** and Ninja **1.13.2** from the official release binaries (Ubuntu's packages are too old)
- OpenOCD, stlink-tools, gdb-multiarch

Open the folder in VS Code and choose **Dev Containers: Reopen in Container**. The first image build downloads Zephyr and the SDK and takes a while.

The container runs privileged with the host's `/dev` bind-mounted, so ST-LINK probes and `/dev/ttyACM*` ports work, including ones that re-enumerate after a board reset. The **host** needs udev rules for the ST-LINK. Your user must be in the `dialout` and `plugdev` groups.

To change tool versions, edit `ZEPHYR_VERSION`, `CMAKE_VERSION` or `NINJA_VERSION` in `devcontainer.json`, then rebuild the container.

## Building, flashing and debugging

Builds are driven by **CMake presets** (`CMakePresets.json`), one per board:

| Preset   | Board target                | Build directory |
|----------|-----------------------------|-----------------|
| `nucleo` | `nucleo_g0b1re/stm32g0b1xx` | `build_nucleo/` |
| `nucleo-vcp` | `nucleo_g0b1re/stm32g0b1xx`, console on ST-LINK VCP | `build_nucleo-vcp/` |
| `custom` | `crsf_fsdb01/stm32g0b1xx`   | `build_custom/` |

### In VS Code

- **Build:** choose the preset in the CMake Tools status bar, then click Build (F7). IntelliSense follows the active preset.
- **Flash / menuconfig:** use the tasks in `.vscode/tasks.json` (*Tasks: Run Task* → `Flash: nucleo`, `Flash: nucleo-vcp`, `Flash: custom`, …).
- **Debug:** use the Cortex-Debug launch configs in `.vscode/launch.json` (`Debug: nucleo`, `Debug: custom`, `Attach: custom`). They build first, then flash and debug via OpenOCD + ST-LINK.

### From the command line

Run these inside the container, from the project root:

```sh
cmake --preset custom                             # configure
cmake --build --preset custom                     # build
cmake --build --preset custom --target flash      # flash (OpenOCD + ST-LINK)
cmake --build --preset custom --target menuconfig # Kconfig
cmake --build --preset custom --target ram_report # memory usage
```

Replace `custom` with `nucleo` for the development board. For a clean rebuild, delete the build directory, or run `cmake --preset <name> --fresh`.

The `flash`, `debug` and `debugserver` targets call Zephyr's west runners internally. `west` still works on the same build directories, e.g. `west flash -d build_custom`, or `west flash -d build_custom -r <runner>` to pick another runner. The custom board also defines STM32CubeProgrammer, pyOCD and J-Link runners. Flashing and Cortex-Debug both use the OpenOCD and GDB from the Zephyr SDK.

## Project layout

```
CMakeLists.txt                  App build; adds this repo as a BOARD_ROOT
CMakePresets.json               Configure/build presets per board
prj.conf                        Common Kconfig: C++23, full libstdc++, logging, shell, USB CDC ACM
src/                            Application sources (C++)
boards/
  usb_console.dtsi              CDC ACM console/shell node, shared by both boards
  nucleo_g0b1re.overlay         Nucleo: crsf-uart alias, USB console, frees PA11/PA12
  nucleo_vcp_console.overlay    Nucleo dev option (nucleo-vcp preset): console on ST-LINK VCP
  nucleo_vcp.conf               Extra Kconfig for the nucleo-vcp preset (debug settings)
  crsf_fsdb01.overlay           Custom board: USB console
  custom/crsf_fsdb01/           Custom board definition (Zephyr hardware model v2)
.vscode/                        Tasks (build/flash/menuconfig) and Cortex-Debug launch configs
.devcontainer/                  Dockerfile + devcontainer.json
```

The application uses devicetree **aliases** (`crsf-uart`, `led0`, …) instead of board-specific code. Each board, or its overlay, maps the aliases to its hardware.
