#include <gtest/gtest.h>
#include "thread/SRP.h"

using namespace mt;

TEST(SRP, ServerRegisterLease) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.rloc16 = 0x0400;
    rec.port = 5540;

    EXPECT_TRUE(server.registerLease(1, 0x0400, "0000000000001001.local", rec, now));
    EXPECT_EQ(server.leases().size(), 1);
    EXPECT_EQ(registry.size(), 1);
}

TEST(SRP, ServerRenewLease) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.rloc16 = 0x0400;
    rec.port = 5540;

    server.registerLease(1, 0x0400, "0000000000001001.local", rec, now);

    // Renew
    EXPECT_TRUE(server.renewLease(1, now + Duration(60000)));
    EXPECT_EQ(server.leases().size(), 1);
    EXPECT_EQ(server.leases()[0].renewal_count, 1);
}

TEST(SRP, ServerExpireLease) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.rloc16 = 0x0400;
    rec.port = 5540;

    server.registerLease(1, 0x0400, "host.local", rec, now);
    EXPECT_TRUE(server.hasActiveLease(1, now));

    // Default lease is 2 hours = 7200000ms
    server.tick(now + Duration(7200001));
    EXPECT_EQ(server.leases().size(), 0);
    EXPECT_EQ(registry.size(), 0);
    EXPECT_FALSE(server.hasActiveLease(1, now + Duration(7200001)));
}

TEST(SRP, ServerForceExpire) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.rloc16 = 0x0400;
    rec.port = 5540;

    server.registerLease(1, 0x0400, "host.local", rec, now);
    EXPECT_TRUE(server.hasActiveLease(1, now));

    server.forceExpireLease(1);
    server.tick(now); // Should expire immediately after force
    EXPECT_EQ(server.leases().size(), 0);
}

TEST(SRP, ServerUpdateRLOC) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.rloc16 = 0x0400;
    rec.port = 5540;

    server.registerLease(1, 0x0400, "host.local", rec, now);

    // Device re-attaches with new RLOC
    server.updateNodeRLOC(1, 0x0005, now + Duration(1000));

    EXPECT_EQ(server.leases()[0].rloc16, 0x0005);
    auto resolved = registry.resolve("node1");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->rloc16, 0x0005);
}

TEST(SRP, ServerRemoveNode) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    ServiceRecord op;
    op.service_name = "node1";
    op.service_type = MATTER_OPERATIONAL_SERVICE;
    op.node_id = 1;
    op.rloc16 = 0x0400;
    op.port = 5540;
    server.registerLease(1, 0x0400, "host.local", op, now);

    ServiceRecord comm;
    comm.service_name = "node1-commission";
    comm.service_type = MATTER_COMMISSION_SERVICE;
    comm.node_id = 1;
    comm.rloc16 = 0x0400;
    comm.port = 5540;
    server.registerLease(1, 0x0400, "host.local", comm, now);

    EXPECT_EQ(server.leases().size(), 2);

    server.removeLeasesForNode(1);
    EXPECT_EQ(server.leases().size(), 0);
    EXPECT_EQ(registry.size(), 0);
}

TEST(SRP, ServerEventCallback) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    std::vector<std::string> events;
    server.onEvent([&events](const std::string& event, NodeId) {
        events.push_back(event);
    });

    ServiceRecord rec;
    rec.service_name = "node1";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 1;
    rec.port = 5540;

    server.registerLease(1, 0x0400, "host.local", rec, now);
    EXPECT_EQ(events.size(), 1);
    EXPECT_EQ(events[0], "lease_created");

    server.renewLease(1, now + Duration(1000));
    EXPECT_EQ(events.size(), 2);
    EXPECT_EQ(events[1], "lease_bulk_renewed");
}

TEST(SRP, ClientRegisterAndRenew) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    SRPClient client(1, 0x0000000000001001);

    EXPECT_TRUE(client.registerWithServer(server, 0x0400, "node1",
                                           MATTER_OPERATIONAL_SERVICE, 5540, now));
    EXPECT_EQ(server.leases().size(), 1);
    EXPECT_EQ(registry.size(), 1);

    // Should not need renewal immediately
    EXPECT_FALSE(client.needsRenewal(now + Duration(1000)));

    // After renewal interval (default 60s), should need renewal
    EXPECT_TRUE(client.needsRenewal(now + Duration(61000)));

    EXPECT_TRUE(client.renewWithServer(server, now + Duration(61000)));
}

TEST(SRP, ClientRLOCChangeTriggerRenewal) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    SRPClient client(1, 0x0000000000001001);
    client.registerWithServer(server, 0x0400, "node1",
                               MATTER_OPERATIONAL_SERVICE, 5540, now);

    EXPECT_FALSE(client.needsRenewal(now + Duration(1000)));

    // RLOC changes (re-attach)
    client.onRLOCChanged(0x0005);
    EXPECT_TRUE(client.hasRLOCChanged());

    // Should need renewal immediately due to RLOC change
    EXPECT_TRUE(client.needsRenewal(now + Duration(1000)));

    client.clearRLOCChanged();
    EXPECT_FALSE(client.hasRLOCChanged());
}

TEST(SRP, ClientHostnameFromEUI64) {
    SRPClient client(1, 0x0000000000001001);
    EXPECT_EQ(client.hostname(), "0000000000001001.local");

    SRPClient client2(0, 0xABCDEF0123456789);
    EXPECT_EQ(client2.hostname(), "abcdef0123456789.local");
}

TEST(SRP, LeaseStateTransitions) {
    SRPLease lease;
    auto now = SteadyClock::now();
    lease.lease_start = now;
    lease.lease_duration = Duration(10000); // 10 seconds for testing

    // Active
    EXPECT_EQ(lease.state(now + Duration(5000)), SRPLeaseState::Active);

    // Expiring (within 10% of TTL = last 1 second)
    EXPECT_EQ(lease.state(now + Duration(9500)), SRPLeaseState::Expiring);

    // Expired
    EXPECT_EQ(lease.state(now + Duration(11000)), SRPLeaseState::Expired);
    EXPECT_TRUE(lease.isExpired(now + Duration(11000)));
}

TEST(SRP, ActiveLeaseCount) {
    ServiceRegistry registry;
    SRPServer server(registry);
    auto now = SteadyClock::now();

    for (NodeId i = 0; i < 3; ++i) {
        ServiceRecord rec;
        rec.service_name = "node" + std::to_string(i);
        rec.service_type = MATTER_OPERATIONAL_SERVICE;
        rec.node_id = i;
        rec.port = 5540;
        server.registerLease(i, makeRLOC16(static_cast<uint8_t>(i), 0),
                             "host" + std::to_string(i) + ".local", rec, now);
    }

    EXPECT_EQ(server.activeLeaseCount(now), 3);

    // Force expire one
    server.forceExpireLease(1);
    server.tick(now);
    EXPECT_EQ(server.activeLeaseCount(now), 2);
}
