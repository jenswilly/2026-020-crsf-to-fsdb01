/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

namespace {

constexpr gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const device* const crsf_uart = DEVICE_DT_GET(DT_ALIAS(crsf_uart));

}  // namespace

int main() {
    if (!gpio_is_ready_dt(&led) || gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
        LOG_ERR("LED not available");
        return 0;
    }

    if (!device_is_ready(crsf_uart)) {
        LOG_ERR("CRSF UART %s not ready", crsf_uart->name);
        return 0;
    }

    LOG_INF("Running on %s, CRSF on %s (C++ %ld)", CONFIG_BOARD_TARGET, crsf_uart->name,
            __cplusplus);

    while (true) {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }
}
