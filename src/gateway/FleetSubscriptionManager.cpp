#include "gateway/FleetSubscriptionManager.h"
#include "core/Log.h"

namespace mt::gateway {

FleetSubscriptionManager::FleetSubscriptionManager(
    std::shared_ptr<hw::ChipToolDriver> driver,
    CASESessionPool& session_pool)
    : driver_(std::move(driver))
    , session_pool_(session_pool) {}

uint32_t FleetSubscriptionManager::addRule(SubscriptionRule rule) {
    rule.rule_id = next_rule_id_++;
    rules_.push_back(rule);
    return rule.rule_id;
}

void FleetSubscriptionManager::removeRule(uint32_t rule_id) {
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
                        [rule_id](const SubscriptionRule& r) { return r.rule_id == rule_id; }),
        rules_.end());
}

std::vector<SubscriptionRule> FleetSubscriptionManager::rules() const {
    return rules_;
}

Result<void> FleetSubscriptionManager::subscribeVan(const VanId& van_id, uint64_t device_id) {
    if (!session_pool_.isConnected(van_id)) {
        return Error{-1, "van not connected: " + van_id};
    }

    auto& subs = van_subs_[van_id];
    subs.clear();

    for (const auto& rule : rules_) {
        applyRule(van_id, device_id, rule);
    }

    MT_INFO("gateway", "Subscribed van " + van_id + " to " +
            std::to_string(rules_.size()) + " rules");
    return Result<void>::success();
}

void FleetSubscriptionManager::unsubscribeVan(const VanId& van_id) {
    auto it = van_subs_.find(van_id);
    if (it != van_subs_.end()) {
        for (auto& sub : it->second) {
            if (sub.active && sub.chip_tool_sub_id != 0) {
                driver_->cancelSubscription(sub.chip_tool_sub_id);
            }
        }
        van_subs_.erase(it);
    }
}

void FleetSubscriptionManager::onReport(FleetReportCallback cb) {
    observers_.push_back(std::move(cb));
}

void FleetSubscriptionManager::tick(TimePoint now) {
    checkLiveness(now);

    // Process any pending subscription reports via driver tick
    driver_->tick();
}

std::vector<VanSubscriptionState> FleetSubscriptionManager::getVanSubscriptions(
    const VanId& van_id) const {
    auto it = van_subs_.find(van_id);
    if (it != van_subs_.end()) return it->second;
    return {};
}

size_t FleetSubscriptionManager::activeSubscriptionCount() const {
    size_t count = 0;
    for (const auto& [van_id, subs] : van_subs_) {
        for (const auto& s : subs) {
            if (s.active) ++count;
        }
    }
    return count;
}

void FleetSubscriptionManager::loadDefaultVanRules() {
    // Based on delivery van endpoint design
    addRule({0, "cargo-temp",      2, 0x0402, 0x0000, Duration(5000),  Duration(30000), true});
    addRule({0, "cargo-humidity",  3, 0x0405, 0x0000, Duration(5000),  Duration(60000), false});
    addRule({0, "door-contact",    4, 0x0045, 0x0000, Duration(1000),  Duration(10000), true});
    addRule({0, "door-lock-state", 1, 0x0101, 0x0000, Duration(1000),  Duration(10000), true});
    addRule({0, "interior-light",  5, 0x0006, 0x0000, Duration(5000),  Duration(60000), false});
    addRule({0, "occupancy",       6, 0x0406, 0x0000, Duration(1000),  Duration(10000), true});
    addRule({0, "battery-voltage", 7, 0x002F, 0x000B, Duration(30000), Duration(300000), false});
    addRule({0, "reefer-setpoint", 8, 0x0201, 0x0012, Duration(5000),  Duration(60000), true});
}

void FleetSubscriptionManager::applyRule(const VanId& van_id, uint64_t device_id,
                                          const SubscriptionRule& rule) {
    VanSubscriptionState state;
    state.van_id = van_id;
    state.rule_id = rule.rule_id;

    auto result = driver_->subscribe(
        device_id, rule.endpoint, rule.cluster, rule.attribute,
        rule.min_interval, rule.max_interval,
        [this, van_id, rule](const AttributeValue& value) {
            VanAttributeReport report;
            report.van_id = van_id;
            report.timestamp = SteadyClock::now();
            report.path = {rule.endpoint, rule.cluster, rule.attribute};
            report.rule_id = rule.rule_id;
            // Convert hw::AttributeValue to mt::AttributeValue
            report.value = value;
            total_report_count_++;
            notifyObservers(report);
        });

    if (result.ok()) {
        state.chip_tool_sub_id = *result;
        state.active = true;
        state.last_report = SteadyClock::now();
    }

    van_subs_[van_id].push_back(state);
}

void FleetSubscriptionManager::notifyObservers(const VanAttributeReport& report) {
    for (const auto& cb : observers_) {
        cb(report);
    }
}

void FleetSubscriptionManager::checkLiveness(TimePoint now) {
    for (auto& [van_id, subs] : van_subs_) {
        for (auto& sub : subs) {
            if (!sub.active) continue;

            // Find the corresponding rule for max_interval
            Duration max_interval{60000}; // Default fallback
            for (const auto& rule : rules_) {
                if (rule.rule_id == sub.rule_id) {
                    max_interval = rule.max_interval;
                    break;
                }
            }

            // Check if we've missed too many report intervals
            if (now - sub.last_report > max_interval * 2) {
                sub.missed_reports++;
                if (sub.missed_reports >= VanSubscriptionState::MAX_MISSED) {
                    sub.active = false;
                    MT_WARN("gateway", "Subscription liveness lost for van " + van_id +
                            " rule " + std::to_string(sub.rule_id));
                }
            }
        }
    }
}

} // namespace mt::gateway
