/*
 * CRSF (Crossfire / ExpressLRS) frame parser
 *
 * Pure protocol code, no Zephyr dependencies: bytes in, CRC-checked frames
 * out. See README, "Input: CRSF from an ELRS receiver".
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CRSF_PARSER_H_
#define CRSF_PARSER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace crsf {

// Sync byte sent by the receiver: the flight controller address
constexpr uint8_t sync_byte = 0xC8;

// The length byte counts type + payload + CRC
constexpr uint8_t min_length = 2;
constexpr uint8_t max_length = 62;
constexpr size_t max_frame_size = max_length + 2;

enum class FrameType : uint8_t {
    link_statistics = 0x14,
    rc_channels_packed = 0x16,
};

constexpr size_t num_channels = 16;
constexpr size_t rc_channels_payload_size = 22;
constexpr size_t link_statistics_payload_size = 10;

// Nominal channel range (988-2012 us), with 992 at center (1500 us)
constexpr uint16_t channel_min = 172;
constexpr uint16_t channel_mid = 992;
constexpr uint16_t channel_max = 1811;

using Channels = std::array<uint16_t, num_channels>;

// Link statistics frame (0x14), as sent by the receiver
struct LinkStatistics {
    // RSSI values are dBm with the sign dropped: 70 means -70 dBm
    uint8_t uplink_rssi_1;
    uint8_t uplink_rssi_2;
    // Link quality, 0-100 %
    uint8_t uplink_lq;
    // dB
    int8_t uplink_snr;
    uint8_t active_antenna;
    uint8_t rf_mode;
    // Transmitter power, as an index into the CRSF power table
    uint8_t uplink_tx_power;
    uint8_t downlink_rssi;
    uint8_t downlink_lq;
    int8_t downlink_snr;
};

// A received frame; payload points into the parser's buffer
struct Frame {
    uint8_t type;
    std::span<const uint8_t> payload;
};

// CRC8 with the DVB-S2 polynomial 0xD5, initial value 0
uint8_t crc8(std::span<const uint8_t> data);

// Unpack 16 little-endian 11-bit channels; false if the payload size is wrong
bool decode_channels(std::span<const uint8_t> payload, Channels& channels);

// False if the payload is too short
bool decode_link_statistics(std::span<const uint8_t> payload, LinkStatistics& stats);

// Channel value to microseconds, as ELRS maps it: 172 = 988 us, 1811 = 2012 us
int channel_to_us(uint16_t value);

struct ParserCounters {
    uint32_t frames;
    uint32_t crc_errors;
    uint32_t length_errors;
    // Partial frames dropped because the rest didn't arrive in time
    uint32_t timeouts;
};

class Parser {
public:
    /*
     * frame_timeout: the longest time a frame may take, in the units of the
     * `now` passed to feed(). After that, a partial frame is dropped.
     */
    explicit constexpr Parser(uint32_t frame_timeout)
        : frame_timeout_(frame_timeout) {}

    /*
     * Feed one received byte. now is a free-running timestamp; wraparound is
     * fine. Returns a frame once its CRC checks out. The frame's payload is
     * valid until the next call.
     */
    std::optional<Frame> feed(uint8_t byte, uint32_t now);

    const ParserCounters& counters() const { return counters_; }

private:
    uint32_t frame_timeout_;
    uint32_t frame_start_ = 0;
    size_t pos_ = 0;
    std::array<uint8_t, max_frame_size> buf_{};
    ParserCounters counters_{};
};

}  // namespace crsf

#endif  // CRSF_PARSER_H_
