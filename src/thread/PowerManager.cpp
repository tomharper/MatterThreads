#include "thread/PowerManager.h"
#include "core/Log.h"
#include <algorithm>

namespace mt {

void PowerManager::registerNode(NodeId node_id, ShutdownPriority priority) {
    // Don't double-register
    if (findNode(node_id)) return;

    NodePowerInfo info;
    info.node_id = node_id;
    info.priority = priority;
    info.state = (state_ == PowerState::EngineOn) ? NodePowerState::On : NodePowerState::Off;
    nodes_.push_back(info);
}

void PowerManager::setNodeShutdownCallback(NodeId node_id, NodeShutdownCallback cb) {
    shutdown_cbs_[node_id] = std::move(cb);
}

void PowerManager::setNodeBootCallback(NodeId node_id, NodeBootCallback cb) {
    boot_cbs_[node_id] = std::move(cb);
}

Result<void> PowerManager::initiateShutdown(TimePoint now, Duration battery_life) {
    if (state_ != PowerState::EngineOn) {
        return Result<void>(Error{1, "Cannot shut down: system not in EngineOn state"});
    }

    state_ = PowerState::ShuttingDown;
    shutdown_initiated_ = now;
    battery_life_ = battery_life;

    emit(PowerEvent::ShutdownInitiated, INVALID_NODE, now,
         "battery_life=" + std::to_string(battery_life.count()) + "ms");

    scheduleShutdownSequence(now, battery_life);
    return Result<void>::success();
}

Result<void> PowerManager::hardCutoff(TimePoint now) {
    if (state_ == PowerState::Off) {
        return Result<void>(Error{1, "System already off"});
    }

    // Any node not yet Off gets hard-cutoff
    for (auto& node : nodes_) {
        if (node.state != NodePowerState::Off) {
            node.state = NodePowerState::Off;
            node.shutdown_clean = false;
            hard_cutoffs_++;
            emit(PowerEvent::NodeHardCutoff, node.node_id, now,
                 "power_lost_during_" + std::string(nodePowerStateToString(node.state)));
        }
    }

    state_ = PowerState::Off;
    emit(PowerEvent::SystemOff, INVALID_NODE, now, "hard_cutoff");
    return Result<void>::success();
}

Result<void> PowerManager::initiateBoot(TimePoint now) {
    if (state_ != PowerState::Off) {
        return Result<void>(Error{1, "Cannot boot: system not in Off state"});
    }

    state_ = PowerState::Booting;
    emit(PowerEvent::BootInitiated, INVALID_NODE, now);

    scheduleBootSequence(now);
    return Result<void>::success();
}

void PowerManager::tick(TimePoint now) {
    if (state_ == PowerState::ShuttingDown) {
        // Check if battery has run out
        auto elapsed = std::chrono::duration_cast<Duration>(now - shutdown_initiated_);
        if (elapsed >= battery_life_) {
            hardCutoff(now);
            return;
        }

        // Process scheduled shutdowns
        bool all_off = true;
        for (auto& node : nodes_) {
            if (node.state == NodePowerState::On && now >= node.shutdown_at) {
                // Start this node's shutdown
                node.state = NodePowerState::ShuttingDown;
                emit(PowerEvent::NodeShutdownStarted, node.node_id, now);

                // Invoke callback
                auto it = shutdown_cbs_.find(node.node_id);
                if (it != shutdown_cbs_.end()) {
                    auto result = it->second(node.node_id, now);
                    node.shutdown_clean = result.ok();
                } else {
                    node.shutdown_clean = true; // No callback = nothing to clean up
                }

                node.state = NodePowerState::Off;
                graceful_shutdowns_++;
                emit(PowerEvent::NodeShutdownComplete, node.node_id, now,
                     node.shutdown_clean ? "clean" : "callback_failed");
            }

            if (node.state != NodePowerState::Off) {
                all_off = false;
            }
        }

        if (all_off) {
            state_ = PowerState::Off;
            emit(PowerEvent::SystemOff, INVALID_NODE, now, "graceful");
        }
    } else if (state_ == PowerState::Booting) {
        bool all_on = true;
        for (auto& node : nodes_) {
            if (node.state == NodePowerState::Off && now >= node.boot_at) {
                node.state = NodePowerState::Booting;
                emit(PowerEvent::NodeBootStarted, node.node_id, now);

                // Invoke callback
                auto it = boot_cbs_.find(node.node_id);
                if (it != boot_cbs_.end()) {
                    it->second(node.node_id, now);
                }

                node.state = NodePowerState::On;
                emit(PowerEvent::NodeBootComplete, node.node_id, now);
            }

            if (node.state != NodePowerState::On) {
                all_on = false;
            }
        }

        if (all_on) {
            state_ = PowerState::EngineOn;
            emit(PowerEvent::SystemOn, INVALID_NODE, now);
        }
    }
}

NodePowerState PowerManager::nodeState(NodeId node_id) const {
    auto* info = findNode(node_id);
    if (!info) return NodePowerState::Off;
    return info->state;
}

bool PowerManager::isNodeOperational(NodeId node_id) const {
    return nodeState(node_id) == NodePowerState::On;
}

Duration PowerManager::shutdownRemaining(TimePoint now) const {
    if (state_ != PowerState::ShuttingDown) return Duration(0);
    auto elapsed = std::chrono::duration_cast<Duration>(now - shutdown_initiated_);
    if (elapsed >= battery_life_) return Duration(0);
    return battery_life_ - elapsed;
}

void PowerManager::emit(PowerEvent event, NodeId node_id, TimePoint now,
                         const std::string& detail) {
    PowerEventRecord rec{now, event, node_id, detail};
    history_.push_back(rec);
    if (event_cb_) {
        event_cb_(rec);
    }
}

void PowerManager::scheduleShutdownSequence(TimePoint now, Duration battery_life) {
    // Sort nodes by priority (lowest = first to shut down)
    std::sort(nodes_.begin(), nodes_.end(), [](const NodePowerInfo& a, const NodePowerInfo& b) {
        return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
    });

    // Count distinct priority groups
    std::vector<uint8_t> priorities;
    for (const auto& n : nodes_) {
        auto p = static_cast<uint8_t>(n.priority);
        if (priorities.empty() || priorities.back() != p) {
            priorities.push_back(p);
        }
    }

    // Divide battery_life across priority groups with spacing
    // Reserve last 20% of battery for the highest-priority group (BR)
    size_t num_groups = priorities.size();
    if (num_groups == 0) return;

    Duration spacing = Duration(battery_life.count() / static_cast<long long>(num_groups + 1));

    // Schedule each node based on its priority group index
    for (auto& node : nodes_) {
        auto p = static_cast<uint8_t>(node.priority);
        size_t group_idx = 0;
        for (size_t i = 0; i < priorities.size(); ++i) {
            if (priorities[i] == p) { group_idx = i; break; }
        }
        node.shutdown_at = now + spacing * static_cast<long long>(group_idx + 1);
    }
}

void PowerManager::scheduleBootSequence(TimePoint now) {
    // Boot in reverse priority order: BR first, then relay, then sensors
    std::sort(nodes_.begin(), nodes_.end(), [](const NodePowerInfo& a, const NodePowerInfo& b) {
        return static_cast<uint8_t>(a.priority) > static_cast<uint8_t>(b.priority);
    });

    TimePoint boot_time = now;
    uint8_t last_priority = 255;
    for (auto& node : nodes_) {
        auto p = static_cast<uint8_t>(node.priority);
        if (p != last_priority) {
            boot_time = boot_time + boot_per_node_;
            last_priority = p;
        }
        node.boot_at = boot_time;
    }
}

NodePowerInfo* PowerManager::findNode(NodeId node_id) {
    for (auto& n : nodes_) {
        if (n.node_id == node_id) return &n;
    }
    return nullptr;
}

const NodePowerInfo* PowerManager::findNode(NodeId node_id) const {
    for (const auto& n : nodes_) {
        if (n.node_id == node_id) return &n;
    }
    return nullptr;
}

} // namespace mt
