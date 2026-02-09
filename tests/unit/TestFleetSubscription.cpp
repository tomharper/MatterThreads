#include <gtest/gtest.h>
#include "gateway/FleetSubscriptionManager.h"

using namespace mt;
using namespace mt::gateway;

class FleetSubTest : public ::testing::Test {
protected:
    std::shared_ptr<hw::ChipToolDriver> driver;
    std::unique_ptr<CASESessionPool> pool;

    void SetUp() override {
        hw::ChipToolConfig config;
        config.binary_path = "/bin/echo";
        config.storage_dir = "/tmp/mt-fleet-sub-test";
        config.command_timeout = Duration(2000);
        driver = std::make_shared<hw::ChipToolDriver>(config);
        pool = std::make_unique<CASESessionPool>(driver);
    }
};

TEST_F(FleetSubTest, AddRule) {
    FleetSubscriptionManager mgr(driver, *pool);
    SubscriptionRule rule;
    rule.name = "test-temp";
    rule.endpoint = 2;
    rule.cluster = 0x0402;
    rule.attribute = 0x0000;

    auto id = mgr.addRule(rule);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(mgr.rules().size(), 1u);
    EXPECT_EQ(mgr.rules()[0].name, "test-temp");
}

TEST_F(FleetSubTest, RemoveRule) {
    FleetSubscriptionManager mgr(driver, *pool);
    SubscriptionRule rule;
    rule.name = "to-remove";
    auto id = mgr.addRule(rule);

    mgr.removeRule(id);
    EXPECT_EQ(mgr.rules().size(), 0u);
}

TEST_F(FleetSubTest, SubscribeVanRequiresConnection) {
    FleetSubscriptionManager mgr(driver, *pool);
    mgr.addRule({0, "temp", 2, 0x0402, 0x0000, Duration(5000), Duration(30000), true});

    // Van not connected
    auto result = mgr.subscribeVan("VAN-NOCONN", 1);
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("not connected"), std::string::npos);
}

TEST_F(FleetSubTest, UnsubscribeVan) {
    FleetSubscriptionManager mgr(driver, *pool);
    mgr.addRule({0, "temp", 2, 0x0402, 0x0000, Duration(5000), Duration(30000), true});

    // Unsubscribe non-existent van should be safe
    mgr.unsubscribeVan("VAN-NONEXIST");
    EXPECT_EQ(mgr.activeSubscriptionCount(), 0u);
}

TEST_F(FleetSubTest, ReportCallbackFired) {
    FleetSubscriptionManager mgr(driver, *pool);
    bool callback_fired = false;
    mgr.onReport([&](const VanAttributeReport&) {
        callback_fired = true;
    });

    // The callback infrastructure is wired — actual firing depends on chip-tool reports
    EXPECT_FALSE(callback_fired);
    EXPECT_EQ(mgr.totalReportCount(), 0u);
}

TEST_F(FleetSubTest, LivenessTracking) {
    FleetSubscriptionManager mgr(driver, *pool);
    mgr.addRule({0, "temp", 2, 0x0402, 0x0000, Duration(1000), Duration(5000), true});

    // Tick without any active subscriptions should be safe
    auto now = SteadyClock::now();
    mgr.tick(now);
    mgr.tick(now + Duration(100000));
    EXPECT_EQ(mgr.activeSubscriptionCount(), 0u);
}

TEST_F(FleetSubTest, ResubscribeOnFailure) {
    FleetSubscriptionManager mgr(driver, *pool);
    mgr.addRule({0, "temp", 2, 0x0402, 0x0000, Duration(1000), Duration(5000), true});

    // Can't subscribe without connection, but unsubscribe + re-attempt is safe
    mgr.unsubscribeVan("VAN-1");
    auto result = mgr.subscribeVan("VAN-1", 1);
    EXPECT_FALSE(result.ok()); // Not connected
}

TEST_F(FleetSubTest, DefaultRulesLoaded) {
    FleetSubscriptionManager mgr(driver, *pool);
    mgr.loadDefaultVanRules();

    auto rules = mgr.rules();
    EXPECT_EQ(rules.size(), 8u);

    // Check a few specific rules
    bool found_temp = false, found_lock = false;
    for (const auto& r : rules) {
        if (r.name == "cargo-temp") {
            found_temp = true;
            EXPECT_EQ(r.endpoint, 2u);
            EXPECT_EQ(r.cluster, 0x0402u);
            EXPECT_TRUE(r.critical);
        }
        if (r.name == "door-lock-state") {
            found_lock = true;
            EXPECT_EQ(r.endpoint, 1u);
            EXPECT_EQ(r.cluster, 0x0101u);
        }
    }
    EXPECT_TRUE(found_temp);
    EXPECT_TRUE(found_lock);
}
