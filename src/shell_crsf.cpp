/*
 * Shell commands for inspecting the CRSF input
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include <cstdint>

#include "crsf.h"

namespace {

/* Prints the age of a timestamp from crsf::Status, or "never" */
void print_age(const shell* sh, const char* what, int64_t ticks) {
    if (ticks == 0) {
        shell_print(sh, "%-17s never", what);
        return;
    }
    shell_print(sh, "%-17s %lld ms ago", what, k_ticks_to_ms_floor64(k_uptime_ticks() - ticks));
}

int cmd_status(const shell* sh, size_t, char**) {
    const crsf::Status status = crsf::get_status();
    const crsf::Counters& c = status.counters;

    print_age(sh, "RC channels:", status.channels_ticks);
    print_age(sh, "Link statistics:", status.link_ticks);
    shell_print(sh, "Bytes: %u   UART errors: %u", c.bytes, c.uart_errors);
    shell_print(sh, "Frames: %u (RC %u, link %u, other %u)", c.parser.frames, c.rc_frames,
                c.link_frames, c.other_frames);
    shell_print(sh, "Errors: CRC %u, length %u, timeout %u, payload %u", c.parser.crc_errors,
                c.parser.length_errors, c.parser.timeouts, c.bad_payloads);

    if (status.link_ticks != 0) {
        const crsf::LinkStatistics& l = status.link;
        shell_print(sh, "Uplink:   RSSI -%u/-%u dBm (antenna %u), LQ %u %%, SNR %d dB, power %u",
                    l.uplink_rssi_1, l.uplink_rssi_2, l.active_antenna, l.uplink_lq, l.uplink_snr,
                    l.uplink_tx_power);
        shell_print(sh, "Downlink: RSSI -%u dBm, LQ %u %%, SNR %d dB   RF mode %u", l.downlink_rssi,
                    l.downlink_lq, l.downlink_snr, l.rf_mode);
    }
    return 0;
}

int cmd_channels(const shell* sh, size_t, char**) {
    const crsf::Status status = crsf::get_status();

    print_age(sh, "RC channels:", status.channels_ticks);
    if (status.channels_ticks == 0) {
        return 0;
    }

    for (size_t i = 0; i < status.channels.size(); i++) {
        uint16_t value = status.channels[i];
        shell_print(sh, "  ch%-2u  %4u  %4d us", i + 1, value, crsf::channel_to_us(value));
    }
    return 0;
}

}  // namespace

/* Shell macros expand to `cond ? arg : NULL`, so pass NULL rather than nullptr */
SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_crsf,
    SHELL_CMD(status, NULL, "Show frame counters, errors and link statistics", cmd_status),
    SHELL_CMD(channels, NULL, "Show the latest RC channel values", cmd_channels),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(crsf, &sub_crsf, "CRSF input commands", NULL);
