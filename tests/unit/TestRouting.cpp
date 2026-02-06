#include <gtest/gtest.h>
#include "thread/Routing.h"

using namespace mt;

TEST(Routing, DirectNeighbor) {
    RoutingTable rt;
    rt.setOwnRouterId(0);

    auto now = SteadyClock::now();
    rt.addDirectNeighbor(1, now);

    EXPECT_EQ(rt.getCost(1), 1);
    auto hop = rt.getNextHop(1);
    ASSERT_TRUE(hop.has_value());
    EXPECT_EQ(*hop, 1); // Direct
}

TEST(Routing, TwoHopRoute) {
    RoutingTable rt;
    rt.setOwnRouterId(0);

    auto now = SteadyClock::now();

    // Node 1 is a direct neighbor
    rt.addDirectNeighbor(1, now);

    // Node 1 advertises route to node 2 with cost 1
    std::vector<RouteEntry> neighbor_routes;
    RouteEntry re;
    re.router_id = 2;
    re.cost = 1;
    re.reachable = true;
    neighbor_routes.push_back(re);

    rt.updateFromAdvertisement(1, neighbor_routes, now);

    // Node 2 should be reachable via node 1 with cost 2
    EXPECT_EQ(rt.getCost(2), 2);
    auto hop = rt.getNextHop(2);
    ASSERT_TRUE(hop.has_value());
    EXPECT_EQ(*hop, 1);
}

TEST(Routing, InvalidateRouter) {
    RoutingTable rt;
    rt.setOwnRouterId(0);

    auto now = SteadyClock::now();
    rt.addDirectNeighbor(1, now);
    rt.addDirectNeighbor(2, now);

    EXPECT_EQ(rt.reachableCount(), 2);

    rt.invalidateRouter(1);

    EXPECT_EQ(rt.getCost(1), ROUTE_COST_INFINITE);
    EXPECT_FALSE(rt.getNextHop(1).has_value());
    EXPECT_EQ(rt.reachableCount(), 1);
}

TEST(Routing, ExpireStaleRoutes) {
    RoutingTable rt;
    rt.setOwnRouterId(0);

    auto t0 = SteadyClock::now();
    rt.addDirectNeighbor(1, t0);

    EXPECT_EQ(rt.reachableCount(), 1);

    // Expire after 100ms timeout
    auto t1 = t0 + Duration(200);
    rt.expireStaleRoutes(t1, Duration(100));

    EXPECT_EQ(rt.reachableCount(), 0);
}

TEST(Routing, PreferLowerCost) {
    RoutingTable rt;
    rt.setOwnRouterId(0);
    auto now = SteadyClock::now();

    // Route to 2 via node 1 with cost 3
    rt.addDirectNeighbor(1, now);
    std::vector<RouteEntry> routes1;
    routes1.push_back({.router_id = 2, .cost = 2, .reachable = true});
    rt.updateFromAdvertisement(1, routes1, now);

    EXPECT_EQ(rt.getCost(2), 3); // 2 + 1

    // Better route to 2 via node 3 with cost 2
    rt.addDirectNeighbor(3, now);
    std::vector<RouteEntry> routes3;
    routes3.push_back({.router_id = 2, .cost = 1, .reachable = true});
    rt.updateFromAdvertisement(3, routes3, now);

    EXPECT_EQ(rt.getCost(2), 2); // 1 + 1
    EXPECT_EQ(*rt.getNextHop(2), 3);
}

TEST(Routing, AdvertisableRoutes) {
    RoutingTable rt;
    rt.setOwnRouterId(0);
    auto now = SteadyClock::now();

    rt.addDirectNeighbor(1, now);
    rt.addDirectNeighbor(2, now);

    auto routes = rt.getAdvertisableRoutes();
    EXPECT_EQ(routes.size(), 2);
}
