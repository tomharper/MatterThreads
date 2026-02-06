#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "thread/Routing.h"

using namespace mt;

TEST(Scenarios, RouteFailureAndReroute) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::fullyConnected();
    config.random_seed = 42;

    auto result = runner.run("route-failure", config, [](ScenarioContext& ctx) {
        // Node 0 has routes to Node 2 via both Node 1 and direct
        RoutingTable rt;
        rt.setOwnRouterId(0);
        auto now = SteadyClock::now();

        // Direct route to Node 2 (cost 1)
        rt.addDirectNeighbor(2, now);
        // Route via Node 1 (cost 2)
        rt.addDirectNeighbor(1, now);
        std::vector<RouteEntry> n1_routes;
        n1_routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
        rt.updateFromAdvertisement(1, n1_routes, now);

        // Should prefer direct route (cost 1)
        EXPECT_EQ(rt.getCost(2), 1);
        EXPECT_EQ(*rt.getNextHop(2), 2); // Direct

        ctx.metrics.event(now, 0, "route", "initial",
                           "Direct route to Node 2, cost=1");

        // Direct link degrades — invalidate direct route
        rt.invalidateRouter(2);

        // Node 2 should now be unreachable (invalidateRouter removes all routes)
        EXPECT_FALSE(rt.getNextHop(2).has_value());

        ctx.metrics.event(now, 0, "route", "direct_link_failed",
                           "Direct route to Node 2 lost");

        // Re-learn route via Node 1
        rt.addDirectNeighbor(1, now + Duration(1000));
        std::vector<RouteEntry> updated_routes;
        updated_routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
        rt.updateFromAdvertisement(1, updated_routes, now + Duration(1000));

        EXPECT_TRUE(rt.getNextHop(2).has_value());
        EXPECT_EQ(*rt.getNextHop(2), 1); // Via Node 1
        EXPECT_EQ(rt.getCost(2), 2);     // Cost 2 (1 hop to Node 1 + 1 hop to Node 2)

        ctx.metrics.event(now + Duration(1000), 0, "route", "rerouted",
                           "Route to Node 2 via Node 1, cost=2");

        ctx.metrics.increment("route_cost_changes");
        return true;
    });

    EXPECT_TRUE(result.passed);
}
