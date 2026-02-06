#include "thread/ThreadNode.h"
#include "core/Log.h"
#include <string>

namespace mt {

ThreadNode::ThreadNode(NodeId id, DeviceMode mode, uint64_t ext_addr)
    : id_(id), mode_(mode), ext_addr_(ext_addr) {}

void ThreadNode::attach() {
    if (role_ != DeviceRole::Detached) return;
    role_ = (mode_ == DeviceMode::FTD) ? DeviceRole::REED : DeviceRole::EndDevice;
    MT_INFO("thread", "Node " + std::to_string(id_) + " attached as " + roleToString(role_));
}

void ThreadNode::detach() {
    role_ = DeviceRole::Detached;
    rloc16_ = INVALID_RLOC16;
    router_id_ = INVALID_ROUTER_ID;
    MT_INFO("thread", "Node " + std::to_string(id_) + " detached");
}

void ThreadNode::promoteToRouter(uint8_t assigned_router_id) {
    if (mode_ != DeviceMode::FTD) return;
    router_id_ = assigned_router_id;
    rloc16_ = makeRLOC16(router_id_, 0);
    role_ = DeviceRole::Router;
    routing_.setOwnRouterId(router_id_);
    MT_INFO("thread", "Node " + std::to_string(id_) + " promoted to Router (ID=" +
            std::to_string(router_id_) + ", RLOC16=0x" +
            std::to_string(rloc16_) + ")");
}

void ThreadNode::becomeLeader() {
    if (mode_ != DeviceMode::FTD) return;

    // Self-assign router ID 0
    router_id_ = 0;
    rloc16_ = makeRLOC16(0, 0);
    role_ = DeviceRole::Leader;
    routing_.setOwnRouterId(0);
    leader_data_.setLeaderRouterId(0);

    // Register self in leader's router ID table
    leader_data_.assignRouterId(); // Gets ID 0

    MT_INFO("thread", "Node " + std::to_string(id_) + " became Leader (RLOC16=0x0000)");
}

void ThreadNode::tick() {
    if (role_ == DeviceRole::Detached) return;

    TimePoint now = clock_ ? clock_->now() : SteadyClock::now();

    // Send MLE advertisement if it's time (routers and leader only)
    if ((role_ == DeviceRole::Router || role_ == DeviceRole::Leader) &&
        mle_.shouldAdvertise(now)) {
        sendMLEAdvertisement();
        mle_.markAdvertised(now);
    }

    // Expire stale routes
    routing_.expireStaleRoutes(now, neighbor_timeout_);

    // Expire stale children
    if (role_ == DeviceRole::Router || role_ == DeviceRole::Leader) {
        child_table_.expireStale(now, neighbor_timeout_ * 4);
    }
}

void ThreadNode::onFrameReceived(const MacFrame& frame) {
    // Route based on frame content type marker in payload
    if (!frame.payload.empty() && frame.payload[0] == 0x01) {
        // MLE frame (type marker 0x01)
        processMLEFrame(frame);
    } else {
        processDataFrame(frame);
    }
}

void ThreadNode::processMLEFrame(const MacFrame& frame) {
    auto adv = MLEEngine::deserializeAdvertisement(frame.payload.data(), frame.payload.size());

    TimePoint now = clock_ ? clock_->now() : SteadyClock::now();

    // Update routing table from the advertisement
    routing_.addDirectNeighbor(adv.source_router_id, now);
    routing_.updateFromAdvertisement(adv.source_router_id, adv.route_data, now);

    MT_TRACE("thread", "Node " + std::to_string(id_) + " received MLE from router " +
             std::to_string(adv.source_router_id) + " (LQI=" + std::to_string(frame.lqi) + ")");
}

void ThreadNode::processDataFrame(const MacFrame& frame) {
    // Check if this frame is destined for us or needs forwarding
    if (frame.dst_addr == rloc16_ || frame.dst_addr == 0xFFFF) {
        MT_DEBUG("thread", "Node " + std::to_string(id_) + " received data frame from RLOC16=0x" +
                 std::to_string(frame.src_addr) + " (" + std::to_string(frame.payload.size()) + " bytes)");
        // TODO: pass up to Matter layer
    } else if (role_ == DeviceRole::Router || role_ == DeviceRole::Leader) {
        // Forward: look up next hop
        uint8_t dst_router_id = getRouterId(frame.dst_addr);
        auto next_hop = routing_.getNextHop(dst_router_id);
        if (next_hop) {
            MacFrame fwd = frame;
            fwd.src_addr = rloc16_;
            sendData(makeRLOC16(*next_hop, 0), fwd.payload);
        } else {
            MT_WARN("thread", "Node " + std::to_string(id_) + " no route to RLOC16=0x" +
                    std::to_string(frame.dst_addr));
        }
    }
}

void ThreadNode::sendMLEAdvertisement() {
    auto adv = mle_.buildAdvertisement(router_id_, rloc16_, ext_addr_, routing_);
    auto payload = MLEEngine::serializeAdvertisement(adv);

    MacFrame frame;
    frame.type = MacFrame::Type::Data;
    frame.seq_number = seq_counter_++;
    frame.src_addr = rloc16_;
    frame.dst_addr = 0xFFFF; // Broadcast
    frame.payload = std::move(payload);

    if (send_frame_) {
        send_frame_(frame);
    }
}

void ThreadNode::sendData(RLOC16 dst, const std::vector<uint8_t>& payload) {
    MacFrame frame;
    frame.type = MacFrame::Type::Data;
    frame.seq_number = seq_counter_++;
    frame.src_addr = rloc16_;
    frame.dst_addr = dst;
    frame.payload = payload;

    if (send_frame_) {
        send_frame_(frame);
    }
}

std::string ThreadNode::statusString() const {
    std::string s = "Node " + std::to_string(id_) + ": role=" + roleToString(role_);
    if (rloc16_ != INVALID_RLOC16) {
        s += " rloc16=0x" + std::to_string(rloc16_);
    }
    if (router_id_ != INVALID_ROUTER_ID) {
        s += " router_id=" + std::to_string(router_id_);
    }
    s += " routes=" + std::to_string(routing_.reachableCount());
    if (role_ == DeviceRole::Router || role_ == DeviceRole::Leader) {
        s += " children=" + std::to_string(child_table_.size());
    }
    return s;
}

} // namespace mt
