#include "net/FaultInjector.h"
#include <algorithm>

namespace mt {

DeliveryDecision FaultInjector::applyFaults(NodeId src, NodeId dst, MacFrame& frame, TimePoint now) {
    DeliveryDecision decision;
    decision.deliver = true;
    decision.delay = Duration{0};

    for (const auto& rule : rules_) {
        if (!rule.matchesLink(src, dst)) continue;
        if (!rule.isActive(now)) continue;
        if (!rng_.chance(static_cast<double>(rule.probability))) continue;

        switch (rule.type) {
        case FaultType::PacketDrop:
            if (rng_.chance(static_cast<double>(rule.drop_rate))) {
                decision.deliver = false;
                return decision;
            }
            break;

        case FaultType::LatencySpike:
            decision.delay += rule.extra_latency;
            break;

        case FaultType::Corrupt:
            if (!frame.payload.empty()) {
                for (uint32_t i = 0; i < rule.corrupt_bit_count; ++i) {
                    size_t byte_idx = static_cast<size_t>(rng_.nextInt(
                        0, static_cast<int>(frame.payload.size()) - 1));
                    int bit_idx = rng_.nextInt(0, 7);
                    frame.payload[byte_idx] ^= static_cast<uint8_t>(1 << bit_idx);
                }
            }
            break;

        case FaultType::Duplicate:
            // Caller should check this flag and send the frame twice
            // We signal via a negative delay (hacky but simple)
            // TODO: proper duplicate signaling via DeliveryDecision flags
            break;

        case FaultType::LinkDown:
            decision.deliver = false;
            return decision;

        case FaultType::LinkDegrade:
            decision.delivered_lqi = static_cast<uint8_t>(
                std::max(0, static_cast<int>(decision.delivered_lqi) - 50));
            decision.delivered_rssi = static_cast<int8_t>(
                std::max(-127, static_cast<int>(decision.delivered_rssi) - 20));
            break;

        case FaultType::PartialPartition:
            decision.deliver = false;
            return decision;

        case FaultType::Reorder:
            // Add random extra delay to cause reordering
            decision.delay += Duration(rng_.nextInt(10, 200));
            break;

        case FaultType::NodeCrash:
        case FaultType::NodeFreeze:
            // These are handled by the controller, not the broker
            break;
        }
    }

    return decision;
}

void FaultInjector::purgeExpired(TimePoint now) {
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
            [now](const FaultRule& r) { return !r.isActive(now); }),
        rules_.end());
}

} // namespace mt
