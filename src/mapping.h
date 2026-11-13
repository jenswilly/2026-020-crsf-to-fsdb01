/*
 * CRSF channels to FS-DB01 frame mapping
 *
 * Each rule turns one FS-DB01 output on while one CRSF channel is inside an
 * interval. An output is on if any of its rules match.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPING_H_
#define MAPPING_H_

#include <cstdint>
#include <span>

#include "crsf_parser.h"

namespace mapping {

constexpr uint16_t kMaxValue = 2012;
constexpr uint16_t kMinValue = 988;

struct Rule {
    // CRSF channel, 1-16 as numbered on the radio
    uint8_t channel;
    // Inclusive interval in us (see crsf::channel_to_us())
    uint16_t min_us;
    uint16_t max_us;
    // FS-DB01 frame bits to turn on (FSDB01_* flags)
    uint16_t output;
};

// The rules used until they're configurable
std::span<const Rule> default_rules();

// FS-DB01 frame for the given channels; rules with an invalid channel never match
uint16_t map_channels(const crsf::Channels& channels, std::span<const Rule> rules);

}  // namespace mapping

#endif /* MAPPING_H_ */
