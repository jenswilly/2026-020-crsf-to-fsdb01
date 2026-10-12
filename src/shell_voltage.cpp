/*
 * Shell commands for the voltage sense inputs (ADC through voltage dividers)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>

#include <array>
#include <cstdint>

namespace {

struct VoltageInput {
    const char* name;
    const device* dev;
    /* Divider ratio as output/full ohms, to show the voltage at the pin */
    uint32_t output_ohms;
    uint32_t full_ohms;
};

#define VOLTAGE_INPUT(alias)                                                           \
    VoltageInput {                                                                     \
        #alias, DEVICE_DT_GET(DT_ALIAS(alias)), DT_PROP(DT_ALIAS(alias), output_ohms), \
            DT_PROP(DT_ALIAS(alias), full_ohms)                                        \
    }

const std::array<VoltageInput, 2> inputs{{
    VOLTAGE_INPUT(vsense0),
    VOLTAGE_INPUT(vsense1),
}};

int cmd_voltage(const shell* sh, size_t, char**) {
    int ret = 0;

    for (const VoltageInput& in : inputs) {
        if (!device_is_ready(in.dev)) {
            shell_error(sh, "%s: device %s not ready", in.name, in.dev->name);
            ret = -ENODEV;
            continue;
        }

        sensor_value val;
        int err = sensor_sample_fetch(in.dev);
        if (err == 0) {
            err = sensor_channel_get(in.dev, SENSOR_CHAN_VOLTAGE, &val);
        }
        if (err < 0) {
            shell_error(sh, "%s: read failed (%d)", in.name, err);
            ret = err;
            continue;
        }

        /* Single-ended readings, so never negative; printf has no float support */
        auto input_mv = static_cast<uint32_t>(sensor_value_to_milli(&val));
        auto pin_mv = static_cast<uint32_t>(uint64_t{input_mv} * in.output_ohms / in.full_ohms);
        shell_print(sh, "%-8s %2u.%03u V   (pin %u.%03u V)", in.name, input_mv / 1000,
                    input_mv % 1000, pin_mv / 1000, pin_mv % 1000);
    }
    return ret;
}

}  // namespace

/* Shell macros expand to `cond ? arg : NULL`, so pass NULL rather than nullptr */
SHELL_CMD_REGISTER(voltage, NULL, "Show the voltage sense inputs (before the dividers)",
                   cmd_voltage);
