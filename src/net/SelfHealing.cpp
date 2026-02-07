#include "net/SelfHealing.h"
#include "core/Log.h"
#include <algorithm>

namespace mt {

void SelfHealingEngine::tick(TimePoint now, const RoutingTable& routing) {
    checkNeighborLiveness(now);
    checkPartitions(routing, now);
}

void SelfHealingEngine::onNeighborHeard(NodeId node_id, uint8_t router_id, TimePoint now) {
    auto it = neighbors_.find(node_id);
    if (it == neighbors_.end()) {
        // New neighbor
        neighbors_[node_id] = NeighborState{node_id, router_id, now, true, 0};
        return;
    }

    auto& state = it->second;
    bool was_unreachable = !state.reachable;
    state.last_heard = now;
    state.missed_advertisements = 0;
    state.reachable = true;

    if (was_unreachable) {
        // Neighbor came back!
        Duration recovery = std::chrono::duration_cast<Duration>(now - state.last_heard);
        emit(HealingEvent::NodeReattached, node_id,
             "Router " + std::to_string(router_id) + " reachable again",
             recovery);
    }
}

void SelfHealingEngine::onNodeReattached(NodeId node_id, TimePoint now) {
    auto it = neighbors_.find(node_id);
    if (it != neighbors_.end()) {
        it->second.reachable = true;
        it->second.last_heard = now;
        it->second.missed_advertisements = 0;
    }
    emit(HealingEvent::NodeReattached, node_id, "Node re-attached to mesh");
}

void SelfHealingEngine::onSubscriptionDropped(NodeId remote_node, SubscriptionId sub_id,
                                               TimePoint now) {
    emit(HealingEvent::SubscriptionDropped, remote_node,
         "Subscription " + std::to_string(sub_id) + " dropped");
    (void)now;
}

void SelfHealingEngine::onSubscriptionRecovered(NodeId remote_node, SubscriptionId sub_id,
                                                  TimePoint now) {
    emit(HealingEvent::SubscriptionRecovered, remote_node,
         "Subscription " + std::to_string(sub_id) + " recovered");
    (void)now;
}

void SelfHealingEngine::onBackhaulLost(TimePoint now) {
    backhaul_.markDisconnected(now);
    emit(HealingEvent::BackhaulLost, BROADCAST_NODE,
         "Cellular/Wi-Fi backhaul disconnected");
}

void SelfHealingEngine::onBackhaulRestored(TimePoint now) {
    Duration downtime = backhaul_.downtime(now);
    backhaul_.markConnected(now);
    emit(HealingEvent::BackhaulRestored, BROADCAST_NODE,
         "Backhaul restored after " + std::to_string(downtime.count()) + "ms, " +
         std::to_string(backhaul_.buffered_messages) + " buffered messages to drain",
         downtime);
    backhaul_.buffered_messages = 0;
}

bool SelfHealingEngine::isNeighborReachable(NodeId node_id) const {
    auto it = neighbors_.find(node_id);
    if (it == neighbors_.end()) return false;
    return it->second.reachable;
}

std::vector<NodeId> SelfHealingEngine::unreachableNeighbors() const {
    std::vector<NodeId> result;
    for (const auto& [id, state] : neighbors_) {
        if (!state.reachable) {
            result.push_back(id);
        }
    }
    return result;
}

void SelfHealingEngine::emit(HealingEvent event, NodeId node, const std::string& detail,
                              Duration recovery) {
    auto now = SteadyClock::now();
    HealingRecord record{now, event, node, detail, recovery};
    history_.push_back(record);

    MT_INFO("healing", std::string(healingEventToString(event)) + " node=" +
            std::to_string(node) + " " + detail);

    if (callback_) {
        callback_(record);
    }
}

void SelfHealingEngine::checkNeighborLiveness(TimePoint now) {
    for (auto& [id, state] : neighbors_) {
        if (!state.reachable) continue;

        auto elapsed = std::chrono::duration_cast<Duration>(now - state.last_heard);
        if (elapsed > neighbor_timeout_) {
            state.reachable = false;
            state.missed_advertisements++;
            emit(HealingEvent::NeighborLost, id,
                 "Router " + std::to_string(state.router_id) +
                 " missed " + std::to_string(state.missed_advertisements) +
                 " advertisements (timeout=" + std::to_string(neighbor_timeout_.count()) + "ms)");
        }
    }
}

void SelfHealingEngine::checkPartitions(const RoutingTable& routing, TimePoint now) {
    // Build current reachable router set
    std::vector<uint8_t> current_reachable;
    for (uint8_t i = 0; i < MAX_ROUTERS; ++i) {
        const auto& entry = routing.getEntry(i);
        if (entry.reachable && entry.router_id != INVALID_ROUTER_ID) {
            current_reachable.push_back(i);
        }
    }

    // Check if any previously reachable router is now gone
    for (uint8_t prev_id : prev_reachable_routers_) {
        auto it = std::find(current_reachable.begin(), current_reachable.end(), prev_id);
        if (it == current_reachable.end()) {
            // Router was reachable, now isn't — possible partition
            ++partitions_detected_;
            emit(HealingEvent::PartitionDetected, prev_id,
                 "Router " + std::to_string(prev_id) + " no longer reachable "
                 "(partition #" + std::to_string(partitions_detected_) + ")");
        }
    }

    // Check if any previously unreachable router is now reachable (healed)
    for (uint8_t cur_id : current_reachable) {
        auto it = std::find(prev_reachable_routers_.begin(), prev_reachable_routers_.end(), cur_id);
        if (it == prev_reachable_routers_.end() && !prev_reachable_routers_.empty()) {
            emit(HealingEvent::RouteRecalculated, cur_id,
                 "Router " + std::to_string(cur_id) + " reachable again via new route");
        }
    }

    prev_reachable_routers_ = current_reachable;
    (void)now;
}

} // namespace mt
