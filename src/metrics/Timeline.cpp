#include "metrics/Timeline.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace mt {

std::vector<TimelineEvent> Timeline::query(
    std::optional<NodeId> node,
    std::optional<std::string> category,
    std::optional<TimePoint> from,
    std::optional<TimePoint> to) const
{
    std::vector<TimelineEvent> result;
    for (const auto& e : events_) {
        if (node && e.node != *node) continue;
        if (category && e.category != *category) continue;
        if (from && e.timestamp < *from) continue;
        if (to && e.timestamp > *to) continue;
        result.push_back(e);
    }
    return result;
}

std::string Timeline::exportJson() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& e : events_) {
        auto ms = std::chrono::duration_cast<Duration>(e.timestamp - base_time_).count();
        j.push_back({
            {"time_ms", ms},
            {"node", e.node},
            {"category", e.category},
            {"event", e.event},
            {"detail", e.detail}
        });
    }
    return j.dump(2);
}

std::string Timeline::exportCsv() const {
    std::ostringstream ss;
    ss << "time_ms,node,category,event,detail\n";
    for (const auto& e : events_) {
        auto ms = std::chrono::duration_cast<Duration>(e.timestamp - base_time_).count();
        ss << ms << "," << e.node << "," << e.category << ","
           << e.event << ",\"" << e.detail << "\"\n";
    }
    return ss.str();
}

} // namespace mt
