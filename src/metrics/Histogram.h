#pragma once

#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace mt {

struct HistogramStats {
    double min = 0;
    double max = 0;
    double mean = 0;
    double p50 = 0;
    double p95 = 0;
    double p99 = 0;
    size_t count = 0;
    double sum = 0;
};

class Histogram {
    std::vector<double> samples_;

public:
    void record(double value) { samples_.push_back(value); }
    void reset() { samples_.clear(); }
    size_t count() const { return samples_.size(); }

    HistogramStats stats() const {
        HistogramStats s;
        if (samples_.empty()) return s;

        s.count = samples_.size();

        // Sort a copy for percentile computation
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        s.min = sorted.front();
        s.max = sorted.back();

        s.sum = 0;
        for (double v : sorted) s.sum += v;
        s.mean = s.sum / static_cast<double>(s.count);

        auto percentile = [&](double p) -> double {
            double idx = p * static_cast<double>(s.count - 1);
            auto lo = static_cast<size_t>(std::floor(idx));
            auto hi = static_cast<size_t>(std::ceil(idx));
            if (lo == hi) return sorted[lo];
            double frac = idx - static_cast<double>(lo);
            return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
        };

        s.p50 = percentile(0.50);
        s.p95 = percentile(0.95);
        s.p99 = percentile(0.99);

        return s;
    }
};

} // namespace mt
