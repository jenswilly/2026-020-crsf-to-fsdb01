/*
 * FlySky FS-DB01 LED control module driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT flysky_fsdb01

#include "fsdb01.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(fsdb01, CONFIG_FSDB01_LOG_LEVEL);

struct fsdb01_config {
    struct gpio_dt_spec out;
};

struct fsdb01_data {
    atomic_t frame;
};

int fsdb01_set_frame(const struct device *dev, uint16_t frame) {
    struct fsdb01_data *data = dev->data;

    if (frame & ~FSDB01_FRAME_MASK) {
        return -EINVAL;
    }

    atomic_set(&data->frame, frame);
    return 0;
}

static int fsdb01_init(const struct device *dev) {
    const struct fsdb01_config *config = dev->config;
    struct fsdb01_data *data = dev->data;
    int ret;

    if (!gpio_is_ready_dt(&config->out)) {
        LOG_ERR("Output GPIO not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&config->out, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure output GPIO (%d)", ret);
        return ret;
    }

    atomic_set(&data->frame, FSDB01_FRAME_NO_SIGNAL);

    /* TODO: start the bit engine that sends the frame continuously */

    return 0;
}

#define FSDB01_DEFINE(inst)                                                          \
    static const struct fsdb01_config fsdb01_config_##inst = {                       \
        .out = GPIO_DT_SPEC_INST_GET(inst, out_gpios),                               \
    };                                                                               \
                                                                                     \
    static struct fsdb01_data fsdb01_data_##inst;                                    \
                                                                                     \
    DEVICE_DT_INST_DEFINE(inst, fsdb01_init, NULL, &fsdb01_data_##inst,              \
                          &fsdb01_config_##inst, POST_KERNEL,                        \
                          CONFIG_FSDB01_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(FSDB01_DEFINE)
