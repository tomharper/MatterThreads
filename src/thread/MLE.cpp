#include "thread/MLE.h"
#include <cstring>

namespace mt {

bool MLEEngine::shouldAdvertise(TimePoint now) const {
    if (last_advertisement_ == TimePoint{}) return true; // Never sent
    return (now - last_advertisement_) >= advertisement_interval_;
}

MLEAdvertisement MLEEngine::buildAdvertisement(uint8_t router_id, RLOC16 rloc16,
                                                 uint64_t ext_addr,
                                                 const RoutingTable& routing) {
    MLEAdvertisement adv;
    adv.source_router_id = router_id;
    adv.source_rloc16 = rloc16;
    adv.source_ext_addr = ext_addr;
    adv.frame_counter = ++frame_counter_;
    adv.route_data = routing.getAdvertisableRoutes();
    return adv;
}

std::vector<uint8_t> MLEEngine::serializeAdvertisement(const MLEAdvertisement& adv) {
    std::vector<uint8_t> buf;

    // Header: type marker
    buf.push_back(0x01); // MLE Advertisement type

    // Source router ID
    buf.push_back(adv.source_router_id);

    // Source RLOC16
    buf.push_back(static_cast<uint8_t>(adv.source_rloc16 & 0xFF));
    buf.push_back(static_cast<uint8_t>((adv.source_rloc16 >> 8) & 0xFF));

    // Extended address
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((adv.source_ext_addr >> (i * 8)) & 0xFF));
    }

    // Frame counter
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<uint8_t>((adv.frame_counter >> (i * 8)) & 0xFF));
    }

    // Route count
    auto route_count = static_cast<uint8_t>(adv.route_data.size());
    buf.push_back(route_count);

    // Route entries: router_id(1) + cost(1) = 2 bytes each
    for (const auto& route : adv.route_data) {
        buf.push_back(route.router_id);
        buf.push_back(route.cost);
    }

    return buf;
}

MLEAdvertisement MLEEngine::deserializeAdvertisement(const uint8_t* data, size_t len) {
    MLEAdvertisement adv{};
    if (len < 16) return adv; // Minimum: 1 + 1 + 2 + 8 + 4 + 1 = 17... but we need at least header

    size_t pos = 0;

    // Type marker
    if (data[pos++] != 0x01) return adv;

    adv.source_router_id = data[pos++];

    adv.source_rloc16 = static_cast<RLOC16>(data[pos]) |
                        (static_cast<RLOC16>(data[pos + 1]) << 8);
    pos += 2;

    adv.source_ext_addr = 0;
    for (int i = 0; i < 8; ++i) {
        adv.source_ext_addr |= (static_cast<uint64_t>(data[pos + static_cast<size_t>(i)]) << (i * 8));
    }
    pos += 8;

    adv.frame_counter = 0;
    for (int i = 0; i < 4; ++i) {
        adv.frame_counter |= (static_cast<uint32_t>(data[pos + static_cast<size_t>(i)]) << (i * 8));
    }
    pos += 4;

    if (pos >= len) return adv;

    uint8_t route_count = data[pos++];
    for (uint8_t i = 0; i < route_count && (pos + 1) < len; ++i) {
        RouteEntry entry;
        entry.router_id = data[pos++];
        entry.cost = data[pos++];
        entry.reachable = (entry.cost < ROUTE_COST_INFINITE);
        adv.route_data.push_back(entry);
    }

    return adv;
}

} // namespace mt
