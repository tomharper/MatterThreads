#pragma once

#include "core/Types.h"
#include "matter/DataModel.h"
#include <vector>
#include <functional>

namespace mt {

struct Subscription {
    SubscriptionId subscription_id;
    SessionId session_id;
    NodeId peer_node_id;
    std::vector<AttributePath> paths;
    Duration min_interval;
    Duration max_interval;
    TimePoint last_report_time{};
    uint32_t report_count = 0;
    bool active = true;

    // Liveness tracking
    TimePoint last_liveness_check{};
    uint8_t missed_liveness_count = 0;
    static constexpr uint8_t MAX_MISSED_LIVENESS = 3;
};

using ReportCallback = std::function<void(SubscriptionId, const std::vector<AttributePath>&)>;

class SubscriptionManager {
    std::vector<Subscription> subs_;
    SubscriptionId next_id_ = 1;
    ReportCallback report_callback_;

public:
    void setReportCallback(ReportCallback cb) { report_callback_ = std::move(cb); }

    SubscriptionId createSubscription(SessionId session_id, NodeId peer_node_id,
                                       const std::vector<AttributePath>& paths,
                                       Duration min_interval, Duration max_interval);

    void cancelSubscription(SubscriptionId id);
    void cancelAllForSession(SessionId session_id);

    // Called when an attribute changes — triggers reports if any subscription covers it
    void onDataChanged(const AttributePath& path, TimePoint now);

    // Periodic tick: send max-interval reports, check liveness
    void tick(TimePoint now);

    // Mark liveness check as received
    void onLivenessCheckReceived(SubscriptionId id, TimePoint now);

    // Mark liveness check as failed
    void onLivenessCheckFailed(SubscriptionId id);

    const Subscription* findSubscription(SubscriptionId id) const;
    std::vector<Subscription> getActiveSubscriptions() const;
    size_t activeCount() const;
};

} // namespace mt
