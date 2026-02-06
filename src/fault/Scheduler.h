#pragma once

#include "core/Types.h"
#include "net/FaultInjector.h"
#include "fault/FaultPlan.h"
#include <vector>

namespace mt {

class FaultScheduler {
    FaultInjector& injector_;
    FaultPlan plan_;
    TimePoint start_time_{};
    size_t next_step_ = 0;
    bool running_ = false;

public:
    explicit FaultScheduler(FaultInjector& injector) : injector_(injector) {}

    void loadPlan(const FaultPlan& plan);
    void start(TimePoint now);
    void stop();
    bool isRunning() const { return running_; }

    // Advance: apply any steps whose time has come
    void tick(TimePoint now);

    // Progress
    size_t completedSteps() const { return next_step_; }
    size_t totalSteps() const { return plan_.steps.size(); }
    bool isComplete() const { return next_step_ >= plan_.steps.size(); }
};

} // namespace mt
