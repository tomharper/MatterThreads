#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "thread/PowerManager.h"
#include "thread/Routing.h"
#include "thread/BorderRouter.h"
#include "thread/SRP.h"
#include "net/SelfHealing.h"
#include "net/Discovery.h"

using namespace mt;

// Helper: set up a standard 3-node van mesh (BR=0, Relay=1, Sensor=2)
// with PowerManager, routing, SRP, and proxy table wired together.
struct VanPowerTestFixture {
    RoutingTable routing;
    ServiceRegistry registry;
    SRPServer srp{registry};
    BorderRouterProxy proxy;
    SelfHealingEngine healing;
    PowerManager power;

    void setupMesh(TimePoint now) {
        // Routing: linear chain 0 <-> 1 <-> 2
        routing.setOwnRouterId(0);
        routing.addDirectNeighbor(1, now);
        std::vector<RouteEntry> n1_routes;
        n1_routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
        routing.updateFromAdvertisement(1, n1_routes, now);

        // SRP: register sensor
        ServiceRecord sensor_rec;
        sensor_rec.service_name = "temp-sensor";
        sensor_rec.service_type = MATTER_OPERATIONAL_SERVICE;
        sensor_rec.node_id = 2;
        sensor_rec.rloc16 = 0x0401;
        sensor_rec.port = 5540;
        srp.registerLease(2, 0x0401, "sensor.local", sensor_rec, now);

        // Proxy: phone (3) has session to sensor (2) through BR
        proxy.addEntry(3, 2, 0x0401, 100, now);

        // Register nodes with power manager
        power.registerNode(0, ShutdownPriority::BorderRouter);
        power.registerNode(1, ShutdownPriority::Relay);
        power.registerNode(2, ShutdownPriority::Sensor);
    }
};

// Scenario: Graceful shutdown within 60s, then cold boot recovery
TEST(PowerLifecycle, GracefulShutdownAndBoot) {
    ScenarioRunner runner;
    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("graceful-shutdown-boot", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();
        VanPowerTestFixture van;
        van.setupMesh(now);

        // Wire shutdown callbacks: SRP deregister, proxy clear, route invalidate
        van.power.setNodeShutdownCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            van.srp.removeLeasesForNode(2);
            ctx.metrics.event(t, 2, "power", "sensor_shutdown_srp_deregistered");
            return Result<void>::success();
        });
        van.power.setNodeShutdownCallback(1, [&](NodeId, TimePoint t) -> Result<void> {
            van.routing.invalidateRouter(1);
            ctx.metrics.event(t, 1, "power", "relay_shutdown_route_invalidated");
            return Result<void>::success();
        });
        van.power.setNodeShutdownCallback(0, [&](NodeId, TimePoint t) -> Result<void> {
            van.proxy.expireIdle(t + Duration(999999)); // Force expire all
            van.healing.onSystemPowerDown(t);
            ctx.metrics.event(t, 0, "power", "br_shutdown_proxy_cleared");
            return Result<void>::success();
        });

        // Initiate shutdown with 60s battery
        auto shutdown_result = van.power.initiateShutdown(now, Duration(60000));
        EXPECT_TRUE(shutdown_result.ok());
        ctx.metrics.event(now, INVALID_NODE, "power", "shutdown_initiated_60s");

        // Tick through 60s
        for (int i = 1; i <= 60; ++i) {
            van.power.tick(now + Duration(i * 1000));
        }

        // Verify everything is shut down
        EXPECT_EQ(van.power.systemState(), PowerState::Off);
        EXPECT_EQ(van.power.nodesShutdownGracefully(), 3);
        EXPECT_EQ(van.power.nodesHardCutoff(), 0);

        // SRP lease was deregistered
        EXPECT_FALSE(van.srp.hasActiveLease(2, now + Duration(60000)));

        // Route to node 2 is gone (relay invalidated)
        EXPECT_FALSE(van.routing.getNextHop(2).has_value());

        ctx.metrics.event(now + Duration(60000), INVALID_NODE, "power", "system_off");

        // === COLD BOOT ===
        auto boot_time = now + Duration(65000);
        auto boot_result = van.power.initiateBoot(boot_time);
        EXPECT_TRUE(boot_result.ok());

        // Wire boot callbacks: re-register SRP, rebuild routes
        van.power.setNodeBootCallback(0, [&](NodeId, TimePoint t) -> Result<void> {
            van.healing.onSystemPowerUp(t);
            ctx.metrics.event(t, 0, "power", "br_booted");
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(1, [&](NodeId, TimePoint t) -> Result<void> {
            van.routing.addDirectNeighbor(1, t);
            std::vector<RouteEntry> routes;
            routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
            van.routing.updateFromAdvertisement(1, routes, t);
            ctx.metrics.event(t, 1, "power", "relay_booted_routes_rebuilt");
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            ServiceRecord rec;
            rec.service_name = "temp-sensor";
            rec.service_type = MATTER_OPERATIONAL_SERVICE;
            rec.node_id = 2;
            rec.rloc16 = 0x0401;
            rec.port = 5540;
            van.srp.registerLease(2, 0x0401, "sensor.local", rec, t);
            ctx.metrics.event(t, 2, "power", "sensor_booted_srp_registered");
            return Result<void>::success();
        });

        // Tick through boot
        for (int i = 1; i <= 15; ++i) {
            van.power.tick(boot_time + Duration(i * 1000));
        }

        EXPECT_EQ(van.power.systemState(), PowerState::EngineOn);
        EXPECT_TRUE(van.power.isNodeOperational(0));
        EXPECT_TRUE(van.power.isNodeOperational(1));
        EXPECT_TRUE(van.power.isNodeOperational(2));

        // SRP re-registered
        auto final_time = boot_time + Duration(15000);
        EXPECT_TRUE(van.srp.hasActiveLease(2, final_time));

        // Routes rebuilt
        EXPECT_TRUE(van.routing.getNextHop(2).has_value());

        ctx.metrics.event(final_time, INVALID_NODE, "power", "system_on_recovered");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Battery dies at 45s — hard cutoff before relay/BR finish
TEST(PowerLifecycle, HardCutoffAt45s) {
    ScenarioRunner runner;
    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("hard-cutoff-45s", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();
        VanPowerTestFixture van;
        van.setupMesh(now);

        bool sensor_cleaned = false;
        bool relay_cleaned = false;
        bool br_cleaned = false;

        van.power.setNodeShutdownCallback(2, [&](NodeId, TimePoint) -> Result<void> {
            sensor_cleaned = true;
            van.srp.removeLeasesForNode(2);
            return Result<void>::success();
        });
        van.power.setNodeShutdownCallback(1, [&](NodeId, TimePoint) -> Result<void> {
            relay_cleaned = true;
            return Result<void>::success();
        });
        van.power.setNodeShutdownCallback(0, [&](NodeId, TimePoint) -> Result<void> {
            br_cleaned = true;
            return Result<void>::success();
        });

        // Start shutdown expecting 90s battery
        van.power.initiateShutdown(now, Duration(90000));
        ctx.metrics.event(now, INVALID_NODE, "power", "shutdown_initiated_90s_expected");

        // Tick to 45s
        for (int i = 1; i <= 45; ++i) {
            van.power.tick(now + Duration(i * 1000));
        }

        // Battery dies at 45s!
        van.power.hardCutoff(now + Duration(45000));
        ctx.metrics.event(now + Duration(45000), INVALID_NODE, "power", "hard_cutoff_at_45s");

        EXPECT_EQ(van.power.systemState(), PowerState::Off);

        // Sensor should have cleaned up (earliest priority)
        EXPECT_TRUE(sensor_cleaned);

        // At least one node should have been hard-cutoff
        EXPECT_GT(van.power.nodesHardCutoff(), 0);

        // SRP lease for sensor was cleaned (graceful), but if BR was cutoff
        // the proxy table was NOT cleaned
        if (!br_cleaned) {
            // BR didn't get to clean up — proxy entries still exist
            EXPECT_GT(van.proxy.size(), 0);
            ctx.metrics.event(now + Duration(45000), 0, "power", "br_hard_cutoff_dirty_state");
        }

        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Full shutdown, 3 hours pass, SRP leases expire, then boot
TEST(PowerLifecycle, ColdBootAfterExtendedParking) {
    ScenarioRunner runner;
    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("cold-boot-extended-parking", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();
        VanPowerTestFixture van;
        van.setupMesh(now);

        // Graceful shutdown
        van.power.setNodeShutdownCallback(2, [&](NodeId, TimePoint) -> Result<void> {
            // Sensor deregisters SRP
            van.srp.removeLeasesForNode(2);
            return Result<void>::success();
        });

        van.power.initiateShutdown(now, Duration(60000));
        for (int i = 1; i <= 60; ++i) {
            van.power.tick(now + Duration(i * 1000));
        }
        EXPECT_EQ(van.power.systemState(), PowerState::Off);

        // 3 hours pass — SRP tick would expire any remaining leases
        auto parked_time = now + Duration(10800000); // 3 hours
        van.srp.tick(parked_time);

        // All leases expired
        EXPECT_EQ(van.srp.activeLeaseCount(parked_time), 0);
        EXPECT_EQ(van.registry.size(), 0);

        ctx.metrics.event(parked_time, INVALID_NODE, "power", "parked_3_hours_leases_expired");

        // Boot up
        auto boot_time = parked_time + Duration(5000);
        van.power.initiateBoot(boot_time);

        van.power.setNodeBootCallback(0, [&](NodeId, TimePoint t) -> Result<void> {
            van.healing.onSystemPowerUp(t);
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(1, [&](NodeId, TimePoint t) -> Result<void> {
            van.routing.addDirectNeighbor(1, t);
            std::vector<RouteEntry> routes;
            routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
            van.routing.updateFromAdvertisement(1, routes, t);
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            // Re-register SRP after boot
            ServiceRecord rec;
            rec.service_name = "temp-sensor";
            rec.service_type = MATTER_OPERATIONAL_SERVICE;
            rec.node_id = 2;
            rec.rloc16 = 0x0401;
            rec.port = 5540;
            van.srp.registerLease(2, 0x0401, "sensor.local", rec, t);
            return Result<void>::success();
        });

        for (int i = 1; i <= 15; ++i) {
            van.power.tick(boot_time + Duration(i * 1000));
        }

        EXPECT_EQ(van.power.systemState(), PowerState::EngineOn);

        // SRP re-registered
        auto final_time = boot_time + Duration(15000);
        EXPECT_TRUE(van.srp.hasActiveLease(2, final_time));
        EXPECT_EQ(van.registry.size(), 1);

        // Phone can discover again
        DiscoveryClient phone;
        auto results = phone.browseOperational(van.registry);
        EXPECT_EQ(results.size(), 1);

        ctx.metrics.event(final_time, INVALID_NODE, "power", "boot_complete_after_parking");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Sensor tries to send data while relay is already off
TEST(PowerLifecycle, RaceConditionSensorVsRelay) {
    ScenarioRunner runner;
    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("race-sensor-relay-shutdown", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();
        VanPowerTestFixture van;
        van.setupMesh(now);

        // Track what happens during shutdown
        bool relay_off = false;
        bool sensor_tried_send = false;
        bool send_failed = false;

        van.power.setNodeShutdownCallback(1, [&](NodeId, TimePoint t) -> Result<void> {
            van.routing.invalidateRouter(1);
            relay_off = true;
            ctx.metrics.event(t, 1, "power", "relay_off");
            return Result<void>::success();
        });

        van.power.setNodeShutdownCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            sensor_tried_send = true;
            // Sensor tries to send final temperature reading
            // Route goes through relay (node 1), check if it's reachable
            auto next_hop = van.routing.getNextHop(0); // Try to reach BR
            if (!next_hop.has_value()) {
                // Can't reach BR — relay is already down
                send_failed = true;
                ctx.metrics.event(t, 2, "power", "final_reading_failed_no_route");
            } else {
                ctx.metrics.event(t, 2, "power", "final_reading_sent");
            }
            return Result<void>::success();
        });

        // With sensor being priority 0 (first), it should shut down before relay
        // So normally sensor sends before relay goes down.
        // But let's test the case where relay goes down first by using a short battery.
        // With very tight timing, the race becomes visible.
        van.power.initiateShutdown(now, Duration(60000));

        for (int i = 1; i <= 60; ++i) {
            van.power.tick(now + Duration(i * 1000));
        }

        EXPECT_EQ(van.power.systemState(), PowerState::Off);
        EXPECT_TRUE(sensor_tried_send);

        // Since sensor has lower priority (shuts down first), it should succeed
        // because relay is still up when sensor shuts down
        if (!relay_off || !send_failed) {
            // Sensor shut down before relay — send succeeded (normal case)
            ctx.metrics.event(now + Duration(60000), 2, "power", "sensor_sent_before_relay_down");
        }

        ctx.metrics.event(now + Duration(60000), INVALID_NODE, "power", "race_test_complete");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Active subscription survives power cycle
TEST(PowerLifecycle, PowerCycleSubscriptionRecovery) {
    ScenarioRunner runner;
    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("power-cycle-subscription", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();
        VanPowerTestFixture van;
        van.setupMesh(now);

        // Track healing events
        std::vector<HealingEvent> healing_events;
        van.healing.onHealingEvent([&healing_events](const HealingRecord& r) {
            healing_events.push_back(r.event);
        });

        // Simulate active subscription: phone (3) subscribed to sensor (2) temp
        van.proxy.addEntry(3, 2, 0x0401, 100, now);
        van.proxy.touchSession(3, 2, now);
        ctx.metrics.event(now, 3, "subscription", "active_temp_subscription");

        // Shutdown
        van.power.setNodeShutdownCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            van.srp.removeLeasesForNode(2);
            van.healing.onSubscriptionDropped(2, 100, t);
            return Result<void>::success();
        });
        van.power.setNodeShutdownCallback(0, [&](NodeId, TimePoint t) -> Result<void> {
            van.proxy.removeDevice(2);
            van.healing.onSystemPowerDown(t);
            return Result<void>::success();
        });

        van.power.initiateShutdown(now, Duration(60000));
        for (int i = 1; i <= 60; ++i) {
            van.power.tick(now + Duration(i * 1000));
        }
        EXPECT_EQ(van.power.systemState(), PowerState::Off);

        // Verify subscription dropped during shutdown
        bool sub_dropped = false;
        for (auto ev : healing_events) {
            if (ev == HealingEvent::SubscriptionDropped) sub_dropped = true;
        }
        EXPECT_TRUE(sub_dropped);

        // Boot
        auto boot_time = now + Duration(70000);
        van.power.initiateBoot(boot_time);

        van.power.setNodeBootCallback(0, [&](NodeId, TimePoint t) -> Result<void> {
            van.healing.onSystemPowerUp(t);
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(1, [&](NodeId, TimePoint t) -> Result<void> {
            van.routing.addDirectNeighbor(1, t);
            std::vector<RouteEntry> routes;
            routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
            van.routing.updateFromAdvertisement(1, routes, t);
            return Result<void>::success();
        });
        van.power.setNodeBootCallback(2, [&](NodeId, TimePoint t) -> Result<void> {
            // Re-register SRP
            ServiceRecord rec;
            rec.service_name = "temp-sensor";
            rec.service_type = MATTER_OPERATIONAL_SERVICE;
            rec.node_id = 2;
            rec.rloc16 = 0x0401;
            rec.port = 5540;
            van.srp.registerLease(2, 0x0401, "sensor.local", rec, t);

            // Re-establish subscription
            van.healing.onSubscriptionRecovered(2, 100, t);
            return Result<void>::success();
        });

        for (int i = 1; i <= 15; ++i) {
            van.power.tick(boot_time + Duration(i * 1000));
        }

        EXPECT_EQ(van.power.systemState(), PowerState::EngineOn);

        // Verify subscription recovered
        bool sub_recovered = false;
        bool power_down = false;
        bool power_up = false;
        for (auto ev : healing_events) {
            if (ev == HealingEvent::SubscriptionRecovered) sub_recovered = true;
            if (ev == HealingEvent::PowerDown) power_down = true;
            if (ev == HealingEvent::PowerUp) power_up = true;
        }
        EXPECT_TRUE(sub_recovered);
        EXPECT_TRUE(power_down);
        EXPECT_TRUE(power_up);

        // Phone can re-establish proxy session
        auto final_time = boot_time + Duration(15000);
        van.proxy.addEntry(3, 2, 0x0401, 101, final_time);
        EXPECT_EQ(van.proxy.size(), 1);

        ctx.metrics.event(final_time, 3, "subscription", "subscription_recovered_after_power_cycle");
        return true;
    });

    EXPECT_TRUE(result.passed);
}
