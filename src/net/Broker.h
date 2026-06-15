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

    // Runtime control channel: open a small TCP command port so a controller can
    // mutate the live link matrix / fault state while the sim is running (e.g. the
    // interactive `link` and `chaos` REPL commands). One connection per command,
    // newline-terminated text, replies "OK ...\n" or "ERR ...\n". Opening it is
    // opt-in; the broker is unaffected if never enabled.
    Result<void> enableControl(uint16_t control_port);

    // Toggle a global random packet-drop fault set ("chaos mode") on/off. Tagged
    // so it can be removed without disturbing scenario-injected faults.
    void setChaos(bool on);

    // Fault injection
    FaultInjector& faultInjector() { return fault_injector_; }

    // Stats
    uint64_t framesForwarded() const { return frames_forwarded_; }
    uint64_t framesDropped() const { return frames_dropped_; }

    // Per-link counters: how many frames passed vs were dropped by the link
    // model on the directed link from->to. This is the broker's ground-truth
    // basis for loss localization (the diagnostic-counter-differencing the
    // diagram shows: a link's drops are attributable, not just global).
    struct LinkStat {
        uint64_t forwarded = 0;  // frames that passed the channel + fault model
        uint64_t dropped = 0;    // frames the link dropped
    };
    LinkStat linkStat(NodeId from, NodeId to) const;

    // Tracer mode: opt-in. When off (the default), the broker keeps NO per-link
    // ground-truth counters — mirroring the field, where no single observer sees
    // every hop. When on, it records them (a sim luxury: the broker is the radio
    // medium). Gated so "default conditions" in the sim match real observability.
    void enableTracer(bool on = true) { tracer_enabled_ = on; }
    bool tracerEnabled() const { return tracer_enabled_; }

    // The hop sequence [src, ..., dst] the broker would relay a unicast over,
    // computed from the link-quality graph (least-hop, same as distance-vector).
    // Empty if unreachable. Pure — no sockets — so it's directly testable.
    std::vector<NodeId> route(NodeId src, NodeId dst) const;

private:
    void acceptConnections();
    // Accept one control connection, read a single command line, apply it, reply.
    void acceptControl();
    // Parse+apply a control command line; returns the reply string (newline-ended).
    std::string applyControlCommand(const std::string& line);
    void processIncoming(size_t node_idx);
    void deliverDelayed();
    void forwardFrame(NodeId src, MacFrame frame);
    // Tracer-mode multi-hop relay: walk the route, apply the per-link model at
    // each hop, record per-hop counters, deliver only if every hop survives.
    void relayMultiHop(NodeId src, NodeId final_dst, MacFrame frame, TimePoint now);
    void deliverToNode(NodeId dst, const MacFrame& frame);

    Socket listen_socket_;
    Socket control_listen_socket_;   // runtime control command port (opt-in)
    bool control_enabled_ = false;
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
    bool tracer_enabled_ = false;
    uint64_t frames_forwarded_ = 0;
    uint64_t frames_dropped_ = 0;

    // Per-link [src][dst] forward/drop counters (see linkStat()); only populated
    // when tracer_enabled_ is set.
    std::array<std::array<LinkStat, MAX_NODES>, MAX_NODES> link_stats_{};
};

} // namespace mt
