#pragma once

#include "gateway/GatewayTypes.h"
#include "core/Result.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <optional>

namespace mt::gateway {

class VanRegistry {
public:
    VanRegistry() = default;

    Result<void> registerVan(const VanRegistration& reg);
    Result<void> updateVan(const VanId& van_id, const VanRegistration& reg);
    Result<void> deregisterVan(const VanId& van_id);

    const VanRegistration* getVan(const VanId& van_id) const;
    VanRegistration* getVan(const VanId& van_id);

    std::vector<VanRegistration> listVans() const;
    std::vector<VanRegistration> listVansByTenant(TenantId tenant_id) const;

    void setVanState(const VanId& van_id, VanState state, TimePoint now);

    size_t size() const { return vans_.size(); }
    bool contains(const VanId& van_id) const;

    nlohmann::json vanToJson(const VanRegistration& reg) const;
    nlohmann::json listToJson(const std::vector<VanRegistration>& vans) const;

private:
    std::unordered_map<VanId, VanRegistration> vans_;
};

} // namespace mt::gateway
