#include "matter/SubscriptionManager.h"
#include "core/Log.h"
#include <algorithm>
#include <string>

namespace mt {

SubscriptionId SubscriptionManager::createSubscription(SessionId session_id, NodeId peer_node_id,
                                                         const std::vector<AttributePath>& paths,
                                                         Duration min_interval, Duration max_interval) {
    Subscription sub;
    sub.subscription_id = next_id_++;
    sub.session_id = session_id;
    sub.peer_node_id = peer_node_id;
    sub.paths = paths;
    sub.min_interval = min_interval;
    sub.max_interval = max_interval;
    sub.active = true;

    auto id = sub.subscription_id;
    subs_.push_back(std::move(sub));

    MT_INFO("subscribe", "Created subscription " + std::to_string(id) +
            " for " + std::to_string(paths.size()) + " paths, interval=[" +
            std::to_string(min_interval.count()) + "ms, " +
            std::to_string(max_interval.count()) + "ms]");

    return id;
}

void SubscriptionManager::cancelSubscription(SubscriptionId id) {
    for (auto& sub : subs_) {
        if (sub.subscription_id == id) {
            sub.active = false;
            MT_INFO("subscribe", "Cancelled subscription " + std::to_string(id));
            return;
        }
    }
}

void SubscriptionManager::cancelAllForSession(SessionId session_id) {
    for (auto& sub : subs_) {
        if (sub.session_id == session_id && sub.active) {
            sub.active = false;
        }
    }
}

void SubscriptionManager::onDataChanged(const AttributePath& path, TimePoint now) {
    for (auto& sub : subs_) {
        if (!sub.active) continue;

        bool covers = false;
        for (const auto& sub_path : sub.paths) {
            if (sub_path == path) {
                covers = true;
                break;
            }
        }

        if (!covers) continue;

        // Check min interval
        if (sub.last_report_time != TimePoint{} &&
            (now - sub.last_report_time) < sub.min_interval) {
            continue; // Too soon, defer
        }

        // Send report
        sub.last_report_time = now;
        ++sub.report_count;

        if (report_callback_) {
            report_callback_(sub.subscription_id, sub.paths);
        }
    }
}

void SubscriptionManager::tick(TimePoint now) {
    for (auto& sub : subs_) {
        if (!sub.active) continue;

        // Send periodic report at max_interval
        if (sub.last_report_time == TimePoint{} ||
            (now - sub.last_report_time) >= sub.max_interval) {
            sub.last_report_time = now;
            ++sub.report_count;

            if (report_callback_) {
                report_callback_(sub.subscription_id, sub.paths);
            }
        }

        // Check liveness
        if (sub.last_liveness_check != TimePoint{} &&
            (now - sub.last_liveness_check) > sub.max_interval * 2) {
            ++sub.missed_liveness_count;
            sub.last_liveness_check = now;

            if (sub.missed_liveness_count >= Subscription::MAX_MISSED_LIVENESS) {
                MT_WARN("subscribe", "Subscription " + std::to_string(sub.subscription_id) +
                        " dropped (missed " + std::to_string(sub.missed_liveness_count) +
                        " liveness checks)");
                sub.active = false;
            }
        }
    }
}

void SubscriptionManager::onLivenessCheckReceived(SubscriptionId id, TimePoint now) {
    for (auto& sub : subs_) {
        if (sub.subscription_id == id && sub.active) {
            sub.missed_liveness_count = 0;
            sub.last_liveness_check = now;
            return;
        }
    }
}

void SubscriptionManager::onLivenessCheckFailed(SubscriptionId id) {
    for (auto& sub : subs_) {
        if (sub.subscription_id == id && sub.active) {
            ++sub.missed_liveness_count;
            if (sub.missed_liveness_count >= Subscription::MAX_MISSED_LIVENESS) {
                sub.active = false;
            }
            return;
        }
    }
}

const Subscription* SubscriptionManager::findSubscription(SubscriptionId id) const {
    for (const auto& sub : subs_) {
        if (sub.subscription_id == id) return &sub;
    }
    return nullptr;
}

std::vector<Subscription> SubscriptionManager::getActiveSubscriptions() const {
    std::vector<Subscription> active;
    for (const auto& sub : subs_) {
        if (sub.active) active.push_back(sub);
    }
    return active;
}

size_t SubscriptionManager::activeCount() const {
    size_t count = 0;
    for (const auto& sub : subs_) {
        if (sub.active) ++count;
    }
    return count;
}

} // namespace mt
