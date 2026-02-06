#include "ScenarioRunner.h"
#include "core/Log.h"

namespace mt {

ScenarioResult ScenarioRunner::run(const std::string& name, ScenarioConfig config,
                                     std::function<bool(ScenarioContext&)> scenario_fn) {
    ScenarioResult result;
    result.name = name;

    auto start = SteadyClock::now();

    ScenarioContext ctx{
        .metrics = result.metrics,
        .start_time = start,
        .elapsed = Duration(0)
    };

    result.metrics.timeline().setBaseTime(start);

    MT_INFO("scenario", "Starting scenario: " + name);

    try {
        result.passed = scenario_fn(ctx);
    } catch (const std::exception& e) {
        result.passed = false;
        result.summary = "Exception: " + std::string(e.what());
    }

    result.elapsed = std::chrono::duration_cast<Duration>(SteadyClock::now() - start);
    result.summary = result.passed ? "PASSED" : "FAILED";

    MT_INFO("scenario", name + ": " + result.summary +
            " (" + std::to_string(result.elapsed.count()) + "ms)");

    return result;
}

} // namespace mt
