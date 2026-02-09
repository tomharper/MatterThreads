#include "gateway/FabricManager.h"

namespace mt::gateway {

Result<TenantId> FabricManager::createTenant(const std::string& name, size_t max_vans) {
    if (name.empty()) {
        return Error{-1, "tenant name cannot be empty"};
    }

    TenantId id = next_tenant_id_++;
    TenantInfo info;
    info.tenant_id = id;
    info.name = name;
    info.fabric_id = generateFabricId(id);
    info.root_cert = generateRootCert(id);
    info.ipk = {0x00, 0x11, 0x22, 0x33}; // Simulated IPK
    info.created_at = SteadyClock::now();
    info.max_vans = max_vans;

    tenants_[id] = std::move(info);
    return id;
}

Result<void> FabricManager::removeTenant(TenantId id) {
    auto it = tenants_.find(id);
    if (it == tenants_.end()) {
        return Error{-1, "tenant not found: " + std::to_string(id)};
    }

    // Remove all credentials for this tenant
    for (auto cit = credentials_.begin(); cit != credentials_.end();) {
        if (cit->second.tenant_id == id) {
            cit = credentials_.erase(cit);
        } else {
            ++cit;
        }
    }

    tenants_.erase(it);
    return Result<void>::success();
}

const TenantInfo* FabricManager::getTenant(TenantId id) const {
    auto it = tenants_.find(id);
    return it != tenants_.end() ? &it->second : nullptr;
}

std::vector<TenantInfo> FabricManager::listTenants() const {
    std::vector<TenantInfo> result;
    result.reserve(tenants_.size());
    for (const auto& [id, info] : tenants_) {
        result.push_back(info);
    }
    return result;
}

Result<VanCredential> FabricManager::issueNOC(TenantId tenant_id,
                                                const VanId& van_id,
                                                uint64_t device_id) {
    auto* tenant = getTenant(tenant_id);
    if (!tenant) {
        return Error{-1, "tenant not found: " + std::to_string(tenant_id)};
    }

    // Check van limit
    size_t count = credentialCount(tenant_id);
    if (count >= tenant->max_vans) {
        return Error{-2, "tenant van limit reached: " + std::to_string(tenant->max_vans)};
    }

    // Check for duplicate
    if (credentials_.count(van_id)) {
        return Error{-3, "credential already issued for van: " + van_id};
    }

    uint64_t node_id = next_node_id_++;
    VanCredential cred;
    cred.van_id = van_id;
    cred.tenant_id = tenant_id;
    cred.fabric_index = static_cast<FabricIndex>(tenant_id);
    cred.node_id = node_id;
    cred.noc = generateNOC(tenant_id, node_id);
    cred.issued_at = SteadyClock::now();

    credentials_[van_id] = cred;
    return cred;
}

Result<void> FabricManager::revokeNOC(TenantId tenant_id, const VanId& van_id) {
    auto it = credentials_.find(van_id);
    if (it == credentials_.end()) {
        return Error{-1, "no credential for van: " + van_id};
    }
    if (it->second.tenant_id != tenant_id) {
        return Error{-2, "van does not belong to this tenant"};
    }
    credentials_.erase(it);
    return Result<void>::success();
}

const VanCredential* FabricManager::getCredential(const VanId& van_id) const {
    auto it = credentials_.find(van_id);
    return it != credentials_.end() ? &it->second : nullptr;
}

bool FabricManager::canAccess(TenantId tenant_id, const VanId& van_id) const {
    auto it = credentials_.find(van_id);
    if (it == credentials_.end()) return false;
    return it->second.tenant_id == tenant_id;
}

size_t FabricManager::credentialCount(TenantId tenant_id) const {
    size_t count = 0;
    for (const auto& [van_id, cred] : credentials_) {
        if (cred.tenant_id == tenant_id) ++count;
    }
    return count;
}

FabricId FabricManager::generateFabricId(TenantId tenant_id) const {
    // Simulated: derive fabric ID from tenant ID
    return static_cast<FabricId>(0x100000000ULL + tenant_id);
}

std::vector<uint8_t> FabricManager::generateRootCert(TenantId tenant_id) const {
    // Simulated root certificate (placeholder bytes)
    return {0x30, 0x82, static_cast<uint8_t>(tenant_id & 0xFF),
            static_cast<uint8_t>((tenant_id >> 8) & 0xFF)};
}

std::vector<uint8_t> FabricManager::generateNOC(TenantId tenant_id,
                                                  uint64_t node_id) const {
    // Simulated NOC (placeholder bytes encoding tenant + node)
    return {0x30, 0x81,
            static_cast<uint8_t>(tenant_id & 0xFF),
            static_cast<uint8_t>(node_id & 0xFF),
            static_cast<uint8_t>((node_id >> 8) & 0xFF)};
}

} // namespace mt::gateway
