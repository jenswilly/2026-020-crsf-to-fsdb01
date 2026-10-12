# Schematic and Board Design Instructions

## Schematic

The schematic should be organized as _one page_ in KiCad. There are so few components that there
is no need for hierarchical organization.

### General Guidelines

* Supply symbols should be near the top of the page (or at least at the top of components)
* Ground symbols should be near the bottom of the page (or at least at the bottom of components/nets)
* Generic passives should be 0603 size whenever possible
* Use European (IEC 60617) symbols


### Components

| Component        | Part                      | Notes                                                                 |
|------------------|---------------------------|-----------------------------------------------------------------------|
| MCU              | STM32G0B1CET6             | No crystal                                                            |
| LED1             | KP-1608QBC-D              | Active low, direct sink from MCU GPIO pin (20 mA max), 0603, Vf=3.2 V |
| LED2             | ASMT-RF45-AN002           | Active low, direct sink from MCU GPIO pin (20 mA max), 0603, Vf=2.0 V |
| Input connector  | footprint for hand-solder | 3 x 0.1" pin headers near edge of board to allow RA pins              |
| Output connector | footprint for hand-solder | 3 x 0.1" pin headers near edge of board to allow RA pins              |
| LDO              | NCP551SN33T1G             | 3.3 V LDO                                                             |
| Passives         | (generic)                 | Bypass capacitors and resistors                                       |


### High-level Design

1. Input connector has CRSF, Vin and GND. Vin is ~6 V
2. Output connector has FSDB01 signal, Vin and GND
3. Vin (6 V) goes to LDO to generate 3.3 V for MCU
4. CRSF signal is connected to MCU serial in pin
5. FSDB01 signal comes from MCU fsdb01 signal out pin
6. USB-C connector is connected to USB DP/DM pins on MCU. This is only USB 2.0 so no need for high-speed traces
7. VBUS from USB-C should also be connected to LDO to allow device being powered from USB only.  
   VBUS is _not_ connected to output connector's Vin pin. Make sure LDO is protected is both VBUS and Vin are 
   connected.
8. Add bypass capacitors and other passives (e.g. for LDO) as recommended
9. LEDs should be limited to 15 mA current. Forward voltages are 3.2 V (LED1) and 2.0 V (LED2)


-----

## PCB Layout

Information to come...
