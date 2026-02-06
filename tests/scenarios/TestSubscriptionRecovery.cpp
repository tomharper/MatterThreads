#include <gtest/gtest.h>
#include "ScenarioRunner.h"
#include "matter/SubscriptionManager.h"
#include "matter/DataModel.h"

using namespace mt;

TEST(Scenarios, SubscriptionDropAndRecovery) {
    ScenarioRunner runner;

    ScenarioConfig config;
    config.topology = MeshTopology::fullyConnected();
    config.fault_plan = FaultPlan::subscriptionStress();
    config.random_seed = 42;

    auto result = runner.run("subscription-recovery", config, [](ScenarioContext& ctx) {
        SubscriptionManager sub_mgr;

        int report_count = 0;
        sub_mgr.setReportCallback([&](SubscriptionId, const std::vector<AttributePath>&) {
            ++report_count;
        });

        auto now = SteadyClock::now();

        // Create subscription
        AttributePath path{1, Clusters::OnOff, Attributes::OnOff_OnOff};
        auto sub_id = sub_mgr.createSubscription(1, 2, {path},
                                                    Duration(5000), Duration(60000));

        EXPECT_EQ(sub_mgr.activeCount(), 1);

        // Tick at T=0: should send initial report
        sub_mgr.tick(now);
        EXPECT_GE(report_count, 1);

        // Simulate link failure: miss liveness checks
        for (int i = 0; i < 3; ++i) {
            sub_mgr.onLivenessCheckFailed(sub_id);
        }

        // Subscription should be dropped after 3 missed checks
        EXPECT_EQ(sub_mgr.activeCount(), 0);

        ctx.metrics.increment("subscription_drops", 1);
        ctx.metrics.event(now, 0, "subscribe", "subscription_dropped",
                           "Missed 3 liveness checks");

        // Recovery: create new subscription
        sub_mgr.createSubscription(1, 2, {path}, Duration(5000), Duration(60000));
        EXPECT_EQ(sub_mgr.activeCount(), 1);

        ctx.metrics.event(now + Duration(120000), 0, "subscribe", "subscription_recovered",
                           "New subscription established");

        return true;
    });

    EXPECT_TRUE(result.passed);
}
