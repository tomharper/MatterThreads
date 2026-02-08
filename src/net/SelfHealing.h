#pragma once

#include "core/Types.h"
#include "thread/Routing.h"
#include "net/Discovery.h"
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>

namespace mt {

// Events emitted by the self-healing engine
enum class HealingEvent {
    NeighborLost,           // MLE timeout — neighbor unreachable
    PartitionDetected,      // Network partition (lost all routes to a node)
    RouteRecalculated,      // Distance-vector reconverged around failure
    SubscriptionDropped,    // Matter subscription liveness timeout
    SubscriptionRecovered,  // Subscription re-established after healing
    NodeReattached,         // Crashed node came back and re-attached
    ServiceReregistered,    // DNS-SD service re-registered after recovery
    BackhaulLost,           // Cellular/Wi-Fi backhaul down
    BackhaulRestored,       // Backhaul reconnected, draining buffer
    PowerDown,              // System shutting down (engine off)
    PowerUp,                // System booting up (engine on)
};

inline const char* healingEventToString(HealingEvent ev) {
    switch (ev) {
        case HealingEvent::NeighborLost:           return "NeighborLost";
        case HealingEvent::PartitionDetected:      return "PartitionDetected";
        case HealingEvent::RouteRecalculated:      return "RouteRecalculated";
        case HealingEvent::SubscriptionDropped:    return "SubscriptionDropped";
        case HealingEvent::SubscriptionRecovered:  return "SubscriptionRecovered";
        case HealingEvent::NodeReattached:         return "NodeReattached";
        case HealingEvent::ServiceReregistered:    return "ServiceReregistered";
        case HealingEvent::BackhaulLost:           return "BackhaulLost";
        case HealingEvent::BackhaulRestored:       return "BackhaulRestored";
        case HealingEvent::PowerDown:              return "PowerDown";
        case HealingEvent::PowerUp:                return "PowerUp";
    }
    return "Unknown";
}

struct HealingRecord {
    TimePoint timestamp;
    HealingEvent event;
    NodeId affected_node;
    std::string detail;
    Duration recovery_time{0};  // Time from detection to resolution
};

using HealingCallback = std::function<void(const HealingRecord&)>;

// Per-neighbor tracking state
struct NeighborState {
    NodeId node_id;
    uint8_t router_id;
    TimePoint last_heard{};
    bool reachable = true;
    uint32_t missed_advertisements = 0;
    static constexpr uint32_t MAX_MISSED = 3;
};

// Tracks backhaul (phone ↔ BR) connection state
struct BackhaulState {
    bool connected = true;
    TimePoint last_connected{};
    TimePoint disconnected_at{};
    uint32_t buffered_messages = 0;
    static constexpr uint32_t MAX_BUFFER = 1000;

    void markDisconnected(TimePoint now) {
        if (connected) {
            connected = false;
            disconnected_at = now;
        }
    }

    void markConnected(TimePoint now) {
        connected = true;
        last_connected = now;
    }

    Duration downtime(TimePoint now) const {
        if (connected) return Duration(0);
        return std::chrono::duration_cast<Duration>(now - disconnected_at);
    }
};

class SelfHealingEngine {
public:
    // Set callback for healing events
    void onHealingEvent(HealingCallback cb) { callback_ = std::move(cb); }

    // Called periodically by ThreadNode::tick()
    void tick(TimePoint now, const RoutingTable& routing);

    // Notify when an MLE advertisement is received from a neighbor
    void onNeighborHeard(NodeId node_id, uint8_t router_id, TimePoint now);

    // Notify when a node has re-attached after a crash
    void onNodeReattached(NodeId node_id, TimePoint now);

    // Notify when a subscription is dropped
    void onSubscriptionDropped(NodeId remote_node, SubscriptionId sub_id, TimePoint now);

    // Notify when a subscription is re-established
    void onSubscriptionRecovered(NodeId remote_node, SubscriptionId sub_id, TimePoint now);

    // Backhaul management
    void onBackhaulLost(TimePoint now);
    void onBackhaulRestored(TimePoint now);

    // Power lifecycle
    void onSystemPowerDown(TimePoint now);
    void onSystemPowerUp(TimePoint now);
    BackhaulState& backhaulState() { return backhaul_; }
    const BackhaulState& backhaulState() const { return backhaul_; }

    // Query state
    bool isNeighborReachable(NodeId node_id) const;
    std::vector<NodeId> unreachableNeighbors() const;
    const std::vector<HealingRecord>& history() const { return history_; }
    size_t partitionsDetected() const { return partitions_detected_; }

    // Configuration
    void setNeighborTimeout(Duration timeout) { neighbor_timeout_ = timeout; }
    Duration neighborTimeout() const { return neighbor_timeout_; }

private:
    void emit(HealingEvent event, NodeId node, const std::string& detail,
              Duration recovery = Duration(0));
    void checkNeighborLiveness(TimePoint now);
    void checkPartitions(const RoutingTable& routing, TimePoint now);

    std::unordered_map<NodeId, NeighborState> neighbors_;
    BackhaulState backhaul_;

    Duration neighbor_timeout_{25000};  // 2.5x MLE interval
    std::vector<HealingRecord> history_;
    HealingCallback callback_;
    size_t partitions_detected_ = 0;

    // Track previously reachable routers to detect new partitions
    std::vector<uint8_t> prev_reachable_routers_;
};

} // namespace mt
