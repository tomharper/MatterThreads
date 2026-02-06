#pragma once

#include "core/Types.h"
#include <array>
#include <optional>
#include <vector>

namespace mt {

static constexpr uint8_t MAX_ROUTERS = 63;
static constexpr uint8_t ROUTE_COST_INFINITE = 16;

struct RouteEntry {
    uint8_t  router_id = INVALID_ROUTER_ID;
    uint8_t  next_hop = INVALID_ROUTER_ID;  // 0xFF = direct neighbor
    uint8_t  cost = ROUTE_COST_INFINITE;
    bool     reachable = false;
    TimePoint last_updated{};
};

class RoutingTable {
    std::array<RouteEntry, MAX_ROUTERS> entries_{};
    uint8_t own_router_id_ = INVALID_ROUTER_ID;

public:
    void setOwnRouterId(uint8_t id) { own_router_id_ = id; }
    uint8_t ownRouterId() const { return own_router_id_; }

    // Add or update a direct neighbor
    void addDirectNeighbor(uint8_t router_id, TimePoint now);

    // Update from a neighbor's route advertisement
    void updateFromAdvertisement(uint8_t neighbor_router_id,
                                  const std::vector<RouteEntry>& neighbor_routes,
                                  TimePoint now);

    // Get next hop for a destination router
    std::optional<uint8_t> getNextHop(uint8_t dest_router_id) const;

    // Get cost to a destination
    uint8_t getCost(uint8_t dest_router_id) const;

    // Mark a router as unreachable
    void invalidateRouter(uint8_t router_id);

    // Expire stale routes (not updated within timeout)
    void expireStaleRoutes(TimePoint now, Duration timeout);

    // Get all reachable route entries (for advertising to neighbors)
    std::vector<RouteEntry> getAdvertisableRoutes() const;

    // Get entry by router ID
    const RouteEntry& getEntry(uint8_t router_id) const { return entries_[router_id]; }

    // How many reachable routers
    size_t reachableCount() const;
};

} // namespace mt
