/*
 * CRSF receiver: reads the ELRS receiver's UART and keeps the latest values
 *
 * Bytes are parsed straight in the UART ISR. Only the latest values matter,
 * so there's no queue: each frame overwrites the stored status.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crsf.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

#include <array>

LOG_MODULE_REGISTER(crsf, LOG_LEVEL_INF);

namespace crsf {

namespace {

/*
 * Longest time a frame may take before a partial one is dropped, as in
 * Betaflight (CRSF_TIME_NEEDED_PER_FRAME_US). A 64-byte frame takes ~1.5 ms at
 * 420000 baud.
 */
constexpr uint32_t frame_timeout_cycles = k_us_to_cyc_ceil32(1750);

// Only touched from the UART ISR
Parser parser{frame_timeout_cycles};

k_spinlock lock;
// Protected by lock
Status status;

void handle_frame(const Frame& frame) {
    switch (static_cast<FrameType>(frame.type)) {
        case FrameType::rc_channels_packed:
            if (decode_channels(frame.payload, status.channels)) {
                status.channels_ticks = k_uptime_ticks();
                status.counters.rc_frames++;
            } else {
                status.counters.bad_payloads++;
            }
            break;

        case FrameType::link_statistics:
            if (decode_link_statistics(frame.payload, status.link)) {
                status.link_ticks = k_uptime_ticks();
                status.counters.link_frames++;
            } else {
                status.counters.bad_payloads++;
            }
            break;

        default:
            status.counters.other_frames++;
            break;
    }
}

void uart_isr(const device* dev, void*) {
    if (!uart_irq_update(dev)) {
        return;
    }

    K_SPINLOCK(&lock) {
        // Check before reading: reading the data clears the overrun flag
        if (uart_err_check(dev) > 0) {
            status.counters.uart_errors++;
        }

        while (uart_irq_rx_ready(dev)) {
            std::array<uint8_t, 16> buf;
            int len = uart_fifo_read(dev, buf.data(), buf.size());
            if (len <= 0) {
                break;
            }

            status.counters.bytes += len;
            uint32_t now = k_cycle_get_32();
            for (int i = 0; i < len; i++) {
                if (auto frame = parser.feed(buf[i], now)) {
                    handle_frame(*frame);
                }
            }
        }

        status.counters.parser = parser.counters();
    }
}

}  // namespace

int start(const device* uart) {
    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    int ret = uart_irq_callback_user_data_set(uart, uart_isr, nullptr);
    if (ret < 0) {
        LOG_ERR("Can't set UART callback (%d)", ret);
        return ret;
    }

    uart_irq_err_enable(uart);
    uart_irq_rx_enable(uart);
    return 0;
}

Status get_status() {
    Status copy;
    K_SPINLOCK(&lock) { copy = status; }
    return copy;
}

}  // namespace crsf
