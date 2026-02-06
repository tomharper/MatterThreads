#pragma once

#include "core/Types.h"
#include "thread/MeshTopology.h"
#include "fault/FaultPlan.h"
#include "metrics/Collector.h"
#include <string>
#include <functional>

namespace mt {

struct ScenarioConfig {
    MeshTopology topology;
    FaultPlan fault_plan;
    Duration max_duration = Duration(120000);
    uint32_t random_seed = 42;
};

struct ScenarioResult {
    bool passed = false;
    std::string name;
    Duration elapsed{0};
    std::string summary;
    Collector metrics;
};

struct ScenarioContext {
    Collector& metrics;
    TimePoint start_time;
    Duration elapsed;
};

class ScenarioRunner {
public:
    ScenarioResult run(const std::string& name, ScenarioConfig config,
                        std::function<bool(ScenarioContext&)> scenario_fn);
};

} // namespace mt
