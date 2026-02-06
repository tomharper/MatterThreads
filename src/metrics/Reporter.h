#pragma once

#include "metrics/Collector.h"
#include <string>

namespace mt {

class Reporter {
    const Collector& collector_;
public:
    explicit Reporter(const Collector& c) : collector_(c) {}

    std::string summaryText() const;
    std::string summaryJson() const;
    void printDashboard() const;
};

} // namespace mt
