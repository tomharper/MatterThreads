#include "fault/FaultPlan.h"
#include <nlohmann/json.hpp>

namespace mt {

using json = nlohmann::json;

static FaultType faultTypeFromString(const std::string& s) {
    if (s == "packet_drop") return FaultType::PacketDrop;
    if (s == "latency_spike") return FaultType::LatencySpike;
    if (s == "reorder") return FaultType::Reorder;
    if (s == "corrupt") return FaultType::Corrupt;
    if (s == "duplicate") return FaultType::Duplicate;
    if (s == "link_down") return FaultType::LinkDown;
    if (s == "link_degrade") return FaultType::LinkDegrade;
    if (s == "node_crash") return FaultType::NodeCrash;
    if (s == "node_freeze") return FaultType::NodeFreeze;
    if (s == "partial_partition") return FaultType::PartialPartition;
    return FaultType::PacketDrop;
}

static std::string faultTypeToString(FaultType t) {
    switch (t) {
        case FaultType::PacketDrop: return "packet_drop";
        case FaultType::LatencySpike: return "latency_spike";
        case FaultType::Reorder: return "reorder";
        case FaultType::Corrupt: return "corrupt";
        case FaultType::Duplicate: return "duplicate";
        case FaultType::LinkDown: return "link_down";
        case FaultType::LinkDegrade: return "link_degrade";
        case FaultType::NodeCrash: return "node_crash";
        case FaultType::NodeFreeze: return "node_freeze";
        case FaultType::PartialPartition: return "partial_partition";
    }
    return "unknown";
}

FaultPlan FaultPlan::fromJson(const std::string& json_str) {
    FaultPlan plan;
    auto j = json::parse(json_str);
    plan.name = j.value("name", "");
    plan.description = j.value("description", "");

    for (const auto& step_j : j["steps"]) {
        Step step;
        step.delay_from_start = Duration(step_j.value("delay_ms", 0));
        step.description = step_j.value("description", "");

        auto& rule_j = step_j["rule"];
        step.rule.type = faultTypeFromString(rule_j.value("type", "packet_drop"));
        step.rule.affected_src = rule_j.value("src", static_cast<int>(ANY_NODE));
        step.rule.affected_dst = rule_j.value("dst", static_cast<int>(ANY_NODE));
        step.rule.probability = rule_j.value("probability", 1.0f);
        step.rule.drop_rate = rule_j.value("drop_rate", 0.5f);
        step.rule.extra_latency = Duration(rule_j.value("extra_latency_ms", 100));

        int dur = rule_j.value("duration_ms", -1);
        step.rule.duration = (dur < 0) ? INDEFINITE : Duration(dur);

        plan.steps.push_back(std::move(step));
    }

    return plan;
}

std::string FaultPlan::toJson() const {
    json j;
    j["name"] = name;
    j["description"] = description;
    j["steps"] = json::array();

    for (const auto& step : steps) {
        json step_j;
        step_j["delay_ms"] = step.delay_from_start.count();
        step_j["description"] = step.description;
        step_j["rule"]["type"] = faultTypeToString(step.rule.type);
        step_j["rule"]["src"] = step.rule.affected_src;
        step_j["rule"]["dst"] = step.rule.affected_dst;
        step_j["rule"]["probability"] = step.rule.probability;
        step_j["rule"]["drop_rate"] = step.rule.drop_rate;
        step_j["rule"]["extra_latency_ms"] = step.rule.extra_latency.count();
        if (step.rule.duration != INDEFINITE) {
            step_j["rule"]["duration_ms"] = step.rule.duration.count();
        }
        j["steps"].push_back(step_j);
    }

    return j.dump(2);
}

FaultPlan FaultPlan::meshHealingTest() {
    FaultPlan plan;
    plan.name = "mesh-healing";
    plan.description = "Kill middle router in linear chain, measure mesh healing time";

    FaultRule crash;
    crash.type = FaultType::NodeCrash;
    crash.affected_src = 1;
    crash.description = "Crash Node 1 (middle router)";

    plan.steps.push_back({Duration(30000), crash, "Kill Node 1 at T=30s"});
    return plan;
}

FaultPlan FaultPlan::subscriptionStress() {
    FaultPlan plan;
    plan.name = "subscription-stress";
    plan.description = "Drop link carrying subscription traffic, then restore";

    FaultRule drop;
    drop.type = FaultType::LinkDown;
    drop.affected_src = 0;
    drop.affected_dst = 2;
    drop.duration = Duration(90000);

    FaultRule drop_rev;
    drop_rev.type = FaultType::LinkDown;
    drop_rev.affected_src = 2;
    drop_rev.affected_dst = 0;
    drop_rev.duration = Duration(90000);

    plan.steps.push_back({Duration(30000), drop, "Link 0->2 down at T=30s for 90s"});
    plan.steps.push_back({Duration(30000), drop_rev, "Link 2->0 down at T=30s for 90s"});
    return plan;
}

FaultPlan FaultPlan::commissioningFlaky() {
    FaultPlan plan;
    plan.name = "commissioning-flaky";
    plan.description = "Inject increasing packet loss during commissioning attempts";

    for (int loss_pct : {10, 20, 30, 50}) {
        FaultRule drop;
        drop.type = FaultType::PacketDrop;
        drop.affected_src = ANY_NODE;
        drop.affected_dst = ANY_NODE;
        drop.drop_rate = static_cast<float>(loss_pct) / 100.0f;
        drop.duration = Duration(30000);

        plan.steps.push_back({
            Duration(static_cast<int64_t>(loss_pct) * 1000),
            drop,
            std::to_string(loss_pct) + "% packet loss"
        });
    }

    return plan;
}

FaultPlan FaultPlan::progressiveDegradation() {
    FaultPlan plan;
    plan.name = "progressive-degradation";
    plan.description = "Gradually degrade link quality over time";

    for (int i = 1; i <= 5; ++i) {
        FaultRule degrade;
        degrade.type = FaultType::LinkDegrade;
        degrade.affected_src = 0;
        degrade.affected_dst = 2;
        degrade.probability = 1.0f;
        degrade.duration = Duration(20000);

        plan.steps.push_back({
            Duration(static_cast<int64_t>(i) * 20000),
            degrade,
            "Degrade phase " + std::to_string(i)
        });
    }

    return plan;
}

} // namespace mt
