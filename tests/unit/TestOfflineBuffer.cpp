#include <gtest/gtest.h>
#include "gateway/OfflineBuffer.h"

using namespace mt;
using namespace mt::gateway;

TEST(OfflineBuffer, PushAndDrain) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "attribute_report", {{"temp", 4.2}}, now);
    buf.push("VAN-1", "attribute_report", {{"temp", 4.3}}, now + Duration(1000));

    auto events = buf.drain("VAN-1");
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].event_type, "attribute_report");
    EXPECT_EQ(events[0].sequence_id, 1u);
    EXPECT_EQ(events[1].sequence_id, 2u);
}

TEST(OfflineBuffer, DrainSinceTimestamp) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "report", {{"a", 1}}, now);
    buf.push("VAN-1", "report", {{"b", 2}}, now + Duration(5000));
    buf.push("VAN-1", "report", {{"c", 3}}, now + Duration(10000));

    auto events = buf.drainSince("VAN-1", now + Duration(3000));
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].payload["b"], 2);
    EXPECT_EQ(events[1].payload["c"], 3);
}

TEST(OfflineBuffer, DrainSinceSequence) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "a", {}, now);
    buf.push("VAN-1", "b", {}, now);
    buf.push("VAN-1", "c", {}, now);

    auto events = buf.drain("VAN-1", 1); // Since seq 1 → get 2 and 3
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].event_type, "b");
    EXPECT_EQ(events[1].event_type, "c");
}

TEST(OfflineBuffer, PerVanCapacity) {
    OfflineBuffer buf(3, 100); // Max 3 per van
    auto now = SteadyClock::now();

    buf.push("VAN-1", "a", {}, now);
    buf.push("VAN-1", "b", {}, now);
    buf.push("VAN-1", "c", {}, now);
    buf.push("VAN-1", "d", {}, now); // Evicts "a"

    EXPECT_EQ(buf.eventCount("VAN-1"), 3u);
    auto events = buf.drain("VAN-1");
    EXPECT_EQ(events[0].event_type, "b"); // "a" was evicted
}

TEST(OfflineBuffer, TotalCapacity) {
    OfflineBuffer buf(100, 5); // Max 5 total
    auto now = SteadyClock::now();

    buf.push("VAN-1", "a", {}, now);
    buf.push("VAN-2", "b", {}, now);
    buf.push("VAN-3", "c", {}, now);
    buf.push("VAN-1", "d", {}, now);
    buf.push("VAN-2", "e", {}, now);
    buf.push("VAN-3", "f", {}, now); // Evicts oldest globally

    EXPECT_EQ(buf.totalEventCount(), 5u);
}

TEST(OfflineBuffer, ClearVan) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "a", {}, now);
    buf.push("VAN-2", "b", {}, now);

    buf.clear("VAN-1");
    EXPECT_EQ(buf.eventCount("VAN-1"), 0u);
    EXPECT_EQ(buf.eventCount("VAN-2"), 1u);
    EXPECT_EQ(buf.totalEventCount(), 1u);
}

TEST(OfflineBuffer, EvictByAge) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "old", {}, now - Duration(60000));
    buf.push("VAN-1", "new", {}, now);

    buf.evict(Duration(30000), now); // Evict events older than 30s

    EXPECT_EQ(buf.eventCount("VAN-1"), 1u);
    auto events = buf.drain("VAN-1");
    EXPECT_EQ(events[0].event_type, "new");
}

TEST(OfflineBuffer, DrainAll) {
    OfflineBuffer buf;
    auto now = SteadyClock::now();

    buf.push("VAN-1", "a", {}, now);
    buf.push("VAN-2", "b", {}, now);
    buf.push("VAN-1", "c", {}, now);

    auto all = buf.drainAll();
    ASSERT_EQ(all.size(), 3u);
    // Sorted by sequence_id
    EXPECT_EQ(all[0].event_type, "a");
    EXPECT_EQ(all[1].event_type, "b");
    EXPECT_EQ(all[2].event_type, "c");

    // Drain since seq 2
    auto partial = buf.drainAll(2);
    ASSERT_EQ(partial.size(), 1u);
    EXPECT_EQ(partial[0].event_type, "c");
}
