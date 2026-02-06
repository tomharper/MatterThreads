#pragma once

#include "core/Types.h"

namespace mt {

class Clock {
    bool simulated_ = false;
    TimePoint simulated_now_;
public:
    Clock() : simulated_now_(SteadyClock::now()) {}

    TimePoint now() const {
        return simulated_ ? simulated_now_ : SteadyClock::now();
    }

    void setSimulated(bool sim) { simulated_ = sim; }
    bool isSimulated() const { return simulated_; }

    void advance(Duration d) {
        if (simulated_) simulated_now_ += d;
    }

    void setNow(TimePoint tp) {
        simulated_ = true;
        simulated_now_ = tp;
    }

    Duration elapsed(TimePoint since) const {
        return std::chrono::duration_cast<Duration>(now() - since);
    }
};

} // namespace mt
