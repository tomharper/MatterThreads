#include <gtest/gtest.h>
#include "thread/PowerManager.h"
#include "net/SelfHealing.h"

using namespace mt;

TEST(PowerManager, InitialState) {
    PowerManager pm;
    EXPECT_EQ(pm.systemState(), PowerState::EngineOn);
    EXPECT_EQ(pm.registeredNodeCount(), 0);
    EXPECT_EQ(pm.nodesShutdownGracefully(), 0);
    EXPECT_EQ(pm.nodesHardCutoff(), 0);
}

TEST(PowerManager, RegisterNodes) {
    PowerManager pm;
    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);
    EXPECT_EQ(pm.registeredNodeCount(), 3);

    // All nodes should be On
    EXPECT_TRUE(pm.isNodeOperational(0));
    EXPECT_TRUE(pm.isNodeOperational(1));
    EXPECT_TRUE(pm.isNodeOperational(2));

    // Duplicate registration is ignored
    pm.registerNode(0, ShutdownPriority::BorderRouter);
    EXPECT_EQ(pm.registeredNodeCount(), 3);
}

TEST(PowerManager, ShutdownTransition) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    auto result = pm.initiateShutdown(now, Duration(60000));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(pm.systemState(), PowerState::ShuttingDown);

    // Advance through the full shutdown window
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    EXPECT_EQ(pm.systemState(), PowerState::Off);
    EXPECT_FALSE(pm.isNodeOperational(0));
    EXPECT_FALSE(pm.isNodeOperational(1));
    EXPECT_FALSE(pm.isNodeOperational(2));
}

TEST(PowerManager, PriorityOrdering) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    std::vector<NodeId> shutdown_order;
    auto cb = [&shutdown_order](NodeId id, TimePoint) -> Result<void> {
        shutdown_order.push_back(id);
        return Result<void>::success();
    };
    pm.setNodeShutdownCallback(0, cb);
    pm.setNodeShutdownCallback(1, cb);
    pm.setNodeShutdownCallback(2, cb);

    pm.initiateShutdown(now, Duration(60000));

    // Tick through the full window
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    // Sensor (2) should shut down first, then Relay (1), then BR (0)
    ASSERT_EQ(shutdown_order.size(), 3);
    EXPECT_EQ(shutdown_order[0], 2); // Sensor first
    EXPECT_EQ(shutdown_order[1], 1); // Relay second
    EXPECT_EQ(shutdown_order[2], 0); // BR last
}

TEST(PowerManager, ShutdownCallbacks) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(2, ShutdownPriority::Sensor);

    bool sensor_cleaned = false;
    bool br_cleaned = false;

    pm.setNodeShutdownCallback(2, [&sensor_cleaned](NodeId, TimePoint) -> Result<void> {
        sensor_cleaned = true;
        return Result<void>::success();
    });
    pm.setNodeShutdownCallback(0, [&br_cleaned](NodeId, TimePoint) -> Result<void> {
        br_cleaned = true;
        return Result<void>::success();
    });

    pm.initiateShutdown(now, Duration(60000));
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    EXPECT_TRUE(sensor_cleaned);
    EXPECT_TRUE(br_cleaned);
    EXPECT_EQ(pm.nodesShutdownGracefully(), 2);
}

TEST(PowerManager, HardCutoff) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    pm.initiateShutdown(now, Duration(90000)); // 90s window

    // Only tick to 15s — sensor may have shut down but not relay/BR
    for (int i = 1; i <= 15; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    // Force hard cutoff at 15s
    auto result = pm.hardCutoff(now + Duration(15000));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(pm.systemState(), PowerState::Off);

    // At least some nodes should have been hard-cutoff
    EXPECT_GT(pm.nodesHardCutoff(), 0);

    // All nodes must be off
    EXPECT_FALSE(pm.isNodeOperational(0));
    EXPECT_FALSE(pm.isNodeOperational(1));
    EXPECT_FALSE(pm.isNodeOperational(2));
}

TEST(PowerManager, BootSequence) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    // Shut down first
    pm.initiateShutdown(now, Duration(60000));
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }
    EXPECT_EQ(pm.systemState(), PowerState::Off);

    // Boot up
    auto boot_time = now + Duration(70000);
    auto result = pm.initiateBoot(boot_time);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(pm.systemState(), PowerState::Booting);

    // Tick through boot sequence
    for (int i = 1; i <= 15; ++i) {
        pm.tick(boot_time + Duration(i * 1000));
    }

    EXPECT_EQ(pm.systemState(), PowerState::EngineOn);
    EXPECT_TRUE(pm.isNodeOperational(0));
    EXPECT_TRUE(pm.isNodeOperational(1));
    EXPECT_TRUE(pm.isNodeOperational(2));
}

TEST(PowerManager, BootCallbacksReverseOrder) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    // Shut down
    pm.initiateShutdown(now, Duration(60000));
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    // Track boot order
    std::vector<NodeId> boot_order;
    auto cb = [&boot_order](NodeId id, TimePoint) -> Result<void> {
        boot_order.push_back(id);
        return Result<void>::success();
    };
    pm.setNodeBootCallback(0, cb);
    pm.setNodeBootCallback(1, cb);
    pm.setNodeBootCallback(2, cb);

    auto boot_time = now + Duration(70000);
    pm.initiateBoot(boot_time);
    for (int i = 1; i <= 15; ++i) {
        pm.tick(boot_time + Duration(i * 1000));
    }

    // BR (0) boots first, then Relay (1), then Sensor (2)
    ASSERT_EQ(boot_order.size(), 3);
    EXPECT_EQ(boot_order[0], 0); // BR first
    EXPECT_EQ(boot_order[1], 1); // Relay second
    EXPECT_EQ(boot_order[2], 2); // Sensor last
}

TEST(PowerManager, ShutdownRemaining) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);

    pm.initiateShutdown(now, Duration(60000));
    EXPECT_EQ(pm.shutdownRemaining(now).count(), 60000);
    EXPECT_EQ(pm.shutdownRemaining(now + Duration(30000)).count(), 30000);
    EXPECT_EQ(pm.shutdownRemaining(now + Duration(60000)).count(), 0);
}

TEST(PowerManager, EventHistory) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(2, ShutdownPriority::Sensor);

    std::vector<PowerEvent> events;
    pm.onPowerEvent([&events](const PowerEventRecord& rec) {
        events.push_back(rec.event);
    });

    pm.initiateShutdown(now, Duration(60000));
    for (int i = 1; i <= 60; ++i) {
        pm.tick(now + Duration(i * 1000));
    }

    // Should have: ShutdownInitiated, then for each node: NodeShutdownStarted + NodeShutdownComplete, then SystemOff
    EXPECT_GE(events.size(), 6);

    EXPECT_EQ(events.front(), PowerEvent::ShutdownInitiated);
    EXPECT_EQ(events.back(), PowerEvent::SystemOff);

    // Check that NodeShutdownStarted and NodeShutdownComplete appear
    size_t started = 0, completed = 0;
    for (auto ev : events) {
        if (ev == PowerEvent::NodeShutdownStarted) started++;
        if (ev == PowerEvent::NodeShutdownComplete) completed++;
    }
    EXPECT_EQ(started, 2);
    EXPECT_EQ(completed, 2);
}

TEST(PowerManager, DoubleShutdownFails) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);

    auto r1 = pm.initiateShutdown(now, Duration(60000));
    EXPECT_TRUE(r1.ok());

    auto r2 = pm.initiateShutdown(now, Duration(60000));
    EXPECT_FALSE(r2.ok());
}

TEST(PowerManager, BootWhileOnFails) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);

    auto result = pm.initiateBoot(now);
    EXPECT_FALSE(result.ok());
}

TEST(PowerManager, BatteryExpiresAutoHardCutoff) {
    PowerManager pm;
    auto now = SteadyClock::now();

    pm.registerNode(0, ShutdownPriority::BorderRouter);
    pm.registerNode(1, ShutdownPriority::Relay);
    pm.registerNode(2, ShutdownPriority::Sensor);

    // Very short battery — 5s total, not enough for all nodes
    pm.initiateShutdown(now, Duration(5000));

    // Tick past the battery life
    pm.tick(now + Duration(6000));

    // Battery expired → hard cutoff
    EXPECT_EQ(pm.systemState(), PowerState::Off);
    EXPECT_GT(pm.nodesHardCutoff(), 0);
}

TEST(PowerManager, SelfHealingPowerEvents) {
    SelfHealingEngine healing;
    std::vector<HealingEvent> events;
    healing.onHealingEvent([&events](const HealingRecord& r) { events.push_back(r.event); });

    auto now = SteadyClock::now();
    healing.onSystemPowerDown(now);
    healing.onSystemPowerUp(now + Duration(60000));

    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0], HealingEvent::PowerDown);
    EXPECT_EQ(events[1], HealingEvent::PowerUp);
}
