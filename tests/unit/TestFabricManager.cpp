#include <gtest/gtest.h>
#include "gateway/FabricManager.h"

using namespace mt;
using namespace mt::gateway;

TEST(FabricManager, CreateTenant) {
    FabricManager fm;
    auto result = fm.createTenant("Fleet Alpha");
    ASSERT_TRUE(result.ok());

    TenantId id = *result;
    auto* tenant = fm.getTenant(id);
    ASSERT_NE(tenant, nullptr);
    EXPECT_EQ(tenant->name, "Fleet Alpha");
    EXPECT_NE(tenant->fabric_id, 0u);
    EXPECT_FALSE(tenant->root_cert.empty());
}

TEST(FabricManager, RemoveTenant) {
    FabricManager fm;
    auto id = *fm.createTenant("ToRemove");
    EXPECT_EQ(fm.tenantCount(), 1u);

    auto result = fm.removeTenant(id);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(fm.tenantCount(), 0u);
    EXPECT_EQ(fm.getTenant(id), nullptr);
}

TEST(FabricManager, IssueNOC) {
    FabricManager fm;
    auto tid = *fm.createTenant("Fleet");

    auto result = fm.issueNOC(tid, "VAN-001", 12345);
    ASSERT_TRUE(result.ok());

    auto cred = *result;
    EXPECT_EQ(cred.van_id, "VAN-001");
    EXPECT_EQ(cred.tenant_id, tid);
    EXPECT_NE(cred.node_id, 0u);
    EXPECT_FALSE(cred.noc.empty());
}

TEST(FabricManager, RevokeNOC) {
    FabricManager fm;
    auto tid = *fm.createTenant("Fleet");
    fm.issueNOC(tid, "VAN-R", 1);

    auto result = fm.revokeNOC(tid, "VAN-R");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(fm.getCredential("VAN-R"), nullptr);
}

TEST(FabricManager, TenantIsolation) {
    FabricManager fm;
    auto t1 = *fm.createTenant("Fleet A");
    auto t2 = *fm.createTenant("Fleet B");

    fm.issueNOC(t1, "VAN-A1", 1);
    fm.issueNOC(t2, "VAN-B1", 2);

    EXPECT_TRUE(fm.canAccess(t1, "VAN-A1"));
    EXPECT_FALSE(fm.canAccess(t1, "VAN-B1"));
    EXPECT_FALSE(fm.canAccess(t2, "VAN-A1"));
    EXPECT_TRUE(fm.canAccess(t2, "VAN-B1"));
}

TEST(FabricManager, MaxVansEnforced) {
    FabricManager fm;
    auto tid = *fm.createTenant("Small Fleet", 2);

    ASSERT_TRUE(fm.issueNOC(tid, "VAN-1", 1).ok());
    ASSERT_TRUE(fm.issueNOC(tid, "VAN-2", 2).ok());

    auto result = fm.issueNOC(tid, "VAN-3", 3);
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("limit reached"), std::string::npos);
}

TEST(FabricManager, UniqueFabricIds) {
    FabricManager fm;
    auto t1 = *fm.createTenant("Fleet A");
    auto t2 = *fm.createTenant("Fleet B");

    auto* ta = fm.getTenant(t1);
    auto* tb = fm.getTenant(t2);
    EXPECT_NE(ta->fabric_id, tb->fabric_id);
}

TEST(FabricManager, ListTenants) {
    FabricManager fm;
    fm.createTenant("Alpha");
    fm.createTenant("Beta");
    fm.createTenant("Gamma");

    auto tenants = fm.listTenants();
    EXPECT_EQ(tenants.size(), 3u);
}
