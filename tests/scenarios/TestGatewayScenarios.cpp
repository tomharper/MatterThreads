#include <gtest/gtest.h>
#include "gateway/GatewayServer.h"
#include "gateway/VanRegistry.h"
#include "gateway/SessionPool.h"
#include "gateway/FleetSubscriptionManager.h"
#include "gateway/CommandRelay.h"
#include "gateway/OfflineBuffer.h"
#include "gateway/FabricManager.h"

using namespace mt;
using namespace mt::gateway;

// ── Full-stack fixture ──────────────────────────────────────────────────────

class GatewayScenarioTest : public ::testing::Test {
protected:
    std::shared_ptr<hw::ChipToolDriver> driver;
    VanRegistry registry;
    std::unique_ptr<CASESessionPool> sessions;
    std::unique_ptr<FleetSubscriptionManager> subscriptions;
    std::unique_ptr<CommandRelay> commands;
    OfflineBuffer buffer;
    FabricManager fabrics;

    void SetUp() override {
        hw::ChipToolConfig config;
        config.binary_path = "/bin/echo";
        config.storage_dir = "/tmp/mt-gw-scenario";
        config.command_timeout = Duration(2000);
        driver = std::make_shared<hw::ChipToolDriver>(config);
        sessions = std::make_unique<CASESessionPool>(driver);
        subscriptions = std::make_unique<FleetSubscriptionManager>(driver, *sessions);
        commands = std::make_unique<CommandRelay>(driver, *sessions);
    }
};

// ── Scenario 1: Fleet commissioning lifecycle ───────────────────────────────

TEST_F(GatewayScenarioTest, FleetCommissioningLifecycle) {
    // Create a tenant and issue credentials
    auto tenant_result = fabrics.createTenant("Acme Logistics");
    ASSERT_TRUE(tenant_result.ok());
    TenantId tenant_id = *tenant_result;

    // Register 3 vans
    for (int i = 1; i <= 3; ++i) {
        VanRegistration reg;
        reg.van_id = "VAN-" + std::to_string(i);
        reg.device_id = static_cast<uint64_t>(1000 + i);
        reg.tenant_id = tenant_id;
        reg.name = "Delivery Van #" + std::to_string(i);
        ASSERT_TRUE(registry.registerVan(reg).ok());
    }

    EXPECT_EQ(registry.size(), 3u);

    // Issue NOCs for all vans
    for (int i = 1; i <= 3; ++i) {
        auto noc = fabrics.issueNOC(tenant_id, "VAN-" + std::to_string(i),
                                     static_cast<uint64_t>(1000 + i));
        ASSERT_TRUE(noc.ok());
        EXPECT_TRUE(fabrics.canAccess(tenant_id, "VAN-" + std::to_string(i)));
    }

    EXPECT_EQ(fabrics.credentialCount(tenant_id), 3u);

    // All vans should start as Registered
    for (int i = 1; i <= 3; ++i) {
        auto* van = registry.getVan("VAN-" + std::to_string(i));
        ASSERT_NE(van, nullptr);
        EXPECT_EQ(van->state, VanState::Registered);
    }
}

// ── Scenario 2: Subscription rules apply to fleet ───────────────────────────

TEST_F(GatewayScenarioTest, SubscriptionRulesFleetWide) {
    // Load default rules
    subscriptions->loadDefaultVanRules();
    auto rules = subscriptions->rules();
    ASSERT_EQ(rules.size(), 8u);

    // Verify critical rules
    int critical_count = 0;
    for (const auto& rule : rules) {
        if (rule.critical) ++critical_count;
    }
    EXPECT_EQ(critical_count, 5);  // cargo-temp, door-contact, door-lock-state, occupancy, reefer-setpoint

    // Add a custom rule
    SubscriptionRule custom;
    custom.name = "gps-position";
    custom.endpoint = 10;
    custom.cluster = 0xFFF0;
    custom.attribute = 0x0000;
    custom.min_interval = Duration(1000);
    custom.max_interval = Duration(5000);
    custom.critical = true;
    subscriptions->addRule(custom);

    EXPECT_EQ(subscriptions->rules().size(), 9u);

    // Attempting to subscribe a disconnected van should fail
    VanRegistration reg;
    reg.van_id = "VAN-SUB";
    reg.device_id = 2001;
    registry.registerVan(reg);

    auto result = subscriptions->subscribeVan("VAN-SUB", 2001);
    EXPECT_FALSE(result.ok());  // Not connected
}

// ── Scenario 3: Offline buffer accumulation and drain ───────────────────────

TEST_F(GatewayScenarioTest, OfflineBufferAccumulationAndDrain) {
    // Simulate events from multiple vans going offline
    auto now = TimePoint(Duration(1000000));

    buffer.push("VAN-A", "temp_alert", {{"temp_c", 42.5}}, now);
    buffer.push("VAN-A", "door_open", {{"door", "cargo"}}, now + Duration(1000));
    buffer.push("VAN-B", "battery_low", {{"voltage", 3.2}}, now + Duration(2000));
    buffer.push("VAN-A", "temp_alert", {{"temp_c", 44.1}}, now + Duration(3000));
    buffer.push("VAN-C", "occupancy", {{"detected", true}}, now + Duration(4000));

    EXPECT_EQ(buffer.eventCount("VAN-A"), 3u);
    EXPECT_EQ(buffer.eventCount("VAN-B"), 1u);
    EXPECT_EQ(buffer.eventCount("VAN-C"), 1u);
    EXPECT_EQ(buffer.totalEventCount(), 5u);

    // Drain VAN-A events since a specific sequence
    auto van_a_events = buffer.drain("VAN-A", 0);
    EXPECT_EQ(van_a_events.size(), 3u);
    EXPECT_EQ(van_a_events[0].event_type, "temp_alert");
    EXPECT_EQ(van_a_events[2].event_type, "temp_alert");

    // Drain since sequence 2 — VAN-A events have seq 1, 2, 4 (seq 3=VAN-B, 5=VAN-C)
    // since=2 returns events with seq > 2, so just seq 4
    auto partial = buffer.drain("VAN-A", 2);
    EXPECT_EQ(partial.size(), 1u);

    // Fleet-wide drain
    auto all = buffer.drainAll(0);
    EXPECT_EQ(all.size(), 5u);

    // Eviction by age
    buffer.evict(Duration(2500), now + Duration(4000));
    EXPECT_LT(buffer.totalEventCount(), 5u);
}

// ── Scenario 4: Multi-tenant isolation ──────────────────────────────────────

TEST_F(GatewayScenarioTest, MultiTenantIsolation) {
    // Create two tenants
    auto t1 = fabrics.createTenant("Acme");
    auto t2 = fabrics.createTenant("Globex");
    ASSERT_TRUE(t1.ok());
    ASSERT_TRUE(t2.ok());

    TenantId acme = *t1;
    TenantId globex = *t2;

    // Register vans for each tenant
    VanRegistration va;
    va.van_id = "ACME-1";
    va.device_id = 100;
    va.tenant_id = acme;
    registry.registerVan(va);

    VanRegistration vg;
    vg.van_id = "GLOBEX-1";
    vg.device_id = 200;
    vg.tenant_id = globex;
    registry.registerVan(vg);

    // Issue NOCs
    fabrics.issueNOC(acme, "ACME-1", 100);
    fabrics.issueNOC(globex, "GLOBEX-1", 200);

    // Access checks
    EXPECT_TRUE(fabrics.canAccess(acme, "ACME-1"));
    EXPECT_FALSE(fabrics.canAccess(acme, "GLOBEX-1"));
    EXPECT_TRUE(fabrics.canAccess(globex, "GLOBEX-1"));
    EXPECT_FALSE(fabrics.canAccess(globex, "ACME-1"));

    // Tenant-scoped van listing
    auto acme_vans = registry.listVansByTenant(acme);
    EXPECT_EQ(acme_vans.size(), 1u);
    EXPECT_EQ(acme_vans[0].van_id, "ACME-1");

    auto globex_vans = registry.listVansByTenant(globex);
    EXPECT_EQ(globex_vans.size(), 1u);
    EXPECT_EQ(globex_vans[0].van_id, "GLOBEX-1");

    // Unique fabric IDs
    auto* t1_info = fabrics.getTenant(acme);
    auto* t2_info = fabrics.getTenant(globex);
    ASSERT_NE(t1_info, nullptr);
    ASSERT_NE(t2_info, nullptr);
    EXPECT_NE(t1_info->fabric_id, t2_info->fabric_id);
}

// ── Scenario 5: Command relay on disconnected fleet ─────────────────────────

TEST_F(GatewayScenarioTest, CommandRelayDisconnectedFleet) {
    // Register a fleet of vans
    for (int i = 1; i <= 5; ++i) {
        VanRegistration reg;
        reg.van_id = "FLEET-" + std::to_string(i);
        reg.device_id = static_cast<uint64_t>(3000 + i);
        registry.registerVan(reg);
    }

    // None are connected — all commands should fail gracefully
    for (int i = 1; i <= 5; ++i) {
        std::string van_id = "FLEET-" + std::to_string(i);
        uint64_t device_id = static_cast<uint64_t>(3000 + i);

        // Lock command
        auto lock = commands->invoke(van_id, device_id, 1, 0x0101, 0x0000);
        ASSERT_TRUE(lock.ok());
        EXPECT_FALSE(lock->success);
        EXPECT_FALSE(lock->error_message.empty());

        // Read temperature
        auto temp = commands->readAttribute(van_id, device_id, 2, 0x0402, 0x0000);
        ASSERT_TRUE(temp.ok());
        EXPECT_FALSE(temp->success);
    }

    EXPECT_EQ(sessions->connectedVans().size(), 0u);
}

// ── Scenario 6: Van state transitions with events ───────────────────────────

TEST_F(GatewayScenarioTest, VanStateTransitionsWithEvents) {
    // Register a van
    VanRegistration reg;
    reg.van_id = "VAN-LIFECYCLE";
    reg.device_id = 5000;
    registry.registerVan(reg);

    auto now = TimePoint(Duration(0));

    // Initial state
    auto* van = registry.getVan("VAN-LIFECYCLE");
    ASSERT_NE(van, nullptr);
    EXPECT_EQ(van->state, VanState::Registered);

    // Transition to commissioning
    registry.setVanState("VAN-LIFECYCLE", VanState::Commissioning, now);
    van = registry.getVan("VAN-LIFECYCLE");
    EXPECT_EQ(van->state, VanState::Commissioning);

    // Transition to online (CASE session would be established)
    registry.setVanState("VAN-LIFECYCLE", VanState::Online, now + Duration(5000));
    van = registry.getVan("VAN-LIFECYCLE");
    EXPECT_EQ(van->state, VanState::Online);

    // Buffer events while online
    buffer.push("VAN-LIFECYCLE", "connected", {{"session", "CASE"}},
                now + Duration(5000));
    buffer.push("VAN-LIFECYCLE", "subscribed", {{"rules", 8}},
                now + Duration(6000));

    // Go offline
    registry.setVanState("VAN-LIFECYCLE", VanState::Offline, now + Duration(100000));
    buffer.push("VAN-LIFECYCLE", "disconnected", {{"reason", "timeout"}},
                now + Duration(100000));

    // Buffer events during offline period
    buffer.push("VAN-LIFECYCLE", "missed_report", {{"rule", "cargo-temp"}},
                now + Duration(110000));
    buffer.push("VAN-LIFECYCLE", "missed_report", {{"rule", "door-lock"}},
                now + Duration(120000));

    // All events should be buffered
    auto events = buffer.drain("VAN-LIFECYCLE", 0);
    EXPECT_EQ(events.size(), 5u);
    EXPECT_EQ(events[0].event_type, "connected");
    EXPECT_EQ(events[4].event_type, "missed_report");

    // Mark unreachable after too many failures
    registry.setVanState("VAN-LIFECYCLE", VanState::Unreachable, now + Duration(300000));
    van = registry.getVan("VAN-LIFECYCLE");
    EXPECT_EQ(van->state, VanState::Unreachable);
}
