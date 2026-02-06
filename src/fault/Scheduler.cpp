#include "fault/Scheduler.h"
#include "core/Log.h"

namespace mt {

void FaultScheduler::loadPlan(const FaultPlan& plan) {
    plan_ = plan;
    next_step_ = 0;
    MT_INFO("scheduler", "Loaded fault plan: " + plan.name +
            " (" + std::to_string(plan.steps.size()) + " steps)");
}

void FaultScheduler::start(TimePoint now) {
    start_time_ = now;
    next_step_ = 0;
    running_ = true;
    MT_INFO("scheduler", "Started fault plan: " + plan_.name);
}

void FaultScheduler::stop() {
    running_ = false;
    injector_.clearRules();
    MT_INFO("scheduler", "Stopped fault plan");
}

void FaultScheduler::tick(TimePoint now) {
    if (!running_) return;

    auto elapsed = now - start_time_;

    while (next_step_ < plan_.steps.size()) {
        const auto& step = plan_.steps[next_step_];
        if (elapsed >= step.delay_from_start) {
            FaultRule rule = step.rule;
            rule.start_time = now;
            injector_.addRule(rule);
            MT_INFO("scheduler", "Activated step " + std::to_string(next_step_) +
                    ": " + step.description);
            ++next_step_;
        } else {
            break;
        }
    }

    // Purge expired rules
    injector_.purgeExpired(now);

    // Check if complete
    if (next_step_ >= plan_.steps.size()) {
        // Plan fully deployed — keep running until all rules expire
        if (injector_.rules().empty()) {
            running_ = false;
            MT_INFO("scheduler", "Fault plan complete");
        }
    }
}

} // namespace mt
