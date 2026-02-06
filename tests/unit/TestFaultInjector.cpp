#include <gtest/gtest.h>
#include "net/FaultInjector.h"

using namespace mt;

TEST(FaultInjector, NoRulesPassThrough) {
    Random rng(42);
    FaultInjector fi(rng);

    MacFrame frame;
    frame.payload = {1, 2, 3};

    auto decision = fi.applyFaults(0, 1, frame, SteadyClock::now());
    EXPECT_TRUE(decision.deliver);
}

TEST(FaultInjector, LinkDownDrops) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule rule;
    rule.type = FaultType::LinkDown;
    rule.affected_src = 0;
    rule.affected_dst = 1;
    rule.start_time = SteadyClock::now();
    fi.addRule(rule);

    MacFrame frame;
    auto decision = fi.applyFaults(0, 1, frame, SteadyClock::now());
    EXPECT_FALSE(decision.deliver);
}

TEST(FaultInjector, LinkDownDoesNotAffectOtherLinks) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule rule;
    rule.type = FaultType::LinkDown;
    rule.affected_src = 0;
    rule.affected_dst = 1;
    rule.start_time = SteadyClock::now();
    fi.addRule(rule);

    MacFrame frame;
    // Different link: 1 -> 2
    auto decision = fi.applyFaults(1, 2, frame, SteadyClock::now());
    EXPECT_TRUE(decision.deliver);
}

TEST(FaultInjector, PacketDropRate) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule rule;
    rule.type = FaultType::PacketDrop;
    rule.affected_src = ANY_NODE;
    rule.affected_dst = ANY_NODE;
    rule.drop_rate = 0.5f;
    rule.start_time = SteadyClock::now();
    fi.addRule(rule);

    int dropped = 0;
    int total = 10000;
    auto now = SteadyClock::now();

    for (int i = 0; i < total; ++i) {
        MacFrame frame;
        auto d = fi.applyFaults(0, 1, frame, now);
        if (!d.deliver) ++dropped;
    }

    double drop_ratio = static_cast<double>(dropped) / static_cast<double>(total);
    EXPECT_GT(drop_ratio, 0.40);
    EXPECT_LT(drop_ratio, 0.60);
}

TEST(FaultInjector, LatencySpike) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule rule;
    rule.type = FaultType::LatencySpike;
    rule.affected_src = ANY_NODE;
    rule.affected_dst = ANY_NODE;
    rule.extra_latency = Duration(200);
    rule.start_time = SteadyClock::now();
    fi.addRule(rule);

    MacFrame frame;
    auto d = fi.applyFaults(0, 1, frame, SteadyClock::now());
    EXPECT_TRUE(d.deliver);
    EXPECT_GE(d.delay.count(), 200);
}

TEST(FaultInjector, CorruptPayload) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule rule;
    rule.type = FaultType::Corrupt;
    rule.affected_src = ANY_NODE;
    rule.affected_dst = ANY_NODE;
    rule.corrupt_bit_count = 1;
    rule.start_time = SteadyClock::now();
    fi.addRule(rule);

    MacFrame frame;
    frame.payload = {0x00, 0x00, 0x00, 0x00};
    auto original = frame.payload;

    fi.applyFaults(0, 1, frame, SteadyClock::now());

    // At least one byte should differ
    EXPECT_NE(frame.payload, original);
}

TEST(FaultInjector, PurgeExpiredRules) {
    Random rng(42);
    FaultInjector fi(rng);

    auto now = SteadyClock::now();

    FaultRule rule;
    rule.type = FaultType::LinkDown;
    rule.affected_src = 0;
    rule.affected_dst = 1;
    rule.duration = Duration(100);
    rule.start_time = now;
    fi.addRule(rule);

    EXPECT_EQ(fi.rules().size(), 1);

    fi.purgeExpired(now + Duration(200));

    EXPECT_EQ(fi.rules().size(), 0);
}

TEST(FaultInjector, ClearRules) {
    Random rng(42);
    FaultInjector fi(rng);

    FaultRule r;
    r.type = FaultType::LinkDown;
    r.start_time = SteadyClock::now();
    fi.addRule(r);
    fi.addRule(r);

    EXPECT_EQ(fi.rules().size(), 2);
    fi.clearRules();
    EXPECT_EQ(fi.rules().size(), 0);
}
