#pragma once

#include "core/Types.h"
#include "core/Random.h"
#include "net/Frame.h"
#include "net/Channel.h"
#include <vector>
#include <string>

namespace mt {

enum class FaultType {
    PacketDrop,
    LatencySpike,
    Reorder,
    Corrupt,
    Duplicate,
    LinkDown,
    LinkDegrade,
    NodeCrash,
    NodeFreeze,
    PartialPartition
};

static constexpr NodeId ANY_NODE = 0xFFFF;
static constexpr Duration INDEFINITE = Duration::max();

struct FaultRule {
    FaultType type;
    NodeId affected_src = ANY_NODE;
    NodeId affected_dst = ANY_NODE;
    float probability = 1.0f;
    Duration duration = INDEFINITE;
    TimePoint start_time{};

    // Type-specific parameters
    float drop_rate = 0.5f;
    Duration extra_latency{100};
    uint32_t corrupt_bit_count = 1;
    Duration freeze_duration{5000};

    std::string description;

    bool matchesLink(NodeId src, NodeId dst) const {
        return (affected_src == ANY_NODE || affected_src == src) &&
               (affected_dst == ANY_NODE || affected_dst == dst);
    }

    bool isActive(TimePoint now) const {
        if (duration == INDEFINITE) return true;
        return (now - start_time) < duration;
    }
};

class FaultInjector {
    std::vector<FaultRule> rules_;
    Random& rng_;

public:
    explicit FaultInjector(Random& rng) : rng_(rng) {}

    void addRule(FaultRule rule) { rules_.push_back(std::move(rule)); }

    void removeRule(size_t index) {
        if (index < rules_.size()) {
            rules_.erase(rules_.begin() + static_cast<ptrdiff_t>(index));
        }
    }

    void clearRules() { rules_.clear(); }
    const std::vector<FaultRule>& rules() const { return rules_; }

    // Apply all active fault rules to a frame. Returns modified delivery decision.
    DeliveryDecision applyFaults(NodeId src, NodeId dst, MacFrame& frame, TimePoint now);

    // Remove expired rules
    void purgeExpired(TimePoint now);
};

} // namespace mt
