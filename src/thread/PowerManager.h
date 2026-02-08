#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include <vector>
#include <functional>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace mt {

// System-wide power state
enum class PowerState : uint8_t {
    EngineOn,      // Full power, all nodes operational
    ShuttingDown,  // Battery window countdown (60-90s)
    Off,           // All power lost
    Booting        // Cold boot, nodes coming back online
};

inline const char* powerStateToString(PowerState s) {
    switch (s) {
        case PowerState::EngineOn:     return "EngineOn";
        case PowerState::ShuttingDown: return "ShuttingDown";
        case PowerState::Off:          return "Off";
        case PowerState::Booting:      return "Booting";
    }
    return "Unknown";
}

// Per-node power state
enum class NodePowerState : uint8_t {
    On,           // Node operational
    ShuttingDown, // Node performing graceful shutdown
    Off,          // Node powered off
    Booting       // Node booting up
};

inline const char* nodePowerStateToString(NodePowerState s) {
    switch (s) {
        case NodePowerState::On:           return "On";
        case NodePowerState::ShuttingDown: return "ShuttingDown";
        case NodePowerState::Off:          return "Off";
        case NodePowerState::Booting:      return "Booting";
    }
    return "Unknown";
}

// Shutdown priority — lower value = shut down first
enum class ShutdownPriority : uint8_t {
    Sensor = 0,        // Sensors shut down first
    Relay = 1,         // Relay routers next
    BorderRouter = 2   // BR shuts down last (drains buffers)
};

// Events emitted during power lifecycle
enum class PowerEvent {
    ShutdownInitiated,     // Engine off, countdown started
    NodeShutdownStarted,   // Node beginning graceful shutdown
    NodeShutdownComplete,  // Node finished graceful shutdown
    NodeHardCutoff,        // Node lost power without graceful shutdown
    SystemOff,             // All nodes off
    BootInitiated,         // Engine on, boot sequence starting
    NodeBootStarted,       // Node beginning boot
    NodeBootComplete,      // Node finished booting
    SystemOn               // All nodes operational
};

inline const char* powerEventToString(PowerEvent ev) {
    switch (ev) {
        case PowerEvent::ShutdownInitiated:    return "ShutdownInitiated";
        case PowerEvent::NodeShutdownStarted:  return "NodeShutdownStarted";
        case PowerEvent::NodeShutdownComplete: return "NodeShutdownComplete";
        case PowerEvent::NodeHardCutoff:       return "NodeHardCutoff";
        case PowerEvent::SystemOff:            return "SystemOff";
        case PowerEvent::BootInitiated:        return "BootInitiated";
        case PowerEvent::NodeBootStarted:      return "NodeBootStarted";
        case PowerEvent::NodeBootComplete:     return "NodeBootComplete";
        case PowerEvent::SystemOn:             return "SystemOn";
    }
    return "Unknown";
}

struct PowerEventRecord {
    TimePoint timestamp;
    PowerEvent event;
    NodeId node_id;      // INVALID_NODE for system-wide events
    std::string detail;
};

using PowerEventCallback = std::function<void(const PowerEventRecord&)>;
using NodeShutdownCallback = std::function<Result<void>(NodeId, TimePoint)>;
using NodeBootCallback = std::function<Result<void>(NodeId, TimePoint)>;

// Per-node power tracking
struct NodePowerInfo {
    NodeId node_id;
    ShutdownPriority priority;
    NodePowerState state = NodePowerState::On;
    TimePoint shutdown_at{};   // Scheduled shutdown time
    TimePoint boot_at{};       // Scheduled boot time
    bool shutdown_clean = false; // true if shutdown callback succeeded
};

class PowerManager {
public:
    // Configuration
    static constexpr Duration DEFAULT_SHUTDOWN_PER_NODE{10000}; // 10s per node
    static constexpr Duration DEFAULT_BOOT_PER_NODE{3000};      // 3s per node

    // Register a node with its shutdown priority
    void registerNode(NodeId node_id, ShutdownPriority priority);

    // Set per-node callbacks
    void setNodeShutdownCallback(NodeId node_id, NodeShutdownCallback cb);
    void setNodeBootCallback(NodeId node_id, NodeBootCallback cb);

    // Set event observer
    void onPowerEvent(PowerEventCallback cb) { event_cb_ = std::move(cb); }

    // Power lifecycle control
    Result<void> initiateShutdown(TimePoint now, Duration battery_life);
    Result<void> hardCutoff(TimePoint now);
    Result<void> initiateBoot(TimePoint now);

    // Advance state machine — call periodically
    void tick(TimePoint now);

    // Query state
    PowerState systemState() const { return state_; }
    NodePowerState nodeState(NodeId node_id) const;
    bool isNodeOperational(NodeId node_id) const;
    Duration shutdownRemaining(TimePoint now) const;

    // Statistics
    const std::vector<PowerEventRecord>& history() const { return history_; }
    size_t nodesShutdownGracefully() const { return graceful_shutdowns_; }
    size_t nodesHardCutoff() const { return hard_cutoffs_; }
    size_t registeredNodeCount() const { return nodes_.size(); }

private:
    void emit(PowerEvent event, NodeId node_id, TimePoint now,
              const std::string& detail = "");
    void scheduleShutdownSequence(TimePoint now, Duration battery_life);
    void scheduleBootSequence(TimePoint now);
    NodePowerInfo* findNode(NodeId node_id);
    const NodePowerInfo* findNode(NodeId node_id) const;

    PowerState state_ = PowerState::EngineOn;
    std::vector<NodePowerInfo> nodes_;

    std::unordered_map<NodeId, NodeShutdownCallback> shutdown_cbs_;
    std::unordered_map<NodeId, NodeBootCallback> boot_cbs_;

    TimePoint shutdown_initiated_{};
    Duration battery_life_{};

    std::vector<PowerEventRecord> history_;
    PowerEventCallback event_cb_;

    size_t graceful_shutdowns_ = 0;
    size_t hard_cutoffs_ = 0;

    Duration boot_per_node_ = DEFAULT_BOOT_PER_NODE;
};

} // namespace mt
