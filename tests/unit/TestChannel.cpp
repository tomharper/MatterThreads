#include <gtest/gtest.h>
#include "net/Channel.h"

using namespace mt;

TEST(Channel, FullyConnectedDelivers) {
    Random rng(42);
    Channel ch(rng);

    LinkParams params;
    params.link_up = true;
    params.base_loss_rate = 0.0f;

    int delivered = 0;
    for (int i = 0; i < 100; ++i) {
        auto d = ch.evaluate(params);
        if (d.deliver) ++delivered;
    }
    EXPECT_EQ(delivered, 100);
}

TEST(Channel, LinkDownDropsAll) {
    Random rng(42);
    Channel ch(rng);

    LinkParams params;
    params.link_up = false;

    for (int i = 0; i < 100; ++i) {
        auto d = ch.evaluate(params);
        EXPECT_FALSE(d.deliver);
    }
}

TEST(Channel, PartialLoss) {
    Random rng(42);
    Channel ch(rng);

    LinkParams params;
    params.link_up = true;
    params.base_loss_rate = 0.5f;

    int delivered = 0;
    int total = 10000;
    for (int i = 0; i < total; ++i) {
        auto d = ch.evaluate(params);
        if (d.deliver) ++delivered;
    }

    // Should be roughly 50% delivery
    double ratio = static_cast<double>(delivered) / static_cast<double>(total);
    EXPECT_GT(ratio, 0.45);
    EXPECT_LT(ratio, 0.55);
}

TEST(Channel, LatencyIsPositive) {
    Random rng(42);
    Channel ch(rng);

    LinkParams params;
    params.link_up = true;
    params.latency_mean_ms = 10.0f;
    params.latency_stddev_ms = 2.0f;

    for (int i = 0; i < 100; ++i) {
        auto d = ch.evaluate(params);
        if (d.deliver) {
            EXPECT_GE(d.delay.count(), 0);
        }
    }
}

TEST(Channel, LQIAndRSSIStamped) {
    Random rng(42);
    Channel ch(rng);

    LinkParams params;
    params.link_up = true;
    params.lqi = 180;
    params.rssi = -45;

    auto d = ch.evaluate(params);
    ASSERT_TRUE(d.deliver);
    EXPECT_EQ(d.delivered_lqi, 180);
    EXPECT_EQ(d.delivered_rssi, -45);
}
