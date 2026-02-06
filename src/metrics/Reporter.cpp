#include "metrics/Reporter.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace mt {

std::string Reporter::summaryText() const {
    std::ostringstream ss;
    ss << "=== MatterThreads Metrics Report ===\n\n";

    // Counters
    ss << "Counters:\n";
    for (const auto& [name, counter] : collector_.counters()) {
        ss << "  " << name << ": " << counter.value() << "\n";
    }

    // Histograms
    ss << "\nHistograms:\n";
    for (const auto& [name, hist] : collector_.histograms()) {
        auto s = hist.stats();
        if (s.count == 0) continue;
        ss << "  " << name << ":\n";
        ss << std::fixed << std::setprecision(2);
        ss << "    count=" << s.count
           << " min=" << s.min
           << " max=" << s.max
           << " mean=" << s.mean << "\n";
        ss << "    p50=" << s.p50
           << " p95=" << s.p95
           << " p99=" << s.p99 << "\n";
    }

    // Timeline summary
    ss << "\nTimeline: " << collector_.timeline().size() << " events\n";

    return ss.str();
}

std::string Reporter::summaryJson() const {
    nlohmann::json j;

    // Counters
    nlohmann::json counters_j;
    for (const auto& [name, counter] : collector_.counters()) {
        counters_j[name] = counter.value();
    }
    j["counters"] = counters_j;

    // Histograms
    nlohmann::json hist_j;
    for (const auto& [name, hist] : collector_.histograms()) {
        auto s = hist.stats();
        hist_j[name] = {
            {"count", s.count},
            {"min", s.min},
            {"max", s.max},
            {"mean", s.mean},
            {"p50", s.p50},
            {"p95", s.p95},
            {"p99", s.p99},
            {"sum", s.sum}
        };
    }
    j["histograms"] = hist_j;

    // Timeline
    j["timeline"] = nlohmann::json::parse(collector_.timeline().exportJson());

    return j.dump(2);
}

void Reporter::printDashboard() const {
    // Clear screen
    std::fprintf(stderr, "\033[2J\033[H");
    std::fprintf(stderr, "=== MatterThreads Live Dashboard ===\n\n");

    // Counters
    std::fprintf(stderr, "COUNTERS\n");
    for (const auto& [name, counter] : collector_.counters()) {
        std::fprintf(stderr, "  %-40s %llu\n", name.c_str(), static_cast<unsigned long long>(counter.value()));
    }

    // Histograms
    std::fprintf(stderr, "\nHISTOGRAMS\n");
    for (const auto& [name, hist] : collector_.histograms()) {
        auto s = hist.stats();
        if (s.count == 0) continue;
        std::fprintf(stderr, "  %-30s n=%-6zu p50=%-8.1f p95=%-8.1f p99=%-8.1f\n",
                     name.c_str(), s.count, s.p50, s.p95, s.p99);
    }

    std::fprintf(stderr, "\nTIMELINE: %zu events\n", collector_.timeline().size());
}

} // namespace mt
