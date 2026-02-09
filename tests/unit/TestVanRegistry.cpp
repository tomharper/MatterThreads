#include <gtest/gtest.h>
#include "gateway/VanRegistry.h"

using namespace mt;
using namespace mt::gateway;

static VanRegistration makeVan(const std::string& id, uint64_t device_id = 1,
                                TenantId tenant = 1) {
    VanRegistration reg;
    reg.van_id = id;
    reg.device_id = device_id;
    reg.name = "Test Van " + id;
    reg.tenant_id = tenant;
    reg.endpoints = {0, 1, 2, 3, 4, 5, 6, 7};
    reg.ip_address = "fd11:22::1";
    return reg;
}

TEST(VanRegistry, RegisterVan) {
    VanRegistry registry;
    auto reg = makeVan("VAN-001");
    auto result = registry.registerVan(reg);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(registry.size(), 1u);

    auto* v = registry.getVan("VAN-001");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->device_id, 1u);
    EXPECT_EQ(v->name, "Test Van VAN-001");
}

TEST(VanRegistry, GetVanByID) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-A", 42));

    auto* v = registry.getVan("VAN-A");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->device_id, 42u);
    EXPECT_EQ(v->van_id, "VAN-A");
}

TEST(VanRegistry, GetNonexistentVan) {
    VanRegistry registry;
    auto* v = registry.getVan("NOPE");
    EXPECT_EQ(v, nullptr);
}

TEST(VanRegistry, UpdateVan) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-U", 10));

    auto updated = makeVan("VAN-U", 99);
    updated.name = "Updated Name";
    auto result = registry.updateVan("VAN-U", updated);
    ASSERT_TRUE(result.ok());

    auto* v = registry.getVan("VAN-U");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->device_id, 99u);
    EXPECT_EQ(v->name, "Updated Name");
}

TEST(VanRegistry, DeregisterVan) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-D"));
    EXPECT_TRUE(registry.contains("VAN-D"));

    auto result = registry.deregisterVan("VAN-D");
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(registry.contains("VAN-D"));
    EXPECT_EQ(registry.size(), 0u);
}

TEST(VanRegistry, ListVansByTenant) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-T1A", 1, 1));
    registry.registerVan(makeVan("VAN-T1B", 2, 1));
    registry.registerVan(makeVan("VAN-T2A", 3, 2));

    auto tenant1 = registry.listVansByTenant(1);
    EXPECT_EQ(tenant1.size(), 2u);

    auto tenant2 = registry.listVansByTenant(2);
    EXPECT_EQ(tenant2.size(), 1u);

    auto tenant3 = registry.listVansByTenant(99);
    EXPECT_EQ(tenant3.size(), 0u);
}

TEST(VanRegistry, DuplicateRegistration) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-DUP"));
    auto result = registry.registerVan(makeVan("VAN-DUP"));
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("already registered"), std::string::npos);
}

TEST(VanRegistry, VanStateTracking) {
    VanRegistry registry;
    registry.registerVan(makeVan("VAN-S"));
    auto now = SteadyClock::now();

    auto* v = registry.getVan("VAN-S");
    EXPECT_EQ(v->state, VanState::Registered);

    registry.setVanState("VAN-S", VanState::Online, now);
    EXPECT_EQ(v->state, VanState::Online);

    registry.setVanState("VAN-S", VanState::Offline, now + Duration(1000));
    EXPECT_EQ(v->state, VanState::Offline);
}
