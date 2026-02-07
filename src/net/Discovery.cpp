#include "net/Discovery.h"
#include "core/Log.h"
#include <algorithm>

namespace mt {

void ServiceRegistry::registerService(const ServiceRecord& record) {
    // Update existing or add new
    for (auto& r : records_) {
        if (r.service_name == record.service_name && r.node_id == record.node_id) {
            r = record;
            MT_DEBUG("discovery", "Updated service: " + record.service_name +
                     " (node " + std::to_string(record.node_id) + ")");
            return;
        }
    }
    records_.push_back(record);
    MT_INFO("discovery", "Registered service: " + record.service_name +
            " type=" + record.service_type +
            " node=" + std::to_string(record.node_id));
}

void ServiceRegistry::unregisterService(const std::string& service_name, NodeId node_id) {
    auto it = std::remove_if(records_.begin(), records_.end(),
        [&](const ServiceRecord& r) {
            return r.service_name == service_name && r.node_id == node_id;
        });
    if (it != records_.end()) {
        MT_INFO("discovery", "Unregistered service: " + service_name);
        records_.erase(it, records_.end());
    }
}

void ServiceRegistry::unregisterNode(NodeId node_id) {
    auto it = std::remove_if(records_.begin(), records_.end(),
        [node_id](const ServiceRecord& r) { return r.node_id == node_id; });
    if (it != records_.end()) {
        MT_INFO("discovery", "Unregistered all services for node " + std::to_string(node_id));
        records_.erase(it, records_.end());
    }
}

std::vector<BrowseResult> ServiceRegistry::browse(const std::string& service_type) const {
    std::vector<BrowseResult> results;
    for (const auto& r : records_) {
        if (r.service_type == service_type) {
            results.push_back({
                r.service_name,
                r.service_type,
                r.node_id,
                r.rloc16,
                r.port,
                r.vendor_id,
                r.product_id,
                r.device_type
            });
        }
    }
    return results;
}

std::optional<BrowseResult> ServiceRegistry::resolve(const std::string& service_name) const {
    for (const auto& r : records_) {
        if (r.service_name == service_name) {
            return BrowseResult{
                r.service_name,
                r.service_type,
                r.node_id,
                r.rloc16,
                r.port,
                r.vendor_id,
                r.product_id,
                r.device_type
            };
        }
    }
    return std::nullopt;
}

void ServiceRegistry::expireStale(TimePoint now) {
    auto it = std::remove_if(records_.begin(), records_.end(),
        [now](const ServiceRecord& r) {
            if (r.isExpired(now)) {
                MT_INFO("discovery", "Service expired: " + r.service_name);
                return true;
            }
            return false;
        });
    records_.erase(it, records_.end());
}

std::vector<BrowseResult> DiscoveryClient::browseCommissionable(const ServiceRegistry& registry) const {
    return registry.browse(MATTER_COMMISSION_SERVICE);
}

std::vector<BrowseResult> DiscoveryClient::browseOperational(const ServiceRegistry& registry) const {
    return registry.browse(MATTER_OPERATIONAL_SERVICE);
}

std::optional<BrowseResult> DiscoveryClient::resolveDevice(const ServiceRegistry& registry,
                                                            const std::string& name) const {
    return registry.resolve(name);
}

void DiscoveryClient::scan(const ServiceRegistry& registry) {
    if (!on_discovered_) return;

    // Scan both commissionable and operational
    auto commissionable = browseCommissionable(registry);
    auto operational = browseOperational(registry);

    auto notify = [&](const BrowseResult& result) {
        // Only notify for newly discovered services
        auto it = std::find(discovered_names_.begin(), discovered_names_.end(), result.service_name);
        if (it == discovered_names_.end()) {
            discovered_names_.push_back(result.service_name);
            on_discovered_(result);
        }
    };

    for (const auto& r : commissionable) notify(r);
    for (const auto& r : operational) notify(r);
}

} // namespace mt
