#pragma once

#include <cstdint>
#include <atomic>

namespace mt {

class Counter {
    uint64_t value_ = 0;
public:
    void increment(uint64_t delta = 1) { value_ += delta; }
    void reset() { value_ = 0; }
    uint64_t value() const { return value_; }
};

} // namespace mt
