#pragma once

#include <cstddef>
#include <cstdint>

namespace screen_stream {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kLinesPerPacket = 4;
constexpr int kChunksPerFrame = kHeight / kLinesPerPacket;
constexpr size_t kHeaderSize = 12;
constexpr size_t kPayloadSize = kWidth * kLinesPerPacket;
constexpr uint8_t kProtocolVersion = 1;
constexpr uint8_t kPacketFrameData = 1;
constexpr uint8_t kPacketHeartbeat = 2;
constexpr uint8_t kPacketControl = 3;
constexpr uint8_t kColorRgb332 = 1;

struct PacketHeader {
    uint8_t packet_type = 0;
    uint16_t frame_id = 0;
    uint16_t y_start = 0;
    uint8_t line_count = 0;
    uint8_t color_mode = 0;
    uint16_t payload_length = 0;
};

inline uint16_t ReadBe16(const uint8_t* value) {
    return static_cast<uint16_t>((value[0] << 8) | value[1]);
}

inline bool ParseHeader(const uint8_t* packet, size_t length, PacketHeader* header) {
    if (!packet || !header || length < kHeaderSize || packet[0] != 'S' || packet[1] != 'S' ||
        packet[2] != kProtocolVersion) {
        return false;
    }
    header->packet_type = packet[3];
    header->frame_id = ReadBe16(packet + 4);
    header->y_start = ReadBe16(packet + 6);
    header->line_count = packet[8];
    header->color_mode = packet[9];
    header->payload_length = ReadBe16(packet + 10);
    return kHeaderSize + header->payload_length == length;
}

}  // namespace screen_stream
