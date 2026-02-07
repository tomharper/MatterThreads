#include <gtest/gtest.h>
#include "thread/BorderRouter.h"

using namespace mt;

TEST(BorderRouter, AddAndResolve) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    EXPECT_TRUE(proxy.addEntry(3, 2, 0x0401, 100, now));
    EXPECT_EQ(proxy.size(), 1);

    auto rloc = proxy.resolveDevice(2);
    ASSERT_TRUE(rloc.has_value());
    EXPECT_EQ(*rloc, 0x0401);
}

TEST(BorderRouter, TableFullRejects) {
    BorderRouterProxy proxy(2); // max 2 entries
    auto now = SteadyClock::now();

    EXPECT_TRUE(proxy.addEntry(3, 0, 0x0000, 100, now));
    EXPECT_TRUE(proxy.addEntry(3, 1, 0x0400, 101, now));
    EXPECT_FALSE(proxy.addEntry(3, 2, 0x0401, 102, now));

    EXPECT_EQ(proxy.rejectedCount(), 1);
    EXPECT_TRUE(proxy.isFull());
}

TEST(BorderRouter, UpdateExistingEntry) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 2, 0x0401, 100, now);
    EXPECT_EQ(proxy.size(), 1);

    // Same controller+device, different RLOC
    proxy.addEntry(3, 2, 0x0005, 100, now);
    EXPECT_EQ(proxy.size(), 1); // Should update, not add

    auto rloc = proxy.resolveDevice(2);
    ASSERT_TRUE(rloc.has_value());
    EXPECT_EQ(*rloc, 0x0005);
}

TEST(BorderRouter, RemoveDevice) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 2, 0x0401, 100, now);
    proxy.addEntry(4, 2, 0x0401, 101, now); // different controller, same device
    EXPECT_EQ(proxy.size(), 2);

    proxy.removeDevice(2);
    EXPECT_EQ(proxy.size(), 0);
}

TEST(BorderRouter, RemoveController) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 0, 0x0000, 100, now);
    proxy.addEntry(3, 1, 0x0400, 101, now);
    proxy.addEntry(3, 2, 0x0401, 102, now);
    EXPECT_EQ(proxy.size(), 3);

    proxy.removeController(3);
    EXPECT_EQ(proxy.size(), 0);
}

TEST(BorderRouter, ExpireIdleSessions) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 2, 0x0401, 100, now);
    EXPECT_EQ(proxy.size(), 1);

    // Not expired yet
    proxy.expireIdle(now + Duration(100000));
    EXPECT_EQ(proxy.size(), 1);

    // Expired (default idle timeout is 300000ms = 5min)
    proxy.expireIdle(now + Duration(400000));
    EXPECT_EQ(proxy.size(), 0);
    EXPECT_EQ(proxy.expiredCount(), 1);
}

TEST(BorderRouter, TouchSessionPreventsExpiry) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 2, 0x0401, 100, now);

    // Touch at 4 minutes
    proxy.touchSession(3, 2, now + Duration(240000));

    // Check at 5 minutes — should NOT expire because we touched at 4min
    proxy.expireIdle(now + Duration(300000));
    EXPECT_EQ(proxy.size(), 1);

    // Check at 10 minutes — now expired (6min since last touch)
    proxy.expireIdle(now + Duration(600000));
    EXPECT_EQ(proxy.size(), 0);
}

TEST(BorderRouter, UpdateDeviceRLOC) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 2, 0x0401, 100, now);
    proxy.addEntry(4, 2, 0x0401, 101, now);

    // Device re-attaches with new RLOC
    proxy.updateDeviceRLOC(2, 0x0005);

    // Both entries should be updated
    auto rloc = proxy.resolveDevice(2);
    ASSERT_TRUE(rloc.has_value());
    EXPECT_EQ(*rloc, 0x0005);
}

TEST(BorderRouter, ResolveUnknownDevice) {
    BorderRouterProxy proxy;
    auto result = proxy.resolveDevice(99);
    EXPECT_FALSE(result.has_value());
}

TEST(BorderRouter, RemoveSpecificEntry) {
    BorderRouterProxy proxy;
    auto now = SteadyClock::now();

    proxy.addEntry(3, 0, 0x0000, 100, now);
    proxy.addEntry(3, 1, 0x0400, 101, now);
    proxy.addEntry(3, 2, 0x0401, 102, now);

    proxy.removeEntry(3, 1);
    EXPECT_EQ(proxy.size(), 2);

    // Device 1 should be gone
    EXPECT_FALSE(proxy.resolveDevice(1).has_value());
    // Others still there
    EXPECT_TRUE(proxy.resolveDevice(0).has_value());
    EXPECT_TRUE(proxy.resolveDevice(2).has_value());
}
