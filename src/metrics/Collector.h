#pragma once

#include "core/Types.h"
#include "metrics/Counter.h"
#include "metrics/Histogram.h"
#include "metrics/Timeline.h"
#include <unordered_map>
#include <string>

namespace mt {

class Collector {
    std::unordered_map<std::string, Counter> counters_;
    std::unordered_map<std::string, Histogram> histograms_;
    Timeline timeline_;

public:
    void increment(const std::string& name, uint64_t delta = 1) {
        counters_[name].increment(delta);
    }

    void record(const std::string& name, double value) {
        histograms_[name].record(value);
    }

    void event(TimePoint when, NodeId node, std::string category,
               std::string event_name, std::string detail = "") {
        timeline_.record(when, node, std::move(category),
                         std::move(event_name), std::move(detail));
    }

    uint64_t getCounter(const std::string& name) const {
        auto it = counters_.find(name);
        return (it != counters_.end()) ? it->second.value() : 0;
    }

    HistogramStats getHistogram(const std::string& name) const {
        auto it = histograms_.find(name);
        return (it != histograms_.end()) ? it->second.stats() : HistogramStats{};
    }

    Timeline& timeline() { return timeline_; }
    const Timeline& timeline() const { return timeline_; }

    const std::unordered_map<std::string, Counter>& counters() const { return counters_; }
    const std::unordered_map<std::string, Histogram>& histograms() const { return histograms_; }

    void reset() {
        counters_.clear();
        histograms_.clear();
        timeline_.clear();
    }
};

} // namespace mt
