#include "gateway/VanRegistry.h"

namespace mt::gateway {

Result<void> VanRegistry::registerVan(const VanRegistration& reg) {
    if (reg.van_id.empty()) {
        return Error{-1, "van_id cannot be empty"};
    }
    if (vans_.count(reg.van_id)) {
        return Error{-2, "van already registered: " + reg.van_id};
    }
    vans_[reg.van_id] = reg;
    return Result<void>::success();
}

Result<void> VanRegistry::updateVan(const VanId& van_id, const VanRegistration& reg) {
    auto it = vans_.find(van_id);
    if (it == vans_.end()) {
        return Error{-1, "van not found: " + van_id};
    }
    it->second = reg;
    it->second.van_id = van_id; // Preserve original ID
    return Result<void>::success();
}

Result<void> VanRegistry::deregisterVan(const VanId& van_id) {
    auto it = vans_.find(van_id);
    if (it == vans_.end()) {
        return Error{-1, "van not found: " + van_id};
    }
    vans_.erase(it);
    return Result<void>::success();
}

const VanRegistration* VanRegistry::getVan(const VanId& van_id) const {
    auto it = vans_.find(van_id);
    return it != vans_.end() ? &it->second : nullptr;
}

VanRegistration* VanRegistry::getVan(const VanId& van_id) {
    auto it = vans_.find(van_id);
    return it != vans_.end() ? &it->second : nullptr;
}

std::vector<VanRegistration> VanRegistry::listVans() const {
    std::vector<VanRegistration> result;
    result.reserve(vans_.size());
    for (const auto& [id, reg] : vans_) {
        result.push_back(reg);
    }
    return result;
}

std::vector<VanRegistration> VanRegistry::listVansByTenant(TenantId tenant_id) const {
    std::vector<VanRegistration> result;
    for (const auto& [id, reg] : vans_) {
        if (reg.tenant_id == tenant_id) {
            result.push_back(reg);
        }
    }
    return result;
}

void VanRegistry::setVanState(const VanId& van_id, VanState state, TimePoint now) {
    auto it = vans_.find(van_id);
    if (it != vans_.end()) {
        it->second.state = state;
        it->second.last_seen = now;
    }
}

bool VanRegistry::contains(const VanId& van_id) const {
    return vans_.count(van_id) > 0;
}

nlohmann::json VanRegistry::vanToJson(const VanRegistration& reg) const {
    nlohmann::json j;
    j["van_id"] = reg.van_id;
    j["device_id"] = reg.device_id;
    j["name"] = reg.name;
    j["tenant_id"] = reg.tenant_id;
    j["fabric_index"] = reg.fabric_index;
    j["state"] = vanStateToString(reg.state);
    j["ip_address"] = reg.ip_address;

    nlohmann::json eps = nlohmann::json::array();
    for (auto ep : reg.endpoints) {
        eps.push_back(ep);
    }
    j["endpoints"] = eps;

    return j;
}

nlohmann::json VanRegistry::listToJson(const std::vector<VanRegistration>& vans) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& v : vans) {
        arr.push_back(vanToJson(v));
    }
    return arr;
}

} // namespace mt::gateway
