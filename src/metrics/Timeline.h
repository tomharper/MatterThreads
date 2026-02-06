#pragma once

#include "core/Types.h"
#include <vector>
#include <string>
#include <optional>

namespace mt {

struct TimelineEvent {
    TimePoint timestamp;
    NodeId node;
    std::string category;
    std::string event;
    std::string detail;
};

class Timeline {
    std::vector<TimelineEvent> events_;
    TimePoint base_time_{}; // For relative timestamps in output

public:
    void setBaseTime(TimePoint t) { base_time_ = t; }

    void record(TimelineEvent e) { events_.push_back(std::move(e)); }

    void record(TimePoint when, NodeId node, std::string category,
                std::string event, std::string detail = "") {
        events_.push_back({when, node, std::move(category),
                           std::move(event), std::move(detail)});
    }

    std::vector<TimelineEvent> query(
        std::optional<NodeId> node = std::nullopt,
        std::optional<std::string> category = std::nullopt,
        std::optional<TimePoint> from = std::nullopt,
        std::optional<TimePoint> to = std::nullopt) const;

    std::string exportJson() const;
    std::string exportCsv() const;

    size_t size() const { return events_.size(); }
    void clear() { events_.clear(); }
    const std::vector<TimelineEvent>& events() const { return events_; }
};

} // namespace mt
