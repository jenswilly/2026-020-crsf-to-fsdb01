# Schematic and Board Design Instructions

## Schematic

The schematic should be organized as _one page_ in KiCad. There are so few components that there
is no need for hierarchical organization.

### General Guidelines

- Supply symbols should be near the top of the page (or at least at the top of components)
- Ground symbols should be near the bottom of the page (or at least at the bottom of components/nets)
- Generic passives should be 0603 size whenever possible
- Use European (IEC 60617) symbols

### Components

| Component        | Part                                  | Notes                                                                     |
| ---------------- | ------------------------------------- | ------------------------------------------------------------------------- |
| MCU              | STM32G0B1CET6                         | LQFP48, no crystal. Symbol `MCU_ST_STM32G0:STM32G0B1CETx`                 |
| LED1             | KP-1608QBC-D (blue)                   | Active low, direct sink from MCU GPIO, 0603. Vf = 3.3 V typ               |
| LED2             | ASMT-RF45-AN002 (red-orange)          | Active low, direct sink from MCU GPIO, 0603. Vf = 2.0 V                   |
| Input connector  | footprint for hand-solder             | 3 x 0.1" pin header near edge of board to allow RA pins                   |
| Output connector | footprint for hand-solder             | 3 x 0.1" pin header near edge of board to allow RA pins                   |
| USB connector    | USB-C receptacle, USB 2.0             | Symbol `Connector:USB_C_Receptacle_USB2.0_16P`                            |
| USB ESD          | USBLC6-2SC6                           | ESD protection on D+/D−. Symbol `Power_Protection:USBLC6-2SC6`            |
| SWD connector    | Molex PicoBlade, 5 pin, 53261-0571    | Side entry, 1.25 mm. Symbol `Connector_Generic_MountingPin:Conn_01x05_MountingPin`, footprint `Connector_Molex:Molex_PicoBlade_53261-0571_1x05-1MP_P1.25mm_Horizontal`. MP pads are mechanical only: no-connect |
| LDO              | NCP551SN33T1G                         | 3.3 V, 150 mA, TSOP-5/SOT-23-5. Needs a project-local symbol (see below)  |
| D1, D2           | Schottky diode, ≥ 0.5 A (e.g. B5819W) | Power ORing of Vin and VBUS into the LDO, SOD-123 or smaller              |
| Voltage sense    | 100 kΩ + 33 kΩ dividers, 100 nF       | One divider each for Vin and VBUS, into MCU ADC pins (see design item 10) |
| Passives         | (generic)                             | Bypass capacitors and resistors                                           |

### MCU Pin Assignments

These match the firmware (`boards/custom/crsf_fsdb01/crsf_fsdb01.dts` and the README pin table).
If a pin changes here, the firmware and README must be updated too.

| Pin  | Function                      | Notes                                                          |
| ---- | ----------------------------- | -------------------------------------------------------------- |
| PB7  | CRSF in (USART1 RX)           | From input connector signal pin                                |
| PB6  | USART1 TX                     | Not connected: the 3-pin input connector has no telemetry wire |
| PA8  | FS-DB01 signal out            | To output connector signal pin                                 |
| PA11 | USB D−                        | Via USBLC6-2SC6                                                |
| PA12 | USB D+                        | Via USBLC6-2SC6                                                |
| PA13 | SWDIO                         | To SWD connector                                               |
| PA14 | SWCLK                         | To SWD connector. Also BOOT0; no BOOT0 circuitry needed        |
| NRST | Reset                         | To SWD connector, 100 nF to GND                                |
| PB0  | LED1 (blue), active low       | Via series resistor                                            |
| PB1  | LED2 (red-orange), active low | Via series resistor                                            |
| PA0  | VIN_SENSE (ADC1_IN0)          | Vin voltage divider tap (`vsense0`)                            |
| PA1  | VBUS_SENSE (ADC1_IN1)         | VBUS voltage divider tap (`vsense1`)                           |

### Connector Pinouts

Input and output connectors use the servo standard order, with V+ in the middle so that a
reversed plug doesn't swap the power pins:

| Pin | Input connector | Output connector   |
| --- | --------------- | ------------------ |
| 1   | CRSF signal     | FS-DB01 signal     |
| 2   | Vin             | Vin (pass-through) |
| 3   | GND             | GND                |

SWD connector (Molex PicoBlade 53261-0571):

| Pin | Signal |
| --- | ------ |
| 1   | 3V3    |
| 2   | SWDIO  |
| 3   | SWCLK  |
| 4   | NRST   |
| 5   | GND    |

### High-level Design

1. Input connector has CRSF, Vin and GND. Vin is nominally ~6 V and must not exceed 12 V
   (NCP551 absolute maximum).
2. Output connector has FS-DB01 signal, Vin and GND. Its Vin pin is connected **directly** to the
   input connector's Vin, before the ORing diode, so there's no voltage drop. When the board is
   powered from USB only, the output connector's Vin is unpowered. This is intentional.
3. Vin and USB VBUS are ORed into the LDO input through one Schottky diode each (D1 from Vin, D2
   from VBUS, cathodes joined at the LDO input). This keeps VBUS and Vin from back-feeding each
   other when both are connected, and protects against a reversed Vin.
4. LDO (NCP551) generates 3.3 V for the MCU:
    - 1 µF ceramic on the input and 1 µF ceramic on the output (per datasheet)
    - EN tied to VIN (always on), pin 4 (NC) left unconnected
    - Bulk capacitor (e.g. 10 µF) on Vin at the input connector
5. CRSF signal is connected to the MCU USART1 RX pin (PB7).
6. FS-DB01 signal comes from the MCU's FS-DB01 output pin (PA8).
7. USB-C connector is connected to the USB D+/D− pins on the MCU (PA12/PA11) through the
   USBLC6-2SC6. This is only USB 2.0 (full speed), so there's no need for high-speed traces.
    - Both D+ and both D− pins of the receptacle are joined (either plug orientation)
    - CC1 and CC2 each have their own 5.1 kΩ pull-down to GND (UFP/sink)
    - USBLC6-2SC6 VBUS pin to VBUS, GND to GND
8. MCU support circuitry:
    - 100 nF bypass capacitor at each supply pin: VDD (pin 6), VREF+ (pin 5), VBAT (pin 4),
      plus 4.7 µF bulk on VDD
    - VDD, VREF+ and VBAT all connect to 3V3 (no battery backup)
    - NRST: 100 nF to GND (internal pull-up is used)
    - No crystal, no BOOT0 circuitry
9. LEDs are sunk directly by the MCU GPIOs (active low), anode to 3V3 via a series resistor:
    - LED1 (blue): Vf is 3.3 V typical, so it can't reach 15 mA from 3.3 V and will be dim.
      This is accepted. Use **10 Ω**, which still keeps the current ≤ ~15 mA at the lowest Vf.
    - LED2 (red-orange): (3.3 V − 2.0 V) / 15 mA ≈ 87 Ω → **91 Ω**.
10. Vin and VBUS are measured by the MCU ADC through a voltage divider each, so the firmware can
    read the raw input voltages:
    - Both dividers tap the **raw** rails, before the ORing diodes: Vin at the input connector,
      VBUS at the USB-C connector
    - Divider: 100 kΩ (top) / 33 kΩ (bottom), ratio 0.248. Full scale (3.3 V at the ADC pin)
      is ≈ 13.3 V, so the 12 V Vin maximum reads ≈ 2.98 V. Using the same ratio for both rails
      keeps one scale factor in firmware and leaves VBUS headroom for an out-of-spec source
    - 100 nF from each ADC pin to GND, to filter and to give the ADC's sample capacitor a low
      source impedance (the divider is ~25 kΩ)
    - Vin divider → PA0 (VIN_SENSE), VBUS divider → PA1 (VBUS_SENSE)
    - Quiescent draw is ≈ 45 µA from Vin at 6 V. A reversed Vin pulls the pin slightly negative,
      but the 100 kΩ limits the injected current to well under 0.1 mA
    - Use 1 % resistors so the measurement doesn't need calibration

### NCP551 Symbol

The NCP551 is not in the KiCad libraries. Create a project-local symbol (through Konnect's
library tools, not by hand-editing files) with footprint `Package_TO_SOT_SMD:SOT-23-5`:

| Pin | Name |
| --- | ---- |
| 1   | VIN  |
| 2   | GND  |
| 3   | EN   |
| 4   | NC   |
| 5   | VOUT |

---

## PCB Layout

Before continuing with layout instructions, user must verify capacitor footprints and voltages.
