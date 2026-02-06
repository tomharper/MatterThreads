#pragma once

#include <cstdint>
#include <random>

namespace mt {

class Random {
    std::mt19937 engine_;
    uint32_t seed_;
public:
    explicit Random(uint32_t seed = 42) : engine_(seed), seed_(seed) {}

    uint32_t seed() const { return seed_; }
    void reseed(uint32_t s) { seed_ = s; engine_.seed(s); }

    uint32_t nextU32() { return engine_(); }
    uint64_t nextU64() { return (static_cast<uint64_t>(engine_()) << 32) | engine_(); }

    // Uniform in [0.0, 1.0)
    double nextDouble() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine_);
    }

    // Uniform in [lo, hi]
    int nextInt(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(engine_);
    }

    // Normal distribution
    double nextGaussian(double mean, double stddev) {
        std::normal_distribution<double> dist(mean, stddev);
        return dist(engine_);
    }

    // Returns true with probability p
    bool chance(double p) { return nextDouble() < p; }

    // Fill buffer with random bytes
    void fill(uint8_t* buf, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            buf[i] = static_cast<uint8_t>(nextU32() & 0xFF);
        }
    }

    std::mt19937& engine() { return engine_; }
};

} // namespace mt
