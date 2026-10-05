/*
 * Shell commands for testing the FS-DB01 LED module driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/shell/shell.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>

#include "fsdb01.h"

namespace {

const device* const fsdb01 = DEVICE_DT_GET(DT_ALIAS(fsdb01));

struct LedName {
    const char* name;
    uint8_t bit;
};

constexpr std::array<LedName, 7> led_names{{
    {"right", 0},
    {"left", 1},
    {"ill1", 2},
    {"ill2", 3},
    {"brake", 5},
    {"nosig_fast", 6},
    {"nosig_slow", 7},
}};

/* Bits that can be addressed from the shell; bit 8 must always be 0 */
constexpr unsigned int user_bits = 8;
constexpr uint16_t all_mask = BIT_MASK(user_bits);

const char* bit_name(unsigned int bit) {
    auto it = std::ranges::find(led_names, bit, &LedName::bit);
    return it != led_names.end() ? it->name : "-";
}

bool check_ready(const shell* sh) {
    if (!device_is_ready(fsdb01)) {
        shell_error(sh, "FS-DB01 device %s not ready", fsdb01->name);
        return false;
    }
    return true;
}

/* Parse <led|all|0-7> into a frame bit mask */
std::optional<uint16_t> parse_leds(const shell* sh, std::string_view arg) {
    if (arg == "all") {
        return all_mask;
    }

    auto it = std::ranges::find(led_names, arg, &LedName::name);
    if (it != led_names.end()) {
        return BIT(it->bit);
    }

    unsigned int bit;
    auto [end, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), bit);
    if (ec == std::errc{} && end == arg.data() + arg.size() && bit < user_bits) {
        return BIT(bit);
    }

    shell_error(sh, "Unknown LED '%s'; use a name, 'all' or a bit number 0-%u", arg.data(),
                user_bits - 1);
    return std::nullopt;
}

/* Apply op(frame, mask) to the current frame */
template <typename Op>
int modify_frame(const shell* sh, char** argv, Op op) {
    if (!check_ready(sh)) {
        return -ENODEV;
    }

    auto mask = parse_leds(sh, argv[1]);
    if (!mask) {
        return -EINVAL;
    }

    uint16_t frame = op(fsdb01_get_frame(fsdb01), *mask);
    int ret = fsdb01_set_frame(fsdb01, frame);
    if (ret < 0) {
        shell_error(sh, "Failed to set frame 0x%03x (%d)", frame, ret);
    }
    return ret;
}

int cmd_status(const shell* sh, size_t, char**) {
    if (!check_ready(sh)) {
        return -ENODEV;
    }

    uint16_t frame = fsdb01_get_frame(fsdb01);
    shell_print(sh, "Output: %s   Frame: 0x%03x",
                fsdb01_is_enabled(fsdb01) ? "enabled" : "disabled", frame);
    for (unsigned int bit = 0; bit < user_bits; bit++) {
        shell_print(sh, "  %-11s (%u)  %s", bit_name(bit), bit, (frame & BIT(bit)) ? "on" : "off");
    }
    return 0;
}

int cmd_on(const shell* sh, size_t, char** argv) {
    return modify_frame(sh, argv, [](uint16_t frame, uint16_t mask) { return frame | mask; });
}

int cmd_off(const shell* sh, size_t, char** argv) {
    return modify_frame(sh, argv, [](uint16_t frame, uint16_t mask) { return frame & ~mask; });
}

int cmd_toggle(const shell* sh, size_t, char** argv) {
    return modify_frame(sh, argv, [](uint16_t frame, uint16_t mask) { return frame ^ mask; });
}

int cmd_enable(const shell* sh, size_t, char**) {
    if (!check_ready(sh)) {
        return -ENODEV;
    }

    int ret = fsdb01_enable(fsdb01);
    if (ret < 0) {
        shell_error(sh, "Enable failed (%d)", ret);
    }
    return ret;
}

int cmd_disable(const shell* sh, size_t, char**) {
    if (!check_ready(sh)) {
        return -ENODEV;
    }

    int ret = fsdb01_disable(fsdb01);
    if (ret < 0) {
        shell_error(sh, "Disable failed (%d)", ret);
    }
    return ret;
}

/* Tab completion for the LED argument: the names, then "all" */
void led_name_get(size_t idx, shell_static_entry* entry) {
    if (idx < led_names.size()) {
        entry->syntax = led_names[idx].name;
    } else if (idx == led_names.size()) {
        entry->syntax = "all";
    } else {
        entry->syntax = nullptr;
    }
    entry->help = nullptr;
    entry->subcmd = nullptr;
    entry->handler = nullptr;
}

}  // namespace

/* Shell macros expand to `cond ? arg : NULL`, so pass NULL rather than nullptr */
SHELL_DYNAMIC_CMD_CREATE(dsub_led_names, led_name_get);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_leds, SHELL_CMD(status, NULL, "Show output state and LED bits", cmd_status),
    SHELL_CMD_ARG(on, &dsub_led_names, "Turn LED on: <led|all|0-7>", cmd_on, 2, 0),
    SHELL_CMD_ARG(off, &dsub_led_names, "Turn LED off: <led|all|0-7>", cmd_off, 2, 0),
    SHELL_CMD_ARG(toggle, &dsub_led_names, "Toggle LED: <led|all|0-7>", cmd_toggle, 2, 0),
    SHELL_CMD(enable, NULL, "Start sending frames", cmd_enable),
    SHELL_CMD(disable, NULL, "Stop sending; hold line idle", cmd_disable), SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(leds, &sub_leds, "FS-DB01 LED module test commands", NULL);
