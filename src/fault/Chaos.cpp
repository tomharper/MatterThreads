#include "fault/Chaos.h"
#include "core/Log.h"
#include <string>

namespace mt {

void ChaosEngine::start(ChaosConfig config) {
    config_ = std::move(config);
    running_ = true;
    MT_INFO("chaos", "Chaos mode enabled (p=" +
            std::to_string(config_.fault_probability_per_second) + "/s)");
}

void ChaosEngine::stop() {
    running_ = false;
    injector_.clearRules();
    MT_INFO("chaos", "Chaos mode disabled");
}

void ChaosEngine::tick(TimePoint now) {
    if (!running_) return;

    if (last_tick_ == TimePoint{}) {
        last_tick_ = now;
        return;
    }

    auto dt = std::chrono::duration_cast<Duration>(now - last_tick_);
    last_tick_ = now;

    // Probability scaled by time elapsed
    double p = static_cast<double>(config_.fault_probability_per_second) *
               (static_cast<double>(dt.count()) / 1000.0);

    if (!rng_.chance(p)) return;
    if (config_.allowed_faults.empty()) return;

    // Pick a random fault type
    int idx = rng_.nextInt(0, static_cast<int>(config_.allowed_faults.size()) - 1);
    FaultType type = config_.allowed_faults[static_cast<size_t>(idx)];

    // Pick random source and destination (0-2)
    auto src = static_cast<NodeId>(rng_.nextInt(0, 2));
    auto dst = static_cast<NodeId>(rng_.nextInt(0, 2));
    if (src == dst) dst = static_cast<NodeId>((dst + 1) % 3);

    // Random duration within configured range
    int dur_ms = rng_.nextInt(
        static_cast<int>(config_.min_fault_duration.count()),
        static_cast<int>(config_.max_fault_duration.count()));

    FaultRule rule;
    rule.type = type;
    rule.affected_src = src;
    rule.affected_dst = dst;
    rule.duration = Duration(dur_ms);
    rule.start_time = now;
    rule.probability = 1.0f;

    // Set type-specific defaults
    switch (type) {
        case FaultType::PacketDrop:
            rule.drop_rate = static_cast<float>(rng_.nextInt(10, 80)) / 100.0f;
            break;
        case FaultType::LatencySpike:
            rule.extra_latency = Duration(rng_.nextInt(50, 500));
            break;
        default:
            break;
    }

    injector_.addRule(rule);
    MT_INFO("chaos", "Injected fault: src=" + std::to_string(src) +
            " dst=" + std::to_string(dst) + " duration=" + std::to_string(dur_ms) + "ms");

    // Purge expired
    injector_.purgeExpired(now);
}

} // namespace mt
