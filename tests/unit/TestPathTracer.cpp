#include <gtest/gtest.h>
#include "thread/PathTracer.h"
#include "thread/MeshTopology.h"
#include "net/Broker.h"
#include <cmath>

using namespace mt;

TEST(PathTracer, DirectLink) {
    auto t = MeshTopology::fullyConnected();
    auto r = tracePath(t, 0, 1);
    ASSERT_TRUE(r.reachable);
    EXPECT_EQ(r.hopCount(), 1u);
    EXPECT_EQ(r.hops[0].from, 0);
    EXPECT_EQ(r.hops[0].to, 1);
}

TEST(PathTracer, LinearChainTwoHops) {
    auto t = MeshTopology::linearChain();  // 0<->1<->2, no direct 0<->2
    auto r = tracePath(t, 0, 2);
    ASSERT_TRUE(r.reachable);
    EXPECT_EQ(r.hopCount(), 2u);
    EXPECT_EQ(r.hops[0].from, 0);
    EXPECT_EQ(r.hops[0].to, 1);
    EXPECT_EQ(r.hops[1].from, 1);
    EXPECT_EQ(r.hops[1].to, 2);
}

TEST(PathTracer, StarRoutesThroughHub) {
    auto t = MeshTopology::starFromLeader();  // 1<->2 down; hub is node 0
    auto r = tracePath(t, 1, 2);
    ASSERT_TRUE(r.reachable);
    EXPECT_EQ(r.hopCount(), 2u);
    EXPECT_EQ(r.hops[0].to, 0);  // via hub
    EXPECT_EQ(r.hops[1].to, 2);
}

TEST(PathTracer, LocalizesWeakestLink) {
    auto t = MeshTopology::linearChain();
    t.setLinkLoss(1, 2, 0.30f);  // second hop is the lossy one
    auto r = tracePath(t, 0, 2);
    ASSERT_TRUE(r.reachable);
    int worst = r.worstHop();
    ASSERT_GE(worst, 0);
    EXPECT_EQ(r.hops[static_cast<size_t>(worst)].from, 1);
    EXPECT_EQ(r.hops[static_cast<size_t>(worst)].to, 2);
    EXPECT_NEAR(r.hops[static_cast<size_t>(worst)].expected_loss, 0.30f, 1e-5f);
}

TEST(PathTracer, CumulativeDelivery) {
    auto t = MeshTopology::linearChain();
    t.setLinkLoss(0, 1, 0.10f);
    t.setLinkLoss(1, 2, 0.20f);
    auto r = tracePath(t, 0, 2);
    ASSERT_TRUE(r.reachable);
    EXPECT_NEAR(r.cumulativeDelivery(), 0.72f, 1e-4f);  // (1-.1)*(1-.2)
}

TEST(PathTracer, UnreachableIsolatedNode) {
    MeshTopology t;  // default: all links up
    t.setBidirectionalDown(0, 1);
    t.setBidirectionalDown(0, 2);
    t.setBidirectionalDown(0, 3);
    auto r = tracePath(t, 0, 1);  // node 0 now isolated
    EXPECT_FALSE(r.reachable);
    EXPECT_EQ(r.hopCount(), 0u);
}

TEST(PathTracer, SameNodeIsZeroHops) {
    auto t = MeshTopology::fullyConnected();
    auto r = tracePath(t, 2, 2);
    EXPECT_TRUE(r.reachable);
    EXPECT_EQ(r.hopCount(), 0u);
    EXPECT_FLOAT_EQ(r.cumulativeDelivery(), 1.0f);
    EXPECT_EQ(r.worstHop(), -1);
}

TEST(PathTracer, FragmentCount) {
    EXPECT_EQ(fragmentCount(0), 1u);     // empty datagram is still one fragment
    EXPECT_EQ(fragmentCount(80), 1u);    // exactly one fragment
    EXPECT_EQ(fragmentCount(81), 2u);    // spills into a second
    EXPECT_EQ(fragmentCount(160), 2u);
    EXPECT_EQ(fragmentCount(161), 3u);
}

TEST(PathTracer, FragmentationAmplifiesLoss) {
    auto t = MeshTopology::linearChain();
    t.setLinkLoss(0, 1, 0.10f);   // one lossy hop, direct 0->1
    auto r = tracePath(t, 0, 1);
    ASSERT_TRUE(r.reachable);
    EXPECT_NEAR(r.cumulativeDelivery(), 0.90f, 1e-4f);

    // Small datagram (1 fragment) ~ single-frame delivery.
    EXPECT_NEAR(r.deliveryForSize(40), 0.90f, 1e-4f);
    // 160B -> 2 fragments: 0.9^2 = 0.81.
    EXPECT_NEAR(r.deliveryForSize(160), 0.81f, 1e-4f);
    // 400B -> 5 fragments: 0.9^5, markedly worse — the amplification.
    EXPECT_NEAR(r.deliveryForSize(400), std::pow(0.9f, 5.0f), 1e-4f);
}

TEST(PathTracer, PerfectPathDeliversAnySize) {
    auto t = MeshTopology::fullyConnected();  // 0->1 lossless
    auto r = tracePath(t, 0, 1);
    EXPECT_FLOAT_EQ(r.deliveryForSize(1024), 1.0f);
}

TEST(PathTracer, InbandTraceLowersDelivery) {
    // Observer effect: a lossy path where the trace option's own bytes push the
    // datagram into more fragments, lowering its delivery below the untraced case.
    auto t = MeshTopology::linearChain();
    t.setLinkLoss(0, 1, 0.10f);
    t.setLinkLoss(1, 2, 0.10f);
    auto r = tracePath(t, 0, 2);     // 2 hops
    ASSERT_TRUE(r.reachable);
    // 80 B = 1 fragment untraced; +8 B/hop * 2 hops = 96 B = 2 fragments traced.
    float without = r.deliveryForSize(80);
    float with = r.deliveryForSizeWithTrace(80, 8);
    EXPECT_LT(with, without);
}

TEST(BrokerRoute, MultiHopOverLinearChain) {
    Broker b(1);
    b.applyTopology(MeshTopology::linearChain());  // 0<->1<->2, no direct 0<->2
    auto path = b.route(0, 2);
    ASSERT_EQ(path.size(), 3u);
    EXPECT_EQ(path[0], 0);
    EXPECT_EQ(path[1], 1);
    EXPECT_EQ(path[2], 2);
}

TEST(BrokerRoute, UnreachableIsEmpty) {
    Broker b(1);
    MeshTopology t;
    t.setBidirectionalDown(0, 1);
    t.setBidirectionalDown(0, 2);
    t.setBidirectionalDown(0, 3);
    b.applyTopology(t);
    EXPECT_TRUE(b.route(0, 1).empty());
}
