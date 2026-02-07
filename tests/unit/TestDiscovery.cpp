#include <gtest/gtest.h>
#include "net/Discovery.h"

using namespace mt;

TEST(Discovery, RegisterAndBrowse) {
    ServiceRegistry registry;

    ServiceRecord rec;
    rec.service_name = "node0";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 0;
    rec.rloc16 = 0x0000;
    rec.port = 5540;
    rec.registered_at = SteadyClock::now();
    registry.registerService(rec);

    auto results = registry.browse(MATTER_OPERATIONAL_SERVICE);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].service_name, "node0");
    EXPECT_EQ(results[0].node_id, 0);
    EXPECT_EQ(results[0].port, 5540);
}

TEST(Discovery, BrowseFiltersByType) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    ServiceRecord op;
    op.service_name = "node0";
    op.service_type = MATTER_OPERATIONAL_SERVICE;
    op.node_id = 0;
    op.registered_at = now;
    registry.registerService(op);

    ServiceRecord comm;
    comm.service_name = "node1-commission";
    comm.service_type = MATTER_COMMISSION_SERVICE;
    comm.node_id = 1;
    comm.registered_at = now;
    registry.registerService(comm);

    auto operational = registry.browse(MATTER_OPERATIONAL_SERVICE);
    EXPECT_EQ(operational.size(), 1);
    EXPECT_EQ(operational[0].service_name, "node0");

    auto commissionable = registry.browse(MATTER_COMMISSION_SERVICE);
    EXPECT_EQ(commissionable.size(), 1);
    EXPECT_EQ(commissionable[0].service_name, "node1-commission");
}

TEST(Discovery, ResolveByName) {
    ServiceRegistry registry;

    ServiceRecord rec;
    rec.service_name = "sensor-temp";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 2;
    rec.rloc16 = 0x0401;
    rec.registered_at = SteadyClock::now();
    registry.registerService(rec);

    auto resolved = registry.resolve("sensor-temp");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->node_id, 2);
    EXPECT_EQ(resolved->rloc16, 0x0401);

    auto missing = registry.resolve("nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST(Discovery, UpdateExistingService) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node0";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 0;
    rec.rloc16 = 0x0000;
    rec.registered_at = now;
    registry.registerService(rec);
    EXPECT_EQ(registry.size(), 1);

    // Update RLOC (simulates re-attach to different parent)
    rec.rloc16 = 0x0005;
    rec.registered_at = now + Duration(1000);
    registry.registerService(rec);

    // Should still be 1 record, but updated
    EXPECT_EQ(registry.size(), 1);
    auto resolved = registry.resolve("node0");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->rloc16, 0x0005);
}

TEST(Discovery, UnregisterNode) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    // Register two services for node 1
    ServiceRecord op;
    op.service_name = "node1";
    op.service_type = MATTER_OPERATIONAL_SERVICE;
    op.node_id = 1;
    op.registered_at = now;
    registry.registerService(op);

    ServiceRecord comm;
    comm.service_name = "node1-commission";
    comm.service_type = MATTER_COMMISSION_SERVICE;
    comm.node_id = 1;
    comm.registered_at = now;
    registry.registerService(comm);

    EXPECT_EQ(registry.size(), 2);

    registry.unregisterNode(1);
    EXPECT_EQ(registry.size(), 0);
}

TEST(Discovery, ExpireStaleRecords) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    ServiceRecord rec;
    rec.service_name = "node0";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 0;
    rec.registered_at = now;
    rec.ttl = Duration(5000); // 5 second TTL
    registry.registerService(rec);

    EXPECT_EQ(registry.size(), 1);

    // Not expired yet
    registry.expireStale(now + Duration(4000));
    EXPECT_EQ(registry.size(), 1);

    // Now expired
    registry.expireStale(now + Duration(6000));
    EXPECT_EQ(registry.size(), 0);
}

TEST(Discovery, ClientBrowseCommissionable) {
    ServiceRegistry registry;
    DiscoveryClient client;
    auto now = SteadyClock::now();

    ServiceRecord comm;
    comm.service_name = "node2-commission";
    comm.service_type = MATTER_COMMISSION_SERVICE;
    comm.node_id = 2;
    comm.commissioning_open = true;
    comm.discriminator = "1002";
    comm.registered_at = now;
    registry.registerService(comm);

    auto results = client.browseCommissionable(registry);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].node_id, 2);
}

TEST(Discovery, ClientScanNotifiesNewOnly) {
    ServiceRegistry registry;
    DiscoveryClient client;
    auto now = SteadyClock::now();

    int callback_count = 0;
    client.onDeviceDiscovered([&callback_count](const BrowseResult&) {
        ++callback_count;
    });

    ServiceRecord rec;
    rec.service_name = "node0";
    rec.service_type = MATTER_OPERATIONAL_SERVICE;
    rec.node_id = 0;
    rec.registered_at = now;
    registry.registerService(rec);

    // First scan: discovers node0
    client.scan(registry);
    EXPECT_EQ(callback_count, 1);

    // Second scan: node0 already known, no new callback
    client.scan(registry);
    EXPECT_EQ(callback_count, 1);

    // Add new service, scan again
    ServiceRecord rec2;
    rec2.service_name = "node1";
    rec2.service_type = MATTER_OPERATIONAL_SERVICE;
    rec2.node_id = 1;
    rec2.registered_at = now;
    registry.registerService(rec2);

    client.scan(registry);
    EXPECT_EQ(callback_count, 2);
}

TEST(Discovery, UnregisterSingleService) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    ServiceRecord r1;
    r1.service_name = "node0";
    r1.service_type = MATTER_OPERATIONAL_SERVICE;
    r1.node_id = 0;
    r1.registered_at = now;
    registry.registerService(r1);

    ServiceRecord r2;
    r2.service_name = "node0-commission";
    r2.service_type = MATTER_COMMISSION_SERVICE;
    r2.node_id = 0;
    r2.registered_at = now;
    registry.registerService(r2);

    EXPECT_EQ(registry.size(), 2);

    // Unregister just the commissioning service
    registry.unregisterService("node0-commission", 0);
    EXPECT_EQ(registry.size(), 1);

    auto resolved = registry.resolve("node0");
    ASSERT_TRUE(resolved.has_value());
}

TEST(Discovery, MultipleNodesMultipleTypes) {
    ServiceRegistry registry;
    auto now = SteadyClock::now();

    // Register 3 nodes with both service types each
    for (NodeId i = 0; i < 3; ++i) {
        ServiceRecord op;
        op.service_name = "node" + std::to_string(i);
        op.service_type = MATTER_OPERATIONAL_SERVICE;
        op.node_id = i;
        op.registered_at = now;
        registry.registerService(op);

        ServiceRecord comm;
        comm.service_name = "node" + std::to_string(i) + "-commission";
        comm.service_type = MATTER_COMMISSION_SERVICE;
        comm.node_id = i;
        comm.registered_at = now;
        registry.registerService(comm);
    }

    EXPECT_EQ(registry.size(), 6);
    EXPECT_EQ(registry.browse(MATTER_OPERATIONAL_SERVICE).size(), 3);
    EXPECT_EQ(registry.browse(MATTER_COMMISSION_SERVICE).size(), 3);
}
