#pragma once

#include "core/Types.h"
#include "core/ByteBuffer.h"
#include <vector>
#include <cstring>

namespace mt {

// Wire header for frames between nodes and broker
struct WireHeader {
    uint32_t magic = WIRE_MAGIC;
    uint16_t src_node = INVALID_NODE;
    uint16_t dst_node = INVALID_NODE;
    uint16_t payload_len = 0;
    uint8_t  channel = 15;
    uint8_t  flags = 0;

    static constexpr size_t SIZE = 12;

    static constexpr uint8_t FLAG_ACK_REQ  = 0x01;
    static constexpr uint8_t FLAG_SECURED  = 0x02;
    static constexpr uint8_t FLAG_BROADCAST = 0x04;

    void serialize(uint8_t* buf) const {
        std::memcpy(buf + 0, &magic, 4);
        std::memcpy(buf + 4, &src_node, 2);
        std::memcpy(buf + 6, &dst_node, 2);
        std::memcpy(buf + 8, &payload_len, 2);
        buf[10] = channel;
        buf[11] = flags;
    }

    static WireHeader deserialize(const uint8_t* buf) {
        WireHeader h;
        std::memcpy(&h.magic, buf + 0, 4);
        std::memcpy(&h.src_node, buf + 4, 2);
        std::memcpy(&h.dst_node, buf + 6, 2);
        std::memcpy(&h.payload_len, buf + 8, 2);
        h.channel = buf[10];
        h.flags = buf[11];
        return h;
    }
};

// Simulated 802.15.4 MAC frame
struct MacFrame {
    enum class Type : uint8_t {
        Beacon = 0, Data = 1, Ack = 2, Command = 3
    };

    Type type = Type::Data;
    uint8_t  seq_number = 0;
    uint16_t dst_pan_id = 0xFFFF;
    RLOC16   dst_addr = INVALID_RLOC16;
    uint16_t src_pan_id = 0xFFFF;
    RLOC16   src_addr = INVALID_RLOC16;
    std::vector<uint8_t> payload;

    // Set by broker on delivery
    uint8_t lqi = 0;
    int8_t  rssi = -100;

    // Serialize to wire format (payload of WireHeader)
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf;
        buf.reserve(9 + payload.size());
        buf.push_back(static_cast<uint8_t>(type));
        buf.push_back(seq_number);
        buf.push_back(static_cast<uint8_t>(dst_pan_id & 0xFF));
        buf.push_back(static_cast<uint8_t>((dst_pan_id >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(dst_addr & 0xFF));
        buf.push_back(static_cast<uint8_t>((dst_addr >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(src_addr & 0xFF));
        buf.push_back(static_cast<uint8_t>((src_addr >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
        buf.insert(buf.end(), payload.begin(), payload.end());
        return buf;
    }

    static MacFrame deserialize(const uint8_t* data, size_t len) {
        MacFrame f;
        if (len < 9) return f;
        f.type = static_cast<Type>(data[0]);
        f.seq_number = data[1];
        f.dst_pan_id = static_cast<uint16_t>(static_cast<uint16_t>(data[2]) | static_cast<uint16_t>(static_cast<uint16_t>(data[3]) << 8));
        f.dst_addr = static_cast<RLOC16>(static_cast<uint16_t>(data[4]) | static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8));
        f.src_addr = static_cast<RLOC16>(static_cast<uint16_t>(data[6]) | static_cast<uint16_t>(static_cast<uint16_t>(data[7]) << 8));
        uint8_t plen = data[8];
        if (len >= 9u + plen) {
            f.payload.assign(data + 9, data + 9 + plen);
        }
        return f;
    }
};

} // namespace mt
