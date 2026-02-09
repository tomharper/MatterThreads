#pragma once

#include "gateway/GatewayTypes.h"
#include "gateway/SessionPool.h"
#include "core/Result.h"
#include "hw/ChipToolDriver.h"
#include "matter/DataModel.h"

#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

namespace mt::gateway {

struct SubscriptionRule {
    uint32_t rule_id = 0;
    std::string name;
    EndpointId endpoint = 0;
    ClusterId cluster = 0;
    AttributeId attribute = 0;
    Duration min_interval{5000};
    Duration max_interval{30000};
    bool critical = false;
};

struct VanAttributeReport {
    VanId van_id;
    TimePoint timestamp{};
    AttributePath path;
    AttributeValue value;
    uint32_t rule_id = 0;
};

using FleetReportCallback = std::function<void(const VanAttributeReport&)>;

struct VanSubscriptionState {
    VanId van_id;
    uint32_t rule_id = 0;
    SubscriptionId chip_tool_sub_id = 0;
    bool active = false;
    TimePoint last_report{};
    uint32_t report_count = 0;
    uint8_t missed_reports = 0;
    static constexpr uint8_t MAX_MISSED = 3;
};

class FleetSubscriptionManager {
public:
    FleetSubscriptionManager(std::shared_ptr<hw::ChipToolDriver> driver,
                              CASESessionPool& session_pool);

    uint32_t addRule(SubscriptionRule rule);
    void removeRule(uint32_t rule_id);
    std::vector<SubscriptionRule> rules() const;

    Result<void> subscribeVan(const VanId& van_id, uint64_t device_id);
    void unsubscribeVan(const VanId& van_id);

    void onReport(FleetReportCallback cb);

    void tick(TimePoint now);

    std::vector<VanSubscriptionState> getVanSubscriptions(const VanId& van_id) const;
    size_t activeSubscriptionCount() const;
    size_t totalReportCount() const { return total_report_count_; }

    void loadDefaultVanRules();

private:
    std::shared_ptr<hw::ChipToolDriver> driver_;
    CASESessionPool& session_pool_;

    std::vector<SubscriptionRule> rules_;
    uint32_t next_rule_id_ = 1;

    std::unordered_map<VanId, std::vector<VanSubscriptionState>> van_subs_;
    std::vector<FleetReportCallback> observers_;
    size_t total_report_count_ = 0;

    void applyRule(const VanId& van_id, uint64_t device_id, const SubscriptionRule& rule);
    void notifyObservers(const VanAttributeReport& report);
    void checkLiveness(TimePoint now);
};

} // namespace mt::gateway
