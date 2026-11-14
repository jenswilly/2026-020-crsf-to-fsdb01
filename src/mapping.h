/*
 * CRSF channels to FS-DB01 frame mapping
 *
 * Each rule ties one CRSF channel interval to FS-DB01 outputs. A temporary rule
 * turns its outputs on while the channel is inside the interval. A toggle rule
 * toggles its outputs each time the channel enters the interval. An output is
 * on if a temporary rule matches or it's toggled on.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAPPING_H_
#define MAPPING_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "crsf_parser.h"

namespace mapping {

// Min and max channel values in microseconds - used in rules
constexpr uint16_t kMaxValue = 2012;
constexpr uint16_t kMinValue = 988;
constexpr uint16_t kMidValue = 1500;

/**
 * @brief Converts a percentage (0-100) to a channel value in microseconds
 * @param percent Percentage value (0-100)
 * @return Channel value in microseconds corresponding to the given percentage; if the percentage is
 * out of range, the result is clamped to the min or max value
 */
constexpr uint16_t PercentToUs(uint16_t percent) {
    return std::clamp(static_cast<uint16_t>(kMinValue + (kMaxValue - kMinValue) * percent / 100),
                      kMinValue, kMaxValue);
}

// Toggle rules beyond this many never match (State tracks one bit per rule)
constexpr size_t kMaxRules = 32;

enum class Behaviour : uint8_t {
    kTemporary,  ///< Output is on only while the channel is inside the interval
    kToggle,     ///< Output toggles when the channel enters the interval
};

struct Rule {
    uint8_t channel;  ///< CRSF channel, 1-16 as numbered on the radio
    uint16_t min_us;  ///< Minimum channel value in microseconds (see crsf::channel_to_us())
    uint16_t max_us;  ///< Maximum channel value in microseconds
    uint16_t output;  ///< FS-DB01 frame bits to turn on or toggle (FSDB01_* flags)
    Behaviour behaviour = Behaviour::kTemporary;
};

// What map_channels() remembers between calls. Zero-initialized: nothing toggled on, all toggle
// rules armed.
struct State {
    // Outputs currently toggled on. Shared by all toggle rules, so two rules can toggle the same
    // output.
    uint16_t toggled = 0;
    // Bit n is set while rule n's channel is inside its interval (toggle rules only)
    uint32_t in_range = 0;
};

// The rules used until they're configurable
std::span<const Rule> default_rules();

// FS-DB01 frame for the given channels; rules with an invalid channel never match. Updates the
// toggle state, so call it once per new set of channels, with the same rules each time.
uint16_t map_channels(const crsf::Channels& channels, std::span<const Rule> rules, State& state);

}  // namespace mapping

#endif /* MAPPING_H_ */
