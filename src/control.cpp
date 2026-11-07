/*
 * Control thread: CRSF channels to FS-DB01 frames, with failsafe
 *
 * Polls the latest CRSF status every 10 ms. The FS-DB01 sends a frame every
 * 30 ms and latches the newest one at each frame start, so polling is enough.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "control.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "crsf.h"
#include "mapping.h"

LOG_MODULE_REGISTER(control, LOG_LEVEL_INF);

namespace control {

namespace {

constexpr int poll_ms = 10;
constexpr size_t stack_size = 1024;
constexpr int priority = 5;

K_THREAD_STACK_DEFINE(stack, stack_size);
k_thread thread;

const device* output;
atomic_t manual_mode;
atomic_t link_state;

bool channels_fresh(const crsf::Status& status) {
    return status.channels_ticks != 0 &&
           k_ticks_to_ms_floor64(k_uptime_ticks() - status.channels_ticks) < link_timeout_ms;
}

void run(void*, void*, void*) {
    bool was_up = false;

    while (true) {
        const crsf::Status status = crsf::get_status();
        const bool up = channels_fresh(status);

        if (up != was_up) {
            if (up) {
                LOG_INF("Link up");
            } else {
                LOG_WRN("Link lost: no RC frames for %lld ms", link_timeout_ms);
            }
            atomic_set(&link_state, up);
            was_up = up;
        }

        if (!atomic_get(&manual_mode)) {
            uint16_t frame = up ? mapping::map_channels(status.channels, mapping::default_rules())
                                : failsafe_frame;
            fsdb01_set_frame(output, frame);
        }

        k_msleep(poll_ms);
    }
}

}  // namespace

int start(const device* fsdb01) {
    output = fsdb01;

    fsdb01_set_frame(output, failsafe_frame);
    int ret = fsdb01_enable(output);
    if (ret < 0) {
        LOG_ERR("Can't enable FS-DB01 output (%d)", ret);
        return ret;
    }

    k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), run, nullptr, nullptr, nullptr,
                    priority, 0, K_NO_WAIT);
    k_thread_name_set(&thread, "control");
    return 0;
}

bool link_up() { return atomic_get(&link_state) != 0; }

void set_manual(bool manual) { atomic_set(&manual_mode, manual); }

bool is_manual() { return atomic_get(&manual_mode) != 0; }

}  // namespace control
