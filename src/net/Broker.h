#pragma once

#include "core/Types.h"
#include "core/Random.h"
#include "net/Socket.h"
#include "net/Frame.h"
#include "net/Channel.h"
#include "net/FaultInjector.h"
#include <array>
#include <vector>
#include <queue>
#include <functional>

namespace mt { class MeshTopology; }

namespace mt {

static constexpr size_t MAX_NODES = 4;  // 0=Leader/BR, 1=Router, 2=EndDevice, 3=Phone

struct DelayedFrame {
    TimePoint deliver_at;
    NodeId dst;
    MacFrame frame;

    bool operator>(const DelayedFrame& other) const {
        return deliver_at > other.deliver_at;
    }
};

class Broker {
public:
    explicit Broker(uint32_t seed = 42);

    // Start listening for node connections
    Result<void> start(uint16_t port = BROKER_PORT);

    // Main event loop: accept connections, forward frames
    void run();

    // Stop the broker
    void stop();

    // Link quality management
    void setLinkParams(NodeId from, NodeId to, const LinkParams& params);
    LinkParams getLinkParams(NodeId from, NodeId to) const;

    // Apply a MeshTopology preset to the link matrix
    void applyTopology(const MeshTopology& topology);

    // Fault injection
    FaultInjector& faultInjector() { return fault_injector_; }

    // Stats
    uint64_t framesForwarded() const { return frames_forwarded_; }
    uint64_t framesDropped() const { return frames_dropped_; }

private:
    void acceptConnections();
    void processIncoming(size_t node_idx);
    void deliverDelayed();
    void forwardFrame(NodeId src, MacFrame frame);
    void deliverToNode(NodeId dst, const MacFrame& frame);

    Socket listen_socket_;
    std::array<Socket, MAX_NODES> node_sockets_;
    std::array<bool, MAX_NODES> node_connected_{};
    size_t connected_count_ = 0;

    // 3x3 link quality matrix
    std::array<std::array<LinkParams, MAX_NODES>, MAX_NODES> link_matrix_;

    Random rng_;
    Channel channel_;
    FaultInjector fault_injector_;

    // Priority queue for delayed frames
    std::priority_queue<DelayedFrame, std::vector<DelayedFrame>, std::greater<>> delay_queue_;

    bool running_ = false;
    uint64_t frames_forwarded_ = 0;
    uint64_t frames_dropped_ = 0;
};

} // namespace mt
