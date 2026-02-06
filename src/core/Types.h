#pragma once

#include <cstdint>
#include <chrono>

namespace mt {

using NodeId     = uint16_t;
using RLOC16     = uint16_t;
using FabricId   = uint64_t;
using FabricIndex = uint16_t;
using EndpointId = uint16_t;
using ClusterId  = uint32_t;
using AttributeId = uint32_t;
using CommandId  = uint32_t;
using SubscriptionId = uint32_t;
using SessionId  = uint16_t;
using ExchangeId = uint16_t;

using Duration  = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;
using SteadyClock = std::chrono::steady_clock;

static constexpr NodeId BROADCAST_NODE = 0xFFFF;
static constexpr NodeId INVALID_NODE   = 0xFFFE;
static constexpr uint8_t INVALID_ROUTER_ID = 0xFF;
static constexpr RLOC16 INVALID_RLOC16 = 0xFFFE;

static constexpr uint16_t BROKER_PORT = 19000;
static constexpr uint32_t WIRE_MAGIC  = 0x4D545244; // "MTRD"

inline RLOC16 makeRLOC16(uint8_t routerId, uint16_t childId) {
    return static_cast<RLOC16>((static_cast<uint16_t>(routerId) << 10) | (childId & 0x03FF));
}

inline uint8_t getRouterId(RLOC16 rloc) {
    return static_cast<uint8_t>((rloc >> 10) & 0x3F);
}

inline uint16_t getChildId(RLOC16 rloc) {
    return rloc & 0x03FF;
}

} // namespace mt
