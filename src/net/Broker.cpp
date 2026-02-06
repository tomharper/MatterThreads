#include "net/Broker.h"
#include "core/Log.h"

#include <poll.h>
#include <algorithm>
#include <string>

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

void Broker::run() {
    MT_INFO("broker", "Waiting for " + std::to_string(MAX_NODES) + " node connections...");

    // Phase 1: Accept all node connections
    while (running_ && connected_count_ < MAX_NODES) {
        acceptConnections();
    }

    if (!running_) return;
    MT_INFO("broker", "All nodes connected. Forwarding frames.");

    // Phase 2: Forward frames between nodes
    while (running_) {
        // Build pollfd array for all connected nodes
        std::vector<struct pollfd> fds;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (node_connected_[i]) {
                fds.push_back({node_sockets_[i].fd(), POLLIN, 0});
            }
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

        // Process incoming frames
        size_t fd_idx = 0;
        for (size_t i = 0; i < MAX_NODES; ++i) {
            if (!node_connected_[i]) continue;
            if (fds[fd_idx].revents & POLLIN) {
                processIncoming(i);
            }
            if (fds[fd_idx].revents & (POLLHUP | POLLERR)) {
                MT_WARN("broker", "Node " + std::to_string(i) + " disconnected");
                node_sockets_[i].close();
                node_connected_[i] = false;
            }
            ++fd_idx;
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

    // Determine destinations
    std::vector<NodeId> destinations;
    if (frame.dst_addr == INVALID_RLOC16 || frame.dst_addr == 0xFFFF) {
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
            continue;
        }

        // Apply fault injection rules
        auto fault_decision = fault_injector_.applyFaults(src, dst, frame_copy, now);
        if (!fault_decision.deliver) {
            ++frames_dropped_;
            continue;
        }

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
