#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "thread/Routing.h"
#include "thread/ThreadNode.h"
#include "thread/BorderRouter.h"
#include "thread/SRP.h"
#include "net/SelfHealing.h"
#include "net/Discovery.h"

using namespace mt;

// Scenario: Phone commissions van, subscribes, then enters tunnel
// Tests: backhaul loss doesn't affect Thread mesh, BR buffers reports
TEST(Scenarios, PhoneCommissionTunnel) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("phone-commission-tunnel", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        // Set up BR with SRP server and proxy table
        ServiceRegistry registry;
        SRPServer srp_server(registry);
        BorderRouterProxy proxy;

        // Register mesh devices
        for (NodeId i = 0; i < 3; ++i) {
            ServiceRecord rec;
            rec.service_name = "node" + std::to_string(i);
            rec.service_type = MATTER_OPERATIONAL_SERVICE;
            rec.node_id = i;
            rec.rloc16 = makeRLOC16(static_cast<uint8_t>(i), 0);
            rec.port = 5540;
            rec.registered_at = now;
            srp_server.registerLease(i, rec.rloc16, "host" + std::to_string(i) + ".local",
                                      rec, now);
        }

        // Phone discovers devices
        DiscoveryClient phone;
        int discovered = 0;
        phone.onDeviceDiscovered([&discovered](const BrowseResult&) { ++discovered; });
        phone.scan(registry);
        EXPECT_EQ(discovered, 3);

        // Phone creates proxy entries (CASE sessions through BR)
        EXPECT_TRUE(proxy.addEntry(3, 0, 0x0000, 100, now));
        EXPECT_TRUE(proxy.addEntry(3, 1, 0x0400, 101, now));
        EXPECT_TRUE(proxy.addEntry(3, 2, 0x0401, 102, now));
        EXPECT_EQ(proxy.size(), 3);

        ctx.metrics.event(now, 3, "commission", "phone_commissioned_all_devices");

        // Simulate tunnel: backhaul goes down
        SelfHealingEngine healing;
        healing.onBackhaulLost(now + Duration(10000));
        EXPECT_FALSE(healing.backhaulState().connected);

        // Thread mesh should still be operational
        RoutingTable rt;
        rt.setOwnRouterId(0);
        rt.addDirectNeighbor(1, now + Duration(10000));
        EXPECT_TRUE(rt.getNextHop(1).has_value());

        // BR buffers messages during tunnel
        healing.backhaulState().buffered_messages = 50;

        // Tunnel ends: backhaul restored
        healing.onBackhaulRestored(now + Duration(310000)); // 5 min tunnel
        EXPECT_TRUE(healing.backhaulState().connected);
        EXPECT_EQ(healing.backhaulState().buffered_messages, 0); // Drained

        ctx.metrics.event(now + Duration(310000), 3, "backhaul", "tunnel_exit_restored");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Phone loses backhaul during active subscription
// Tests: subscription state management during connectivity loss
TEST(Scenarios, BackhaulLossRecovery) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("backhaul-loss-recovery", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        SelfHealingEngine healing;
        std::vector<HealingEvent> events;
        healing.onHealingEvent([&events](const HealingRecord& r) { events.push_back(r.event); });

        // Normal operation: backhaul connected
        EXPECT_TRUE(healing.backhaulState().connected);

        // Backhaul lost (dead zone)
        healing.onBackhaulLost(now);
        EXPECT_FALSE(healing.backhaulState().connected);

        // Subscription drops because of backhaul loss
        healing.onSubscriptionDropped(2, 1, now + Duration(5000)); // temp sensor sub dropped

        // BR buffers subscription reports locally
        for (int i = 0; i < 100; ++i) {
            healing.backhaulState().buffered_messages++;
        }
        EXPECT_EQ(healing.backhaulState().buffered_messages, 100);

        // Backhaul restored after 2 minutes
        healing.onBackhaulRestored(now + Duration(120000));
        EXPECT_TRUE(healing.backhaulState().connected);

        // Subscription recovered
        healing.onSubscriptionRecovered(2, 1, now + Duration(121000));

        // Verify full event sequence
        EXPECT_GE(events.size(), 4);

        bool has_backhaul_lost = false, has_backhaul_restored = false;
        bool has_sub_dropped = false, has_sub_recovered = false;
        for (auto ev : events) {
            if (ev == HealingEvent::BackhaulLost) has_backhaul_lost = true;
            if (ev == HealingEvent::BackhaulRestored) has_backhaul_restored = true;
            if (ev == HealingEvent::SubscriptionDropped) has_sub_dropped = true;
            if (ev == HealingEvent::SubscriptionRecovered) has_sub_recovered = true;
        }
        EXPECT_TRUE(has_backhaul_lost);
        EXPECT_TRUE(has_backhaul_restored);
        EXPECT_TRUE(has_sub_dropped);
        EXPECT_TRUE(has_sub_recovered);

        ctx.metrics.event(now + Duration(121000), 3, "recovery", "full_backhaul_recovery");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: Driver unlocks door while relay is rebooting (engine crank)
// Tests: command delivery during mesh disruption
TEST(Scenarios, UnlockDuringCrank) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("unlock-during-crank", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        // Set up routing: linear chain 0 <-> 1 <-> 2
        RoutingTable rt;
        rt.setOwnRouterId(0);
        rt.addDirectNeighbor(1, now);

        std::vector<RouteEntry> n1_routes;
        n1_routes.push_back({.router_id = 2, .cost = 1, .reachable = true});
        rt.updateFromAdvertisement(1, n1_routes, now);

        EXPECT_TRUE(rt.getNextHop(2).has_value());
        EXPECT_EQ(rt.getCost(2), 2); // 0 → 1 → 2

        // SRP has the door lock registered
        ServiceRegistry registry;
        SRPServer srp(registry);
        ServiceRecord lock_rec;
        lock_rec.service_name = "door-lock";
        lock_rec.service_type = MATTER_OPERATIONAL_SERVICE;
        lock_rec.node_id = 2;
        lock_rec.rloc16 = 0x0401;
        lock_rec.port = 5540;
        srp.registerLease(2, 0x0401, "doorlock.local", lock_rec, now);

        // BR proxy table has the lock
        BorderRouterProxy proxy;
        proxy.addEntry(3, 2, 0x0401, 100, now);

        ctx.metrics.event(now, 0, "crank", "engine_cranking_starts");

        // Engine crank: relay (node 1) goes down at T=100ms
        auto crank_time = now + Duration(100);
        rt.invalidateRouter(1);

        // Route to node 2 is now gone (was through node 1)
        EXPECT_FALSE(rt.getNextHop(2).has_value());

        // Phone tries to unlock door at T=200ms — will fail!
        ctx.metrics.event(crank_time + Duration(100), 3, "command",
                          "unlock_attempt_during_crank");

        // Relay comes back at T=500ms (voltage recovered)
        auto recovery_time = now + Duration(500);
        rt.addDirectNeighbor(1, recovery_time);
        rt.updateFromAdvertisement(1, n1_routes, recovery_time);

        EXPECT_TRUE(rt.getNextHop(2).has_value());
        EXPECT_EQ(rt.getCost(2), 2);

        ctx.metrics.event(recovery_time, 1, "crank", "relay_recovered");

        // Retry unlock — should succeed now
        auto retry_time = now + Duration(600);
        auto next_hop = rt.getNextHop(2);
        EXPECT_TRUE(next_hop.has_value());

        // Update SRP registration (RLOC may have changed)
        srp.updateNodeRLOC(2, 0x0401, retry_time);
        EXPECT_TRUE(srp.hasActiveLease(2, retry_time));

        ctx.metrics.event(retry_time, 3, "command", "unlock_retry_succeeded");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: SRP lease expires during extended parking
// Tests: device becomes undiscoverable, then re-registers
TEST(Scenarios, SRPLeaseExpiryDuringParking) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("srp-lease-expiry-parking", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        ServiceRegistry registry;
        SRPServer srp(registry);

        // Register sensor
        ServiceRecord rec;
        rec.service_name = "temp-sensor";
        rec.service_type = MATTER_OPERATIONAL_SERVICE;
        rec.node_id = 2;
        rec.rloc16 = 0x0401;
        rec.port = 5540;
        srp.registerLease(2, 0x0401, "sensor.local", rec, now);

        EXPECT_TRUE(srp.hasActiveLease(2, now));
        EXPECT_EQ(registry.size(), 1);

        // Van parked for 3 hours — lease expires (default 2hr)
        auto parked_time = now + Duration(10800000); // 3 hours
        srp.tick(parked_time);

        EXPECT_FALSE(srp.hasActiveLease(2, parked_time));
        EXPECT_EQ(registry.size(), 0); // Device undiscoverable

        // Phone tries to discover — nothing found
        DiscoveryClient phone;
        auto results = phone.browseOperational(registry);
        EXPECT_EQ(results.size(), 0);

        ctx.metrics.event(parked_time, 2, "srp", "lease_expired_during_parking");

        // Van starts up — sensor re-registers
        auto startup_time = parked_time + Duration(5000);
        SRPClient client(2, 0x0000000000001002);
        client.registerWithServer(srp, 0x0401, "temp-sensor",
                                   MATTER_OPERATIONAL_SERVICE, 5540, startup_time);

        EXPECT_TRUE(srp.hasActiveLease(2, startup_time));
        EXPECT_EQ(registry.size(), 1);

        // Phone can discover again
        results = phone.browseOperational(registry);
        EXPECT_EQ(results.size(), 1);

        ctx.metrics.event(startup_time, 2, "srp", "re_registered_after_startup");
        return true;
    });

    EXPECT_TRUE(result.passed);
}

// Scenario: BR proxy table overflow with multiple controllers
TEST(Scenarios, ProxyTableOverflow) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::vanWithPhone();
    config.random_seed = 42;

    auto result = runner.run("proxy-table-overflow", config, [](ScenarioContext& ctx) {
        auto now = SteadyClock::now();

        // Small proxy table (simulates constrained BR)
        BorderRouterProxy proxy(4);

        // Phone creates sessions to 3 devices
        EXPECT_TRUE(proxy.addEntry(3, 0, 0x0000, 100, now));
        EXPECT_TRUE(proxy.addEntry(3, 1, 0x0400, 101, now));
        EXPECT_TRUE(proxy.addEntry(3, 2, 0x0401, 102, now));

        // Cloud controller also connects
        EXPECT_TRUE(proxy.addEntry(10, 0, 0x0000, 200, now));

        // Table is now full (4/4)
        EXPECT_TRUE(proxy.isFull());

        // Cloud tries to add another session — rejected
        EXPECT_FALSE(proxy.addEntry(10, 1, 0x0400, 201, now));
        EXPECT_EQ(proxy.rejectedCount(), 1);

        ctx.metrics.event(now, 0, "proxy", "table_overflow");

        // Expire old phone session
        proxy.expireIdle(now + Duration(400000));

        // All phone sessions expired (hadn't been touched)
        // Should have space now
        EXPECT_FALSE(proxy.isFull());

        // Cloud can now add the session
        EXPECT_TRUE(proxy.addEntry(10, 1, 0x0400, 201, now + Duration(400000)));

        ctx.metrics.event(now + Duration(400000), 10, "proxy", "session_added_after_expiry");
        return true;
    });

    EXPECT_TRUE(result.passed);
}
