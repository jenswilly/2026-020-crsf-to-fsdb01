# CRSF to FSDB01

Firmware that bridges a CRSF (Crossfire) RC link to FSDB01.

<!-- TODO: describe FSDB01 and what the bridge does -->

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
| SWD                 | PA13 SWDIO, PA14 SWCLK | on-board ST-LINK                   |

The CRSF link runs at 420000 baud, 8N1, full duplex.

### Console / CLI

The Zephyr console and shell use **USB CDC ACM** on the MCU's own USB pins on both boards. There is no UART console.

On the Nucleo this needs an **external USB connector**, wired to PA12 (D+), PA11 (D−) and GND on the morpho header. Don't connect its VBUS while the board is also powered from the ST-LINK USB. The ST-LINK virtual COM port (USART2) is not used.

The device currently uses Zephyr's test USB VID/PID (`0x2fe3:0x0004`). Production needs its own.

## Development environment

Everything is built in a VS Code devcontainer (`.devcontainer/`), based on Ubuntu 24.04:

- Zephyr **v4.4.2** west workspace in `/opt/zephyrproject` (`ZEPHYR_BASE` is set). Only the `cmsis`, `cmsis_6` and `hal_stm32` modules are fetched.
- Zephyr SDK **1.0.1** in `/opt/zephyr-sdk-1.0.1` (ARM GCC 14.3 + host tools)
- Python venv with west in `/opt/zephyr-venv` (on `PATH`)
- OpenOCD, stlink-tools, gdb-multiarch

Open the folder in VS Code and choose **Dev Containers: Reopen in Container**. The first image build downloads Zephyr and the SDK and takes a while.

The container runs privileged with the host's `/dev` bind-mounted, so ST-LINK probes and `/dev/ttyACM*` ports work, including ones that re-enumerate after a board reset. The **host** needs udev rules for the ST-LINK. Your user must be in the `dialout` and `plugdev` groups.

To change Zephyr versions, edit `ZEPHYR_VERSION` in `devcontainer.json` and rebuild the container.

## Building and flashing

Run these inside the container, from the project root:

```sh
# Development board
west build -b nucleo_g0b1re/stm32g0b1xx -d build_nucleo .
west flash -d build_nucleo

# Custom board
west build -b crsf_fsdb01/stm32g0b1xx -d build_custom .
west flash -d build_custom
```

Use the full board targets (`<board>/stm32g0b1xx`), as the nRF Connect extension does. West refuses to reuse a build directory if the board name is spelled differently. Add `-p` for a pristine rebuild. Flashing uses OpenOCD with an ST-LINK by default. The custom board also defines runners for STM32CubeProgrammer, pyOCD and J-Link (`west flash -r <runner>`).

Other useful targets:

```sh
west build -d build_custom -t menuconfig   # Kconfig
west build -d build_custom -t ram_report   # memory usage
west debug -d build_nucleo                 # GDB via OpenOCD
```

## Project layout

```
CMakeLists.txt                  App build; adds this repo as a BOARD_ROOT
prj.conf                        Common Kconfig: C++23, full libstdc++, logging, shell, USB CDC ACM
src/                            Application sources (C++)
boards/
  usb_console.dtsi              CDC ACM console/shell node, shared by both boards
  nucleo_g0b1re.overlay         Nucleo: crsf-uart alias, USB console, frees PA11/PA12
  crsf_fsdb01.overlay           Custom board: USB console
  custom/crsf_fsdb01/           Custom board definition (Zephyr hardware model v2)
.devcontainer/                  Dockerfile + devcontainer.json
```

The application uses devicetree **aliases** (`crsf-uart`, `led0`, …) instead of board-specific code. Each board, or its overlay, maps the aliases to its hardware.
