#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace matter {
namespace sim {

// Node roles from the Thread spec
enum class NodeRole { Detached, EndDevice, REED, Router, Leader };

// Node state in the simulation
struct NodeState {
    uint16_t nodeId;
    std::string role;     // "leader", "router", "sed", "phone"
    std::string state;    // "running", "crashed", "frozen"
    int pid = 0;
    bool reachable = true;
};

// Link quality between two nodes
struct LinkInfo {
    float lossPercent = 0.0f;
    float latencyMs = 5.0f;
    bool up = true;
    uint8_t lqi = 200;
    int8_t rssi = -60;
};

// Timeline event from the simulation
struct TimelineEvent {
    uint64_t timeMs;
    uint16_t nodeId;
    std::string category;
    std::string event;
    std::string detail;
};

// Van state from the Gateway API
struct VanState {
    std::string vanId;
    std::string name;
    std::string state;  // "Registered", "Online", "Offline", "Unreachable"
    std::string deviceId;
    bool locked = true;
};

// Fleet-level alert
struct FleetAlert {
    std::string vanId;
    std::string severity;   // "critical", "warning", "info"
    std::string message;
    uint64_t timestampMs;
};

// Manages simulation state received from HTTP APIs
class SimulationState {
public:
    // Update from Dashboard API responses (JSON strings)
    void updateNodes(const std::string& statusJson);
    void updateTopology(const std::string& topologyJson);
    void updateTimeline(const std::string& timelineJson);
    void updateMetrics(const std::string& metricsJson);

    // Update from Gateway API responses
    void updateVans(const std::string& vansJson);
    void updateFleetStatus(const std::string& fleetJson);
    void updateAlerts(const std::string& alertsJson);

    // Accessors
    const std::vector<NodeState>& nodes() const { return nodes_; }
    const std::array<std::array<LinkInfo, 4>, 4>& topology() const { return topology_; }
    const std::vector<TimelineEvent>& timeline() const { return timeline_; }
    const std::vector<VanState>& vans() const { return vans_; }
    const std::vector<FleetAlert>& alerts() const { return alerts_; }

    // Analysis
    std::string meshSummary() const;
    std::string fleetSummary() const;
    std::string nodeDescription(uint16_t nodeId) const;
    std::vector<std::string> activeAlertMessages() const;
    bool isHealthy() const;
    int onlineVanCount() const;
    int totalVanCount() const;

    // For AI chat integration
    std::string answerSimulationQuery(const std::string& query) const;

    // Counters from metrics
    struct MetricsSummary {
        uint64_t framesSent = 0;
        uint64_t framesReceived = 0;
        uint64_t framesDropped = 0;
        double avgLatencyMs = 0.0;
    };
    MetricsSummary metricsSummary() const { return metrics_; }

private:
    std::vector<NodeState> nodes_;
    std::array<std::array<LinkInfo, 4>, 4> topology_{};
    std::vector<TimelineEvent> timeline_;
    std::vector<VanState> vans_;
    std::vector<FleetAlert> alerts_;
    MetricsSummary metrics_;
};

} // namespace sim
} // namespace matter
