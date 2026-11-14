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

// Hardcoded for now; later configurable from the shell and stored in flash
constexpr std::array rules{
    // Steering: left and right
    Rule{.channel = 2,
         .min_us = kMaxValue - 100,
         .max_us = kMaxValue,
         .output = FSDB01_TURN_LEFT,
         .behaviour = Behaviour::kTemporary},
    Rule{.channel = 2,
         .min_us = kMinValue,
         .max_us = kMinValue + 100,
         .output = FSDB01_TURN_RIGHT,
         .behaviour = Behaviour::kTemporary},
    // Channel 5: front and top lights toggle
    Rule{.channel = 5,
         .min_us = kMaxValue - 100,
         .max_us = kMaxValue,
         .output = FSDB01_FRONT_AND_TOP,
         .behaviour = Behaviour::kToggle},
    // Throttle near center: brake light
    Rule{.channel = 1,
         .min_us = 1400,
         .max_us = 1600,
         .output = FSDB01_REVERSE_BRAKE,
         .behaviour = Behaviour::kTemporary},
};

}  // namespace

static_assert(rules.size() <= kMaxRules);

std::span<const Rule> default_rules() { return rules; }

uint16_t map_channels(const crsf::Channels& channels, std::span<const Rule> rules, State& state) {
    uint16_t frame = 0;
    for (size_t i = 0; i < rules.size(); i++) {
        const Rule& rule = rules[i];
        if (rule.channel < 1 || rule.channel > channels.size()) {
            continue;
        }

        int us = crsf::channel_to_us(channels[rule.channel - 1]);
        const bool in_range = us >= rule.min_us && us <= rule.max_us;

        switch (rule.behaviour) {
            case Behaviour::kTemporary:
                if (in_range) {
                    frame |= rule.output;
                }
                break;

            case Behaviour::kToggle: {
                if (i >= kMaxRules) {
                    break;
                }
                // Toggle only on entering the interval; leaving it re-arms the rule
                const uint32_t bit = UINT32_C(1) << i;
                if (in_range && !(state.in_range & bit)) {
                    state.toggled ^= rule.output;
                }
                state.in_range = in_range ? (state.in_range | bit) : (state.in_range & ~bit);
                break;
            }
        }
    }
    return frame | state.toggled;
}

}  // namespace mapping
