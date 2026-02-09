#pragma once

#include "gateway/GatewayTypes.h"
#include "core/Result.h"

#include <unordered_map>
#include <vector>

namespace mt::gateway {

struct TenantInfo {
    TenantId tenant_id = 0;
    std::string name;
    FabricId fabric_id = 0;
    std::vector<uint8_t> root_cert;
    std::vector<uint8_t> ipk;
    TimePoint created_at{};
    size_t max_vans = 100;
};

struct VanCredential {
    VanId van_id;
    TenantId tenant_id = 0;
    FabricIndex fabric_index = 0;
    uint64_t node_id = 0;
    std::vector<uint8_t> noc;
    TimePoint issued_at{};
};

class FabricManager {
public:
    FabricManager() = default;

    Result<TenantId> createTenant(const std::string& name, size_t max_vans = 100);
    Result<void> removeTenant(TenantId id);
    const TenantInfo* getTenant(TenantId id) const;
    std::vector<TenantInfo> listTenants() const;

    Result<VanCredential> issueNOC(TenantId tenant_id, const VanId& van_id,
                                    uint64_t device_id);
    Result<void> revokeNOC(TenantId tenant_id, const VanId& van_id);
    const VanCredential* getCredential(const VanId& van_id) const;

    bool canAccess(TenantId tenant_id, const VanId& van_id) const;

    size_t tenantCount() const { return tenants_.size(); }
    size_t credentialCount(TenantId tenant_id) const;

private:
    std::unordered_map<TenantId, TenantInfo> tenants_;
    std::unordered_map<VanId, VanCredential> credentials_;
    TenantId next_tenant_id_ = 1;
    uint64_t next_node_id_ = 1000;

    FabricId generateFabricId(TenantId tenant_id) const;
    std::vector<uint8_t> generateRootCert(TenantId tenant_id) const;
    std::vector<uint8_t> generateNOC(TenantId tenant_id, uint64_t node_id) const;
};

} // namespace mt::gateway
