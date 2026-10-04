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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(fsdb01, CONFIG_FSDB01_LOG_LEVEL);

/*
 * Protocol timing, from the reference implementation: each bit is a 3 ms cell
 * that starts high (2 ms for a 1, 1 ms for a 0) and ends low. A frame is 9 bits,
 * bit 0 first, followed by a 3 ms low gap.
 */
#define FSDB01_CELL_MS      3
#define FSDB01_ONE_HIGH_MS  2
#define FSDB01_ZERO_HIGH_MS 1
#define FSDB01_GAP_MS       3

struct fsdb01_config {
    struct gpio_dt_spec out;
    /* Logical line level while disabled */
    int idle_level;
};

struct fsdb01_data {
    const struct device* dev;
    struct k_timer timer;
    /* Serializes fsdb01_enable() and fsdb01_disable() */
    struct k_mutex lock;
    /* Checked by the timer handler, so it stops cleanly on disable */
    atomic_t enabled;
    /* Frame set by fsdb01_set_frame(), latched at the start of each frame */
    atomic_t frame;

    /* Bit engine state, only touched from the timer handler and fsdb01_enable() */
    uint16_t tx_frame;
    /* 0..8: bit being sent, FSDB01_FRAME_BITS: inter-frame gap */
    uint8_t bit;
    bool high;
    /* Absolute deadline of the next edge, so re-arming doesn't accumulate drift */
    int64_t next_tick;
};

int fsdb01_set_frame(const struct device* dev, uint16_t frame) {
    struct fsdb01_data* data = dev->data;

    if (frame & ~FSDB01_FRAME_MASK) {
        return -EINVAL;
    }

    atomic_set(&data->frame, frame);
    return 0;
}

static void fsdb01_schedule(struct fsdb01_data* data, uint32_t ms) {
    data->next_tick += k_ms_to_ticks_ceil64(ms);
    k_timer_start(&data->timer, K_TIMEOUT_ABS_TICKS(data->next_tick), K_NO_WAIT);
}

int fsdb01_enable(const struct device* dev) {
    const struct fsdb01_config* config = dev->config;
    struct fsdb01_data* data = dev->data;
    int ret = 0;

    k_mutex_lock(&data->lock, K_FOREVER);

    if (!atomic_get(&data->enabled)) {
        /* The timer is stopped, so the engine state is ours to reset */
        ret = gpio_pin_set_dt(&config->out, 0);
        if (ret == 0) {
            /* Start in the gap, so the first frame begins after 3 ms low */
            data->bit = FSDB01_FRAME_BITS;
            data->high = false;
            data->next_tick = k_uptime_ticks();
            atomic_set(&data->enabled, 1);
            fsdb01_schedule(data, FSDB01_GAP_MS);
        }
    }

    k_mutex_unlock(&data->lock);
    return ret;
}

int fsdb01_disable(const struct device* dev) {
    const struct fsdb01_config* config = dev->config;
    struct fsdb01_data* data = dev->data;
    int ret;

    k_mutex_lock(&data->lock, K_FOREVER);

    /*
     * Clear the flag before stopping the timer: a handler that runs in between
     * sees it and returns without driving the line or re-arming.
     */
    atomic_set(&data->enabled, 0);
    k_timer_stop(&data->timer);
    ret = gpio_pin_set_dt(&config->out, config->idle_level);

    k_mutex_unlock(&data->lock);
    return ret;
}

/* Drive the line high for the current bit; returns the high time in ms */
static uint32_t fsdb01_start_bit(struct fsdb01_data* data, const struct fsdb01_config* config) {
    (void)gpio_pin_set_dt(&config->out, 1);
    data->high = true;

    return (data->tx_frame & BIT(data->bit)) ? FSDB01_ONE_HIGH_MS : FSDB01_ZERO_HIGH_MS;
}

/* Runs in ISR context at every edge (and at the end of the gap) */
static void fsdb01_timer_handler(struct k_timer* timer) {
    struct fsdb01_data* data = CONTAINER_OF(timer, struct fsdb01_data, timer);
    const struct fsdb01_config* config = data->dev->config;
    uint32_t ms;

    if (!atomic_get(&data->enabled)) {
        return;
    }

    if (data->high) {
        /* End of the high phase: low for the rest of the cell */
        uint32_t high_ms =
            (data->tx_frame & BIT(data->bit)) ? FSDB01_ONE_HIGH_MS : FSDB01_ZERO_HIGH_MS;

        (void)gpio_pin_set_dt(&config->out, 0);
        data->high = false;
        ms = FSDB01_CELL_MS - high_ms;
    } else if (++data->bit < FSDB01_FRAME_BITS) {
        ms = fsdb01_start_bit(data, config);
    } else if (data->bit == FSDB01_FRAME_BITS) {
        /* Inter-frame gap; the line is already low */
        ms = FSDB01_GAP_MS;
    } else {
        /* Start of a new frame */
        data->tx_frame = (uint16_t)atomic_get(&data->frame);
        data->bit = 0;
        ms = fsdb01_start_bit(data, config);
    }

    fsdb01_schedule(data, ms);
}

static int fsdb01_init(const struct device* dev) {
    const struct fsdb01_config* config = dev->config;
    struct fsdb01_data* data = dev->data;
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

    data->dev = dev;
    atomic_set(&data->frame, FSDB01_FRAME_NO_SIGNAL);
    k_mutex_init(&data->lock);
    k_timer_init(&data->timer, fsdb01_timer_handler, NULL);

    /* Send the no-signal frame from power-on, like the reference firmware */
    return fsdb01_enable(dev);
}

#define FSDB01_DEFINE(inst)                                                                    \
    static const struct fsdb01_config fsdb01_config_##inst = {                                 \
        .out = GPIO_DT_SPEC_INST_GET(inst, out_gpios),                                         \
        .idle_level = DT_INST_PROP(inst, idle_high),                                           \
    };                                                                                         \
                                                                                               \
    static struct fsdb01_data fsdb01_data_##inst;                                              \
                                                                                               \
    DEVICE_DT_INST_DEFINE(inst, fsdb01_init, NULL, &fsdb01_data_##inst, &fsdb01_config_##inst, \
                          POST_KERNEL, CONFIG_FSDB01_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(FSDB01_DEFINE)
