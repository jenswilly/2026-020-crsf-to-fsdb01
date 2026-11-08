/*
 * CRSF channels to FS-DB01 frame mapping
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mapping.h"

#include <array>

#include "fsdb01.h"

namespace mapping {

namespace {

/* Hardcoded for now; later configurable from the shell and stored in flash */
constexpr std::array rules{
    /* 3-position switch on ch5: low = left, high = right */
    Rule{.channel = 2, .min_us = 1700, .max_us = 2100, .output = FSDB01_TURN_RIGHT},
    Rule{.channel = 2, .min_us = 900, .max_us = 1300, .output = FSDB01_TURN_LEFT},
    /* 3-position switch on ch6: mid = step 1, high = steps 1 and 2 */
    Rule{.channel = 5, .min_us = 900, .max_us = 1400, .output = FSDB01_FRONT_AND_TOP},
    /* Throttle near center */
    Rule{.channel = 1, .min_us = 1400, .max_us = 1600, .output = FSDB01_REVERSE_BRAKE},
};

}  // namespace

std::span<const Rule> default_rules() { return rules; }

uint16_t map_channels(const crsf::Channels& channels, std::span<const Rule> rules) {
    uint16_t frame = 0;
    for (const Rule& rule : rules) {
        if (rule.channel < 1 || rule.channel > channels.size()) {
            continue;
        }

        int us = crsf::channel_to_us(channels[rule.channel - 1]);
        if (us >= rule.min_us && us <= rule.max_us) {
            frame |= rule.output;
        }
    }
    return frame;
}

}  // namespace mapping
