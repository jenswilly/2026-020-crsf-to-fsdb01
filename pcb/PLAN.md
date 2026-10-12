# Review of pcb/INSTRUCTIONS.md before schematic capture

## Context
The user asked whether `pcb/INSTRUCTIONS.md` is good enough to start the KiCad schematic
(`pcb/2026-020-crft-to-fsdb01/`, KiCad 10, currently an empty A4 sheet). Checked via Konnect
(project info, symbol library search) and the datasheets in `pcb/Datasheets/`.

Verdict: the overall structure is fine, but there's one blocking problem (blue LED) and a few gaps
that a schematic builder would otherwise have to guess. Plan: close these in INSTRUCTIONS.md first.

## Findings

### Blocking
1. **LED1 (KP-1608QBC-D, blue) can't be driven at 15 mA from 3.3 V.** Datasheet: Vf 3.3 V typ /
   4.0 V max @ 20 mA (not 3.2 V). With a 3.3 V rail and a sinking GPIO (VOL ~0.4 V+), headroom is
   negative → LED will be dim/off and current uncontrolled. Needs a decision (see questions).

### Gaps (recommended defaults in brackets)
2. **Pin assignments not stated.** Firmware already uses PB6/PB7 USART1 (CRSF), PA11/PA12 USB,
   PA13/PA14 SWD, PB0/PB1 LEDs, PA8 FS-DB01 (placeholder). [Use these; state them in the doc.]
   CRSF input is RX-only with a 3-pin connector → MCU TX (PB6) unused/no telemetry. [Note it.]
3. **Power ORing method** for Vin/VBUS. [Two Schottky diodes (or dual common-cathode, e.g.
   BAT54C/BAT60) into LDO input; also gives Vin reverse-polarity protection.] State max Vin
   [NCP551 abs max 12 V; fine for 2S/HV BEC].
4. **Connector pin order** not given. [Servo standard: Signal, V+ (middle), GND.]
5. **Missing support circuitry:** SWD header (README relies on SWD for recovery), NRST 100 nF,
   USB-C CC1/CC2 5.1 kΩ pull-downs, optional USB ESD (e.g. USBLC6-2SC6), series resistors
   (~100–330 Ω) on CRSF in and FS-DB01 out, bulk cap on Vin, decoupling per VDD pin incl.
   VDDA/VREF+/VBAT. BOOT0: none needed (DFU via option-byte defaults, per README).
6. **NCP551SN33T1G symbol not in KiCad libs.** TSOP-5/SOT-23-5, pinout 1 VIN, 2 GND, 3 EN,
   4 NC, 5 VOUT. [Create a project-local symbol via Konnect `create_symbol`; footprint
   `Package_TO_SOT_SMD:SOT-23-5`; 1 µF in/out ceramic; EN tied to VIN.] MCU symbol
   `MCU_ST_STM32G0:STM32G0B1CETx` and `Connector:USB_C_Receptacle_USB2.0_16P` exist.
7. **Firmware mismatch (follow-up, not schematic):** DTS has LEDs GPIO_ACTIVE_HIGH, labelled
   green/red; instructions say active-low, blue/red-orange. Update DTS + README for the custom board only after schematic.
8. Typo: "LDO is protected is both" → "if both". Fix.

## User decisions
- **Blue LED:** keep it, accept low current. Direct sink from 3.3 V with a 10 Ω series resistor,
  which keeps the current ≤ ~15 mA even at the lowest Vf. This is acceptable.
  LED2 (red-orange, Vf 2.0 V): (3.3 − 2.0) / 15 mA ≈ 87 Ω → 91 Ω.
- **Power:** a Schottky diode on Vin and another on VBUS, joined into the LDO input. The output
  connector's V+ comes **directly from input-connector Vin**, ahead of the diode, with no drop.
  It is unpowered when running from USB only, which is intentional.
- **Connectors:** servo standard pin order: 1 Signal, 2 V+ (middle), 3 GND, on both.
- **Extras:** add USB ESD protection (USBLC6-2SC6) and a JST XH 5-pin SWD connector,
  S5B-XH-A (LF)(SN) (side entry). Proposed pinout: 1 3V3, 2 SWDIO, 3 SWCLK, 4 NRST, 5 GND.
  No series resistors on signals, no power LED.
- **Voltage sensing:** measure raw Vin and VBUS (before the ORing diodes) on ADC pins through
  100 kΩ / 33 kΩ dividers (ratio 0.248, full scale ≈ 13.3 V), 100 nF at each ADC pin, 1 %
  resistors. Vin → PA0 (ADC1_IN0), VBUS → PA1 (ADC1_IN1).

## Plan
1. Edit `pcb/INSTRUCTIONS.md` only (no KiCad files):
   - Components table: add Schottky ×2, USBLC6-2SC6,
     S5B-XH-A SWD connector, USB-C receptacle (USB 2.0 16P), and the LED resistor values.
   - New "MCU pin assignments" table (PB7 CRSF RX, PB6 unused, PA11/PA12 USB, PA13/PA14 SWD,
     NRST, PB0 LED1, PB1 LED2, PA8 FS-DB01 out), matching README/DTS.
   - High-level design: rewrite item 7 with the diode-OR, item 2 with the Vin passthrough,
     connector pin orders, max Vin ≤ 12 V (NCP551 abs max), USB-C CC 5.1 kΩ pull-downs,
     NRST 100 nF, decoupling for each VDD pin plus VDDA/VREF+/VBAT, Vin bulk cap, LDO 1 µF in/out,
     EN tied to VIN.
   - Note that NCP551 needs a project-local symbol (pinout above, footprint SOT-23-5).
   - Vin/VBUS sense dividers: components table, pin table (PA0/PA1) and design item 10.
   - Fix typo "is both" → "if both".
2. Follow-up for after the schematic (record in NOTES.md, don't do it now): DTS LEDs to
   GPIO_ACTIVE_LOW, relabel blue/red-orange, finalize the FS-DB01 pin in README and custom board DTS.
   Verify ADC pins are added correctly to both boards and that ratio is set correctly.
3. Then build the schematic via Konnect / kicad-schematic-build-agent, when the user asks.
3. Then (separate step, user-triggered) build the schematic via Konnect / kicad-schematic-build-agent.

## Verification
Re-read INSTRUCTIONS.md for every item above; no `.kicad_*` files touched.
