#pragma once

#include "core/Types.h"
#include "core/Random.h"
#include "net/FaultInjector.h"
#include <vector>

namespace mt {

struct ChaosConfig {
    float fault_probability_per_second = 0.1f;
    Duration min_fault_duration = Duration(1000);
    Duration max_fault_duration = Duration(30000);
    std::vector<FaultType> allowed_faults = {
        FaultType::PacketDrop,
        FaultType::LatencySpike,
        FaultType::Reorder,
        FaultType::LinkDegrade
    };
};

class ChaosEngine {
    Random& rng_;
    FaultInjector& injector_;
    ChaosConfig config_;
    bool running_ = false;
    TimePoint last_tick_{};

public:
    ChaosEngine(Random& rng, FaultInjector& injector)
        : rng_(rng), injector_(injector) {}

    void start(ChaosConfig config);
    void stop();
    bool isRunning() const { return running_; }

    void tick(TimePoint now);
};

} // namespace mt
