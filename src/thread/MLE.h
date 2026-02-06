#pragma once

#include "core/Types.h"
#include "thread/Routing.h"
#include <vector>
#include <string>

namespace mt {

// MLE Advertisement message (simplified)
struct MLEAdvertisement {
    uint8_t  source_router_id;
    RLOC16   source_rloc16;
    uint64_t source_ext_addr;
    uint32_t frame_counter;
    std::vector<RouteEntry> route_data;  // Routes known by this router
};

// MLE Parent Request/Response for end devices
struct MLEParentRequest {
    uint64_t child_ext_addr;
    bool     request_full_network_data;
};

struct MLEParentResponse {
    uint8_t  router_id;
    RLOC16   router_rloc16;
    uint8_t  link_quality;
    uint8_t  connectivity_metric;  // How well connected is this router
};

class MLEEngine {
    Duration advertisement_interval_ = Duration(10000); // 10s
    TimePoint last_advertisement_{};
    uint32_t frame_counter_ = 0;

public:
    void setAdvertisementInterval(Duration interval) { advertisement_interval_ = interval; }
    Duration advertisementInterval() const { return advertisement_interval_; }

    // Check if it's time to send an advertisement
    bool shouldAdvertise(TimePoint now) const;

    // Build an advertisement message for this router
    MLEAdvertisement buildAdvertisement(uint8_t router_id, RLOC16 rloc16,
                                         uint64_t ext_addr,
                                         const RoutingTable& routing);

    // Serialize advertisement to bytes for transmission
    static std::vector<uint8_t> serializeAdvertisement(const MLEAdvertisement& adv);

    // Deserialize advertisement from bytes
    static MLEAdvertisement deserializeAdvertisement(const uint8_t* data, size_t len);

    void markAdvertised(TimePoint now) { last_advertisement_ = now; }
};

} // namespace mt
