/*
 * CRSF (Crossfire / ExpressLRS) frame parser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crsf_parser.h"

namespace crsf {

namespace {

constexpr uint8_t crc_polynomial = 0xD5;
constexpr unsigned int channel_bits = 11;
constexpr uint32_t channel_mask = (1U << channel_bits) - 1;

constexpr std::array<uint8_t, 256> crc_table = [] {
    std::array<uint8_t, 256> table{};
    for (unsigned int i = 0; i < table.size(); i++) {
        uint8_t crc = i;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (crc << 1) ^ crc_polynomial : crc << 1;
        }
        table[i] = crc;
    }
    return table;
}();

}  // namespace

uint8_t crc8(std::span<const uint8_t> data) {
    uint8_t crc = 0;
    for (uint8_t byte : data) {
        crc = crc_table[crc ^ byte];
    }
    return crc;
}

bool decode_channels(std::span<const uint8_t> payload, Channels& channels) {
    if (payload.size() != rc_channels_payload_size) {
        return false;
    }

    // 16 x 11 bits = 22 bytes exactly, channel 0 in the lowest bits
    uint32_t acc = 0;
    unsigned int bits = 0;
    auto in = payload.begin();
    for (uint16_t& channel : channels) {
        while (bits < channel_bits) {
            acc |= uint32_t{*in++} << bits;
            bits += 8;
        }
        channel = acc & channel_mask;
        acc >>= channel_bits;
        bits -= channel_bits;
    }
    return true;
}

bool decode_link_statistics(std::span<const uint8_t> payload, LinkStatistics& stats) {
    if (payload.size() < link_statistics_payload_size) {
        return false;
    }

    stats = {
        .uplink_rssi_1 = payload[0],
        .uplink_rssi_2 = payload[1],
        .uplink_lq = payload[2],
        .uplink_snr = static_cast<int8_t>(payload[3]),
        .active_antenna = payload[4],
        .rf_mode = payload[5],
        .uplink_tx_power = payload[6],
        .downlink_rssi = payload[7],
        .downlink_lq = payload[8],
        .downlink_snr = static_cast<int8_t>(payload[9]),
    };
    return true;
}

int channel_to_us(uint16_t value) {
    // Linear through (172, 988) and (1811, 2012), rounded to nearest
    constexpr int span_value = channel_max - channel_min;
    constexpr int span_us = 2012 - 988;
    int n = (value - channel_min) * span_us;
    int rounding = (n >= 0 ? span_value : -span_value) / 2;
    return 988 + (n + rounding) / span_value;
}

std::optional<Frame> Parser::feed(uint8_t byte, uint32_t now) {
    if (pos_ > 0 && now - frame_start_ > frame_timeout_) {
        counters_.timeouts++;
        pos_ = 0;
    }

    if (pos_ == 0) {
        if (byte != sync_byte) {
            return std::nullopt;
        }
        frame_start_ = now;
    } else if (pos_ == 1 && (byte < min_length || byte > max_length)) {
        counters_.length_errors++;
        pos_ = 0;
        return std::nullopt;
    }

    buf_[pos_++] = byte;
    if (pos_ < 2 || pos_ < size_t{buf_[1]} + 2) {
        return std::nullopt;
    }

    // Complete: [sync] [len] [type] [payload ...] [crc]; the CRC covers type + payload
    pos_ = 0;
    const size_t length = buf_[1];
    const auto checked = std::span<const uint8_t>(buf_).subspan(2, length - 1);
    if (crc8(checked) != buf_[length + 1]) {
        counters_.crc_errors++;
        return std::nullopt;
    }

    counters_.frames++;
    return Frame{.type = checked[0], .payload = checked.subspan(1)};
}

}  // namespace crsf
