#include <gtest/gtest.h>
#include "net/SelfHealing.h"

using namespace mt;

TEST(SelfHealing, NeighborHeardTracked) {
    SelfHealingEngine engine;
    auto now = SteadyClock::now();

    engine.onNeighborHeard(1, 1, now);
    EXPECT_TRUE(engine.isNeighborReachable(1));
    EXPECT_EQ(engine.unreachableNeighbors().size(), 0);
}

TEST(SelfHealing, UnknownNeighborUnreachable) {
    SelfHealingEngine engine;
    EXPECT_FALSE(engine.isNeighborReachable(99));
}

TEST(SelfHealing, NeighborLostAfterTimeout) {
    SelfHealingEngine engine;
    engine.setNeighborTimeout(Duration(100));

    auto now = SteadyClock::now();
    engine.onNeighborHeard(1, 1, now);
    EXPECT_TRUE(engine.isNeighborReachable(1));

    // Create a routing table for tick
    RoutingTable routing;
    routing.setOwnRouterId(0);

    // Tick well after timeout
    engine.tick(now + Duration(200), routing);
    EXPECT_FALSE(engine.isNeighborReachable(1));

    auto unreachable = engine.unreachableNeighbors();
    ASSERT_EQ(unreachable.size(), 1);
    EXPECT_EQ(unreachable[0], 1);
}

TEST(SelfHealing, NeighborRecovery) {
    SelfHealingEngine engine;
    engine.setNeighborTimeout(Duration(100));

    auto now = SteadyClock::now();
    engine.onNeighborHeard(1, 1, now);

    RoutingTable routing;
    routing.setOwnRouterId(0);

    // Timeout
    engine.tick(now + Duration(200), routing);
    EXPECT_FALSE(engine.isNeighborReachable(1));

    // Neighbor comes back
    engine.onNeighborHeard(1, 1, now + Duration(300));
    EXPECT_TRUE(engine.isNeighborReachable(1));
}

TEST(SelfHealing, HealingEventCallback) {
    SelfHealingEngine engine;
    engine.setNeighborTimeout(Duration(100));

    std::vector<HealingEvent> events;
    engine.onHealingEvent([&events](const HealingRecord& record) {
        events.push_back(record.event);
    });

    auto now = SteadyClock::now();
    engine.onNeighborHeard(1, 1, now);

    RoutingTable routing;
    routing.setOwnRouterId(0);

    // Trigger neighbor lost
    engine.tick(now + Duration(200), routing);
    ASSERT_GE(events.size(), 1);
    EXPECT_EQ(events[0], HealingEvent::NeighborLost);
}

TEST(SelfHealing, BackhaulLostAndRestored) {
    SelfHealingEngine engine;

    std::vector<HealingEvent> events;
    engine.onHealingEvent([&events](const HealingRecord& record) {
        events.push_back(record.event);
    });

    auto now = SteadyClock::now();

    engine.onBackhaulLost(now);
    EXPECT_FALSE(engine.backhaulState().connected);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], HealingEvent::BackhaulLost);

    engine.onBackhaulRestored(now + Duration(5000));
    EXPECT_TRUE(engine.backhaulState().connected);
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[1], HealingEvent::BackhaulRestored);
}

TEST(SelfHealing, BackhaulDowntime) {
    BackhaulState state;
    auto now = SteadyClock::now();

    EXPECT_EQ(state.downtime(now).count(), 0);

    state.markDisconnected(now);
    EXPECT_EQ(state.downtime(now + Duration(3000)).count(), 3000);

    state.markConnected(now + Duration(5000));
    EXPECT_EQ(state.downtime(now + Duration(6000)).count(), 0);
}

TEST(SelfHealing, BackhaulBufferLimit) {
    BackhaulState state;
    state.buffered_messages = BackhaulState::MAX_BUFFER - 1;
    EXPECT_LT(state.buffered_messages, BackhaulState::MAX_BUFFER);
    state.buffered_messages++;
    EXPECT_EQ(state.buffered_messages, BackhaulState::MAX_BUFFER);
}

TEST(SelfHealing, SubscriptionEvents) {
    SelfHealingEngine engine;

    std::vector<HealingEvent> events;
    engine.onHealingEvent([&events](const HealingRecord& record) {
        events.push_back(record.event);
    });

    auto now = SteadyClock::now();

    engine.onSubscriptionDropped(2, 100, now);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], HealingEvent::SubscriptionDropped);

    engine.onSubscriptionRecovered(2, 100, now + Duration(1000));
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[1], HealingEvent::SubscriptionRecovered);
}

TEST(SelfHealing, NodeReattached) {
    SelfHealingEngine engine;

    std::vector<HealingEvent> events;
    engine.onHealingEvent([&events](const HealingRecord& record) {
        events.push_back(record.event);
    });

    auto now = SteadyClock::now();
    engine.onNodeReattached(1, now);

    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], HealingEvent::NodeReattached);
}

TEST(SelfHealing, HistoryAccumulates) {
    SelfHealingEngine engine;
    engine.setNeighborTimeout(Duration(100));

    auto now = SteadyClock::now();
    engine.onNeighborHeard(1, 1, now);
    engine.onNeighborHeard(2, 2, now);

    RoutingTable routing;
    routing.setOwnRouterId(0);

    // Both time out
    engine.tick(now + Duration(200), routing);

    // Both recover
    engine.onNeighborHeard(1, 1, now + Duration(300));
    engine.onNeighborHeard(2, 2, now + Duration(300));

    // Should have: 2 NeighborLost + 2 NodeReattached = 4+ events
    EXPECT_GE(engine.history().size(), 4);
}

TEST(SelfHealing, HealingEventToString) {
    EXPECT_STREQ(healingEventToString(HealingEvent::NeighborLost), "NeighborLost");
    EXPECT_STREQ(healingEventToString(HealingEvent::PartitionDetected), "PartitionDetected");
    EXPECT_STREQ(healingEventToString(HealingEvent::RouteRecalculated), "RouteRecalculated");
    EXPECT_STREQ(healingEventToString(HealingEvent::SubscriptionDropped), "SubscriptionDropped");
    EXPECT_STREQ(healingEventToString(HealingEvent::SubscriptionRecovered), "SubscriptionRecovered");
    EXPECT_STREQ(healingEventToString(HealingEvent::NodeReattached), "NodeReattached");
    EXPECT_STREQ(healingEventToString(HealingEvent::ServiceReregistered), "ServiceReregistered");
    EXPECT_STREQ(healingEventToString(HealingEvent::BackhaulLost), "BackhaulLost");
    EXPECT_STREQ(healingEventToString(HealingEvent::BackhaulRestored), "BackhaulRestored");
}
