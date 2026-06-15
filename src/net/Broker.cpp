#include "net/Broker.h"
#include "thread/MeshTopology.h"
#include "thread/PathTracer.h"
#include "core/Log.h"

#include <poll.h>
#include <algorithm>
#include <string>
#include <sstream>

namespace mt {

Broker::Broker(uint32_t seed)
    : rng_(seed), channel_(rng_), fault_injector_(rng_)
{
    // Initialize link matrix: all links fully connected with good quality
    for (size_t i = 0; i < MAX_NODES; ++i) {
        for (size_t j = 0; j < MAX_NODES; ++j) {
            link_matrix_[i][j] = LinkParams{};
        }
    }
}

Result<void> Broker::start(uint16_t port) {
    auto result = Socket::listen(port);
    if (!result) return result.error();
    listen_socket_ = std::move(*result);
    running_ = true;
    MT_INFO("broker", "Listening on port " + std::to_string(port));
    return Result<void>::success();
}

void Broker::stop() {
    running_ = false;
    listen_socket_.close();
    control_listen_socket_.close();
    for (auto& s : node_sockets_) s.close();
    MT_INFO("broker", "Stopped");
}

void Broker::setLinkParams(NodeId from, NodeId to, const LinkParams& params) {
    if (from < MAX_NODES && to < MAX_NODES) {
        link_matrix_[from][to] = params;
    }
}

LinkParams Broker::getLinkParams(NodeId from, NodeId to) const {
    if (from < MAX_NODES && to < MAX_NODES) {
        return link_matrix_[from][to];
    }
    return LinkParams{};
}

Broker::LinkStat Broker::linkStat(NodeId from, NodeId to) const {
    if (from < MAX_NODES && to < MAX_NODES) {
        return link_stats_[from][to];
    }
    return LinkStat{};
}

void Broker::applyTopology(const MeshTopology& topology) {
    for (size_t i = 0; i < MAX_NODES; ++i) {
        for (size_t j = 0; j < MAX_NODES; ++j) {
            link_matrix_[i][j] = topology.getLinkParams(
                static_cast<NodeId>(i), static_cast<NodeId>(j));
        }
    }
}

Result<void> Broker::enableControl(uint16_t control_port) {
    auto result = Socket::listen(control_port);
    if (!result) return result.error();
    control_listen_socket_ = std::move(*result);
    control_enabled_ = true;
    MT_INFO("broker", "Control channel listening on port " + std::to_string(control_port));
    return Result<void>::success();
}

void Broker::setChaos(bool on) {
    static const std::string kChaosTag = "chaos-mode";
    fault_injector_.removeRulesByDescription(kChaosTag);
    if (on) {
        FaultRule r;
        r.type = FaultType::PacketDrop;
        r.affected_src = ANY_NODE;
        r.affected_dst = ANY_NODE;
        r.probability = 1.0f;
        r.drop_rate = 0.2f;          // 20% random drop across every link
        r.duration = INDEFINITE;
        r.description = kChaosTag;
        fault_injector_.addRule(std::move(r));
    }
}

void Broker::acceptControl() {
    auto res = control_listen_socket_.accept();
    if (!res) return;
    Socket client = std::move(*res);

    // Read one newline-terminated command line. poll() flagged the listener
    // readable and the controller writes a complete line immediately, so the
    // per-byte reads here resolve promptly. Bounded to avoid a runaway peer.
    std::string line;
    uint8_t c = 0;
    for (int i = 0; i < 256; ++i) {
        auto r = client.recvAll(&c, 1);
        if (!r) return;
        if (c == '\n') break;
        line.push_back(static_cast<char>(c));
    }

    std::string reply = applyControlCommand(line);
    client.sendAll(reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
}

std::string Broker::applyControlCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "link") {
        int a = -1, b = -1;
        std::string action;
        if (!(iss >> a >> b >> action) || a < 0 || a >= static_cast<int>(MAX_NODES) ||
            b < 0 || b >= static_cast<int>(MAX_NODES) || a == b) {
            return "ERR usage: link <a> <b> <loss%|down|up> (ids 0-" +
                   std::to_string(MAX_NODES - 1) + ", a!=b)\n";
        }
        LinkParams lp = link_matrix_[static_cast<size_t>(a)][static_cast<size_t>(b)];
        if (action == "down") {
            lp.link_up = false;
        } else if (action == "up") {
            lp.link_up = true;
        } else {
            // Numeric loss percent (accepts "20" or "20%").
            size_t pos = 0;
            float pct = 0.0f;
            try {
                pct = std::stof(action, &pos);
            } catch (...) {
                return "ERR bad loss value '" + action + "'\n";
            }
            lp.base_loss_rate = std::clamp(pct / 100.0f, 0.0f, 1.0f);
            lp.link_up = true;
        }
        link_matrix_[static_cast<size_t>(a)][static_cast<size_t>(b)] = lp;
        MT_INFO("broker", "Control: link " + std::to_string(a) + "->" + std::to_string(b) +
                          " " + action);
        return "OK link " + std::to_string(a) + "->" + std::to_string(b) + " " + action + "\n";
    }

    if (cmd == "get") {
        // Query one live directed link: reply "OK <up|down> <loss_fraction> <lat_ms>".
        int a = -1, b = -1;
        if (!(iss >> a >> b) || a < 0 || a >= static_cast<int>(MAX_NODES) ||
            b < 0 || b >= static_cast<int>(MAX_NODES)) {
            return "ERR usage: get <a> <b>\n";
        }
        const LinkParams& lp = link_matrix_[static_cast<size_t>(a)][static_cast<size_t>(b)];
        std::ostringstream out;
        out << "OK " << (lp.link_up ? "up" : "down") << " " << lp.base_loss_rate
            << " " << lp.latency_mean_ms << "\n";
        return out.str();
    }

    if (cmd == "chaos") {
        std::string state;
        iss >> state;
        if (state == "on") {
            setChaos(true);
            MT_INFO("broker", "Control: chaos ON (20% random drop, all links)");
            return "OK chaos on (20% random drop across all links)\n";
        }
        if (state == "off") {
            setChaos(false);
            MT_INFO("broker", "Control: chaos OFF");
            return "OK chaos off\n";
        }
        return "ERR usage: chaos <on|off>\n";
    }

    return "ERR unknown control command '" + cmd + "'\n";
}

void Broker::run() {
    MT_INFO("broker", "Waiting for " + std::to_string(MAX_NODES) + " node connections...");

    // Phase 1: Accept all node connections
    while (running_ && connected_count_ < MAX_NODES) {
        acceptConnections();
    }

    if (!running_) return;
    MT_INFO("broker", "All nodes connected. Forwarding frames.");

    // Phase 2: Forward frames between nodes
    // kinds[k] tags pollfd k: >=0 is a node index, -1 is the control listener.
    static constexpr int KIND_CONTROL = -1;
    while (running_) {
        std::vector<struct pollfd> fds;
        std::vector<int> kinds;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (node_connected_[i]) {
                fds.push_back({node_sockets_[i].fd(), POLLIN, 0});
                kinds.push_back(static_cast<int>(i));
            }
        }
        if (control_enabled_ && control_listen_socket_.valid()) {
            fds.push_back({control_listen_socket_.fd(), POLLIN, 0});
            kinds.push_back(KIND_CONTROL);
        }

        // Compute timeout based on delay queue
        int timeout_ms = 100; // Default poll timeout
        if (!delay_queue_.empty()) {
            auto now = SteadyClock::now();
            auto next = delay_queue_.top().deliver_at;
            if (next <= now) {
                timeout_ms = 0;
            } else {
                auto wait = std::chrono::duration_cast<Duration>(next - now);
                timeout_ms = std::min(static_cast<int>(wait.count()), timeout_ms);
            }
        }

        int ready = poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout_ms);

        // Process delayed frames
        deliverDelayed();

        if (ready <= 0) continue;

        // Dispatch ready fds
        for (size_t k = 0; k < fds.size(); ++k) {
            short re = fds[k].revents;
            if (!re) continue;
            int kind = kinds[k];
            if (kind == KIND_CONTROL) {
                if (re & POLLIN) acceptControl();
                continue;
            }
            size_t i = static_cast<size_t>(kind);
            if (re & POLLIN) {
                processIncoming(i);
            }
            if (re & (POLLHUP | POLLERR)) {
                MT_WARN("broker", "Node " + std::to_string(i) + " disconnected");
                node_sockets_[i].close();
                node_connected_[i] = false;
            }
        }
    }
}

void Broker::acceptConnections() {
    auto result = listen_socket_.accept();
    if (!result) return;

    Socket client = std::move(*result);

    // Read registration message: node sends its NodeId as a 2-byte value
    uint8_t reg_buf[2];
    auto recv_result = client.recvAll(reg_buf, 2);
    if (!recv_result) {
        MT_ERROR("broker", "Failed to read node registration");
        return;
    }

    NodeId node_id = static_cast<NodeId>(reg_buf[0]) | (static_cast<NodeId>(reg_buf[1]) << 8);
    if (node_id >= MAX_NODES) {
        MT_ERROR("broker", "Invalid node ID: " + std::to_string(node_id));
        return;
    }

    node_sockets_[node_id] = std::move(client);
    node_connected_[node_id] = true;
    ++connected_count_;

    MT_INFO("broker", "Node " + std::to_string(node_id) + " connected (" +
            std::to_string(connected_count_) + "/" + std::to_string(MAX_NODES) + ")");
}

void Broker::processIncoming(size_t node_idx) {
    // Read wire header
    uint8_t hdr_buf[WireHeader::SIZE];
    auto result = node_sockets_[node_idx].recvAll(hdr_buf, WireHeader::SIZE);
    if (!result) {
        MT_WARN("broker", "Node " + std::to_string(node_idx) + " read error: " + result.error().message);
        node_connected_[node_idx] = false;
        return;
    }

    WireHeader hdr = WireHeader::deserialize(hdr_buf);
    if (hdr.magic != WIRE_MAGIC) {
        MT_ERROR("broker", "Bad magic from node " + std::to_string(node_idx));
        return;
    }

    // Read payload
    std::vector<uint8_t> payload(hdr.payload_len);
    if (hdr.payload_len > 0) {
        result = node_sockets_[node_idx].recvAll(payload.data(), hdr.payload_len);
        if (!result) return;
    }

    // Deserialize MAC frame from payload
    MacFrame frame = MacFrame::deserialize(payload.data(), payload.size());

    // Forward
    forwardFrame(static_cast<NodeId>(node_idx), std::move(frame));
}

void Broker::forwardFrame(NodeId src, MacFrame frame) {
    auto now = SteadyClock::now();

    bool is_broadcast = (frame.dst_addr == INVALID_RLOC16 || frame.dst_addr == 0xFFFF);

    // Tracer-mode multi-hop relay (out-of-band measurement): for a resolvable
    // unicast, route along the real path and apply the per-link model at EACH
    // hop, so per-hop counters are route-accurate. Default mode keeps the
    // original single-hop behavior below (and is what every existing test sees).
    if (tracer_enabled_ && !is_broadcast) {
        NodeId final_dst = getRouterId(frame.dst_addr);
        if (final_dst < MAX_NODES && final_dst != src && node_connected_[final_dst]) {
            relayMultiHop(src, final_dst, std::move(frame), now);
            return;
        }
    }

    // Determine destinations
    std::vector<NodeId> destinations;
    if (is_broadcast) {
        // Broadcast
        for (NodeId i = 0; i < MAX_NODES; ++i) {
            if (i != src && node_connected_[i]) destinations.push_back(i);
        }
    } else {
        // Find the node matching this RLOC16 — for simplicity in 3-node sim,
        // we route by node ID derived from the destination header
        // In a more realistic sim, we'd look up RLOC16 -> NodeId mapping
        // For now, the WireHeader dst_node is used by the sending node
        for (NodeId i = 0; i < MAX_NODES; ++i) {
            if (i != src && node_connected_[i]) {
                destinations.push_back(i);
                break; // unicast to first match — refine with RLOC16 mapping later
            }
        }
    }

    for (NodeId dst : destinations) {
        MacFrame frame_copy = frame;

        // Apply link quality model
        const auto& params = link_matrix_[src][dst];
        auto channel_decision = channel_.evaluate(params);

        if (!channel_decision.deliver) {
            ++frames_dropped_;
            if (tracer_enabled_) ++link_stats_[src][dst].dropped;
            continue;
        }

        // Apply fault injection rules
        auto fault_decision = fault_injector_.applyFaults(src, dst, frame_copy, now);
        if (!fault_decision.deliver) {
            ++frames_dropped_;
            if (tracer_enabled_) ++link_stats_[src][dst].dropped;
            continue;
        }

        // Passed the link model — attribute the forward to this link (tracer only).
        // Global frames_forwarded_ is counted at actual delivery in deliverToNode;
        // this per-link tally counts link traversal, including delayed frames.
        if (tracer_enabled_) ++link_stats_[src][dst].forwarded;

        // Stamp LQI/RSSI
        frame_copy.lqi = fault_decision.delivered_lqi > 0 ?
            fault_decision.delivered_lqi : channel_decision.delivered_lqi;
        frame_copy.rssi = fault_decision.delivered_rssi != -100 ?
            fault_decision.delivered_rssi : channel_decision.delivered_rssi;

        Duration total_delay = channel_decision.delay + fault_decision.delay;

        if (total_delay.count() > 0) {
            delay_queue_.push({now + total_delay, dst, std::move(frame_copy)});
        } else {
            deliverToNode(dst, frame_copy);
        }
    }
}

std::vector<NodeId> Broker::route(NodeId src, NodeId dst) const {
    // Snapshot the link matrix into a MeshTopology and reuse the tested
    // least-hop path finder (what distance-vector converges to).
    MeshTopology topo;
    for (NodeId i = 0; i < MAX_NODES; ++i) {
        for (NodeId j = 0; j < MAX_NODES; ++j) {
            topo.setLinkParams(i, j, link_matrix_[i][j]);
        }
    }
    auto tr = tracePath(topo, src, dst);
    std::vector<NodeId> path;
    if (!tr.reachable) return path;
    path.push_back(src);
    for (const auto& h : tr.hops) path.push_back(h.to);
    return path;
}

void Broker::relayMultiHop(NodeId src, NodeId final_dst, MacFrame frame, TimePoint now) {
    auto path = route(src, final_dst);
    if (path.size() < 2) {
        // No route in the current topology — datagram is undeliverable.
        ++frames_dropped_;
        return;
    }

    Duration accumulated{0};
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        NodeId a = path[i];
        NodeId b = path[i + 1];

        const auto& params = link_matrix_[a][b];
        auto channel_decision = channel_.evaluate(params);
        if (!channel_decision.deliver) {
            ++frames_dropped_;
            ++link_stats_[a][b].dropped;  // tracer is enabled on this path
            return;                        // all-or-nothing: dies at this hop
        }

        auto fault_decision = fault_injector_.applyFaults(a, b, frame, now);
        if (!fault_decision.deliver) {
            ++frames_dropped_;
            ++link_stats_[a][b].dropped;
            return;
        }

        ++link_stats_[a][b].forwarded;
        accumulated += channel_decision.delay + fault_decision.delay;

        // Delivered frame reflects the last hop's measured link quality.
        frame.lqi = fault_decision.delivered_lqi > 0 ?
            fault_decision.delivered_lqi : channel_decision.delivered_lqi;
        frame.rssi = fault_decision.delivered_rssi != -100 ?
            fault_decision.delivered_rssi : channel_decision.delivered_rssi;
    }

    // Every hop survived — deliver to the final destination.
    if (accumulated.count() > 0) {
        delay_queue_.push({now + accumulated, final_dst, std::move(frame)});
    } else {
        deliverToNode(final_dst, frame);
    }
}

void Broker::deliverDelayed() {
    auto now = SteadyClock::now();
    while (!delay_queue_.empty() && delay_queue_.top().deliver_at <= now) {
        auto entry = std::move(const_cast<DelayedFrame&>(delay_queue_.top()));
        delay_queue_.pop();
        deliverToNode(entry.dst, entry.frame);
    }
}

void Broker::deliverToNode(NodeId dst, const MacFrame& frame) {
    if (dst >= MAX_NODES || !node_connected_[dst]) return;

    auto payload = frame.serialize();

    WireHeader hdr;
    hdr.src_node = frame.src_addr; // preserve original source info
    hdr.dst_node = dst;
    hdr.payload_len = static_cast<uint16_t>(payload.size());

    uint8_t hdr_buf[WireHeader::SIZE];
    hdr.serialize(hdr_buf);

    auto result = node_sockets_[dst].sendAll(hdr_buf, WireHeader::SIZE);
    if (!result) {
        MT_WARN("broker", "Failed to deliver to node " + std::to_string(dst));
        return;
    }
    if (!payload.empty()) {
        result = node_sockets_[dst].sendAll(payload.data(), payload.size());
        if (!result) {
            MT_WARN("broker", "Failed to deliver payload to node " + std::to_string(dst));
            return;
        }
    }
    ++frames_forwarded_;
}

} // namespace mt
