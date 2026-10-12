#!/usr/bin/env bash
#
# Flash a firmware image over USB DFU, using the STM32 system bootloader.
#
# Usage: scripts/dfu_flash.sh <firmware.bin>
#   e.g. scripts/dfu_flash.sh build_custom/zephyr/zephyr.bin
#
# Put the board in DFU mode first with the `dfu` shell command. Set
# DFU_SERIAL=<serial> to pick one board when several are in DFU mode
# (`dfu-util -l` shows the serial). Needs a udev rule for 0483:df11, or sudo.
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

DFU_ID="0483:df11"
FLASH_ADDR="0x08000000"

if [[ $# -ne 1 ]]; then
	echo "Usage: $(basename "$0") <firmware.bin>" >&2
	exit 2
fi

image="$1"

if [[ ! -f "$image" ]]; then
	echo "Error: '$image' not found" >&2
	exit 1
fi

if [[ "$image" != *.bin ]]; then
	echo "Error: '$image' is not a .bin file (dfu-util needs the raw image, not the .elf/.hex)" >&2
	exit 1
fi

serial_args=()
if [[ -n "${DFU_SERIAL:-}" ]]; then
	serial_args=(-S "$DFU_SERIAL")
fi

if ! dfu-util -l -d "$DFU_ID" "${serial_args[@]}" 2>/dev/null | grep -q "Found DFU: \[$DFU_ID\]"; then
	echo "Error: no STM32 bootloader ($DFU_ID) found." >&2
	echo "Run 'dfu' in the board's shell first, and check USB permissions (udev rule or sudo)." >&2
	exit 1
fi

echo "Flashing $image ($(stat -c %s "$image") bytes) to $FLASH_ADDR"
dfu-util -d "$DFU_ID" "${serial_args[@]}" -a 0 -s "$FLASH_ADDR:leave" -D "$image"

echo
echo "Done. If the board stays in DFU mode, power-cycle it fully: the bootloader"
echo "leaves FLASH_ACR.EMPTY set until a power-on reset (see README)."
