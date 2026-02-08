#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "metrics/Collector.h"
#include "metrics/Reporter.h"
#include "net/Socket.h"
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace mt {

struct NodeStatus {
    NodeId id;
    std::string role;       // "leader", "router", "sed", "phone"
    std::string state;      // "running", "stopped"
    int pid = 0;
};

struct TopologyLink {
    float loss_rate = 0.0f;
    float latency_mean_ms = 5.0f;
    bool link_up = true;
    uint8_t lqi = 200;
    int8_t rssi = -60;
};

using TopologyMatrix = std::array<std::array<TopologyLink, 4>, 4>;
using NodeStatusProvider = std::function<std::vector<NodeStatus>()>;
using TopologyProvider = std::function<TopologyMatrix()>;

class DashboardServer {
    const Collector& collector_;
    Reporter reporter_;
    uint16_t port_;
    Socket listen_socket_;
    bool running_ = false;

    NodeStatusProvider node_status_provider_;
    TopologyProvider topology_provider_;

    void handleClient(Socket client);
    void handleRequest(Socket& client, const std::string& request);
    void sendResponse(Socket& client, int status_code, const std::string& content_type,
                      const std::string& body);
    void sendNotFound(Socket& client);

    std::string buildStatusJson() const;
    std::string buildTopologyJson() const;
    static const std::string& dashboardHtml();

public:
    explicit DashboardServer(const Collector& collector, uint16_t port = 8080);

    void setNodeStatusProvider(NodeStatusProvider provider) {
        node_status_provider_ = std::move(provider);
    }

    void setTopologyProvider(TopologyProvider provider) {
        topology_provider_ = std::move(provider);
    }

    Result<void> start();
    void poll();    // Non-blocking: accept + handle one request
    void stop();

    uint16_t port() const { return port_; }
    bool running() const { return running_; }
};

} // namespace mt
