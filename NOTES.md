# Handoff notes

Working notes for picking up development, on any machine or in a new Claude Code session. `README.md` covers the hardware, pins, protocol and build; this file covers where things stand and why. Update it when an item is settled.

Last updated: 2026-10-05.

## Status

- **Builds:** `nucleo`, `nucleo-vcp` and `custom` presets all build with zero warnings.
- **FS-DB01 driver** (`drivers/fsdb01/`) is functional but **not yet verified on hardware**. Check the output with a logic analyser on the Nucleo's PA10 (Arduino D2). With the no-signal frame (bits 6 and 7), each 30 ms frame should be six short pulses (1 ms high / 2 ms low), two long ones (2 ms / 1 ms), one short, then 3 ms low.
- **Shell:** `leds` command group (`src/shell_leds.cpp`) drives the driver by hand; see README.
- **`main()`** checks devices, **disables FS-DB01 output at boot**, and blinks `led0`. Run `leds enable` to start output.
- **CRSF:** UART is set up (alias `crsf-uart`, 420000 baud), but nothing reads or parses it yet.

## Decisions and why

- **Driver lives in the app, not a Zephyr module** (`drivers/`, binding in `dts/bindings/`, top-level `Kconfig` sources `drivers/Kconfig`). It's only used by this app.
  - Sources are added with `target_sources(app …)`. `zephyr_library()` does **not** work from an app: Zephyr warns and leaves the library out of the link, which shows up as `undefined reference to __device_dts_ord_N`.
- **Driver API:** `fsdb01_set_frame()` (atomic, callable from anywhere, latched at the next frame start), `fsdb01_enable()`/`fsdb01_disable()` (mutex, not ISR-safe), `fsdb01_is_enabled()`.
  - No queue or dedicated thread for the driver: only the latest frame matters, so an atomic value is enough.
- **Bit timing:** one-shot `k_timer` re-armed at each edge (ISR context), scheduled with **absolute** deadlines (`K_TIMEOUT_ABS_TICKS`). Relative re-arms add 1 tick per edge in Zephyr 4.4 (`kernel/timeout.c`, `timeout.ticks + 1`), which would stretch bits to ~3.2 ms. The Kconfig selects `TIMEOUT_64BIT` for this. Tick rate is 10 kHz.
  - Fallback if timing proves too loose: a hardware timer in PWM mode (3 ms period, 1 or 2 ms pulse).
- **`invert` DT property** inverts every level (frames, gap, disabled state, level at boot) for an inverting level shifter. Don't combine it with `GPIO_ACTIVE_LOW` in `out-gpios`; the two cancel out.
- **`nucleo-vcp` preset:** console/shell on the ST-LINK virtual COM port (USART2, 115200) via `boards/nucleo_vcp_console.overlay`, plus debug Kconfig from `boards/nucleo_vcp.conf`. Avoids wiring a USB connector to PA11/PA12. Production stays CDC-ACM-only.

## Open items

- **FS-DB01 output pin:** placeholders, PA10 on the Nucleo, PA8 on the custom board.
- **CRSF channel → FS-DB01 bit mapping:** not decided.
- **Failsafe behaviour:** not decided. This includes link-loss detection (timeout, and/or the link-statistics frame), which frame to send, and whether output should stay disabled until the link is up (currently `main()` disables it at boot and nothing enables it).
- **Application structure:** planned as one control thread that receives CRSF frames from the UART (message queue or ring buffer from the ISR), maps them, handles failsafe and calls `fsdb01_set_frame()`. Not started.
- **Hardware verification** of the bit engine (see Status).
- **USB VID/PID** is still Zephyr's test ID.

## Environment notes

- The `devcontainer` CLI on the original host is at `~/.devcontainers/bin/devcontainer`, which isn't on the PATH of non-interactive shells.
- Without the CLI, `docker exec` into the running container also works (workspace at `/workspaces/2026-020-crsf-to-fsdb01`).
