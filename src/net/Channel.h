#pragma once

#include "core/Types.h"
#include "core/Random.h"
#include <algorithm>

namespace mt {

struct DeliveryDecision {
    bool deliver = true;
    Duration delay{0};
    uint8_t delivered_lqi = 200;
    int8_t  delivered_rssi = -60;
};

struct LinkParams {
    float base_loss_rate = 0.0f;      // 0.0 to 1.0
    float latency_mean_ms = 5.0f;
    float latency_stddev_ms = 1.0f;
    uint8_t lqi = 200;
    int8_t  rssi = -60;
    bool link_up = true;
};

class Channel {
    Random& rng_;
public:
    explicit Channel(Random& rng) : rng_(rng) {}

    DeliveryDecision evaluate(const LinkParams& params) {
        DeliveryDecision d;

        if (!params.link_up) {
            d.deliver = false;
            return d;
        }

        // Packet loss
        if (params.base_loss_rate > 0.0f && rng_.chance(static_cast<double>(params.base_loss_rate))) {
            d.deliver = false;
            return d;
        }

        // Latency (gaussian, clamped to >= 0)
        double latency = rng_.nextGaussian(
            static_cast<double>(params.latency_mean_ms),
            static_cast<double>(params.latency_stddev_ms));
        latency = std::max(0.0, latency);
        d.delay = Duration(static_cast<int64_t>(latency));

        // Link quality
        d.delivered_lqi = params.lqi;
        d.delivered_rssi = params.rssi;

        return d;
    }
};

} // namespace mt
