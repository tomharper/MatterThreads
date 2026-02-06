#include "thread/Routing.h"
#include <algorithm>

namespace mt {

void RoutingTable::addDirectNeighbor(uint8_t router_id, TimePoint now) {
    if (router_id >= MAX_ROUTERS) return;
    auto& entry = entries_[router_id];
    entry.router_id = router_id;
    entry.next_hop = router_id; // direct
    entry.cost = 1;
    entry.reachable = true;
    entry.last_updated = now;
}

void RoutingTable::updateFromAdvertisement(uint8_t neighbor_router_id,
                                            const std::vector<RouteEntry>& neighbor_routes,
                                            TimePoint now) {
    // First ensure the neighbor itself is a direct neighbor
    addDirectNeighbor(neighbor_router_id, now);

    for (const auto& nr : neighbor_routes) {
        if (nr.router_id >= MAX_ROUTERS) continue;
        if (nr.router_id == own_router_id_) continue; // skip self
        if (!nr.reachable) continue;

        uint8_t new_cost = static_cast<uint8_t>(std::min(
            static_cast<int>(nr.cost) + 1,
            static_cast<int>(ROUTE_COST_INFINITE)));

        auto& entry = entries_[nr.router_id];

        // Update if:
        // 1. We don't have a route yet
        // 2. New route is cheaper
        // 3. Route goes through the same next hop (update cost)
        if (!entry.reachable || new_cost < entry.cost ||
            entry.next_hop == neighbor_router_id) {
            entry.router_id = nr.router_id;
            entry.next_hop = neighbor_router_id;
            entry.cost = new_cost;
            entry.reachable = (new_cost < ROUTE_COST_INFINITE);
            entry.last_updated = now;
        }
    }
}

std::optional<uint8_t> RoutingTable::getNextHop(uint8_t dest_router_id) const {
    if (dest_router_id >= MAX_ROUTERS) return std::nullopt;
    const auto& entry = entries_[dest_router_id];
    if (!entry.reachable) return std::nullopt;
    return entry.next_hop;
}

uint8_t RoutingTable::getCost(uint8_t dest_router_id) const {
    if (dest_router_id >= MAX_ROUTERS) return ROUTE_COST_INFINITE;
    return entries_[dest_router_id].cost;
}

void RoutingTable::invalidateRouter(uint8_t router_id) {
    if (router_id >= MAX_ROUTERS) return;

    // Invalidate the router itself
    entries_[router_id].reachable = false;
    entries_[router_id].cost = ROUTE_COST_INFINITE;

    // Invalidate all routes that go through this router
    for (auto& entry : entries_) {
        if (entry.next_hop == router_id && entry.router_id != router_id) {
            entry.reachable = false;
            entry.cost = ROUTE_COST_INFINITE;
        }
    }
}

void RoutingTable::expireStaleRoutes(TimePoint now, Duration timeout) {
    for (auto& entry : entries_) {
        if (entry.reachable && (now - entry.last_updated) > timeout) {
            entry.reachable = false;
            entry.cost = ROUTE_COST_INFINITE;
        }
    }
}

std::vector<RouteEntry> RoutingTable::getAdvertisableRoutes() const {
    std::vector<RouteEntry> routes;
    for (const auto& entry : entries_) {
        if (entry.reachable && entry.router_id != INVALID_ROUTER_ID) {
            routes.push_back(entry);
        }
    }
    return routes;
}

size_t RoutingTable::reachableCount() const {
    size_t count = 0;
    for (const auto& entry : entries_) {
        if (entry.reachable) ++count;
    }
    return count;
}

} // namespace mt
