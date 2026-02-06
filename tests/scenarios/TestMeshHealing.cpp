#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "thread/Routing.h"

using namespace mt;

TEST(Scenarios, MeshHealingDetectsPartition) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::linearChain();
    config.fault_plan = FaultPlan::meshHealingTest();
    config.random_seed = 42;

    auto result = runner.run("mesh-healing", config, [](ScenarioContext& ctx) {
        // Simulate routing table behavior when middle node goes down
        RoutingTable rt;
        rt.setOwnRouterId(0);

        auto now = SteadyClock::now();

        // Initially: direct neighbor to 1, route to 2 via 1
        rt.addDirectNeighbor(1, now);
        std::vector<RouteEntry> n1_routes;
        n1_routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
        rt.updateFromAdvertisement(1, n1_routes, now);

        EXPECT_EQ(rt.reachableCount(), 2); // Nodes 1 and 2
        EXPECT_EQ(rt.getCost(2), 2);

        // Node 1 goes down — stale after 25s timeout
        auto later = now + Duration(30000);
        rt.expireStaleRoutes(later, Duration(25000));

        // Both node 1 and node 2 (via 1) should be unreachable
        EXPECT_EQ(rt.reachableCount(), 0);
        EXPECT_FALSE(rt.getNextHop(2).has_value());

        ctx.metrics.event(now, 0, "route", "partition_detected",
                           "Node 1 and Node 2 unreachable");

        return true;
    });

    EXPECT_TRUE(result.passed);
}
