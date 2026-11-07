/*
 * CRSF receiver: reads the ELRS receiver's UART and keeps the latest values
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRSF_H_
#define CRSF_H_

#include <zephyr/device.h>

#include <cstdint>

#include "crsf_parser.h"

namespace crsf {

struct Counters {
    /* Bytes received on the UART */
    uint32_t bytes;
    /* UART errors (overrun, framing, noise, ...) */
    uint32_t uart_errors;
    /* Valid frames, by type */
    uint32_t rc_frames;
    uint32_t link_frames;
    uint32_t other_frames;
    /* Valid CRC, but the payload doesn't fit the frame type */
    uint32_t bad_payloads;
    ParserCounters parser;
};

/* A consistent copy of everything received so far */
struct Status {
    /* Raw 11-bit values from the latest RC channels frame */
    Channels channels;
    /* k_uptime_ticks() when it arrived; 0 if none has */
    int64_t channels_ticks;

    LinkStatistics link;
    /* k_uptime_ticks() when the latest link statistics frame arrived; 0 if none has */
    int64_t link_ticks;

    Counters counters;
};

/*
 * Start receiving on the CRSF UART. Needs CONFIG_UART_INTERRUPT_DRIVEN. Frames
 * are parsed in the UART ISR.
 */
int start(const device* uart);

/* Callable from any context */
Status get_status();

}  // namespace crsf

#endif /* CRSF_H_ */
