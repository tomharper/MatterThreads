#pragma once

#include "core/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace mt {

// Simulated DNS-SD service record
struct ServiceRecord {
    std::string service_name;     // e.g. "MyLightBulb"
    std::string service_type;     // e.g. "_matter._tcp" or "_matterc._udp"
    NodeId node_id;
    RLOC16 rloc16;
    uint16_t port = 5540;
    TimePoint registered_at{};
    Duration ttl{120000};         // 2-minute TTL

    // Matter-specific TXT record fields
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    uint8_t  device_type = 0;     // 0x0100=light, 0x000A=door-lock, etc.
    std::string discriminator;    // Commissioning discriminator
    bool commissioning_open = false;

    bool isExpired(TimePoint now) const {
        return (now - registered_at) > ttl;
    }
};

// Simulated mDNS browse result
struct BrowseResult {
    std::string service_name;
    std::string service_type;
    NodeId node_id;
    RLOC16 rloc16;
    uint16_t port;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t device_type;
};

// Service types for Matter
static const std::string MATTER_COMMISSION_SERVICE = "_matterc._udp";
static const std::string MATTER_OPERATIONAL_SERVICE = "_matter._tcp";

using DiscoveryCallback = std::function<void(const BrowseResult&)>;

// Simulated DNS-SD / SRP server (runs on Border Router)
class ServiceRegistry {
public:
    // Register a service (SRP registration from a Thread device)
    void registerService(const ServiceRecord& record);

    // Unregister a service
    void unregisterService(const std::string& service_name, NodeId node_id);

    // Unregister all services for a node (e.g. node crashed)
    void unregisterNode(NodeId node_id);

    // Browse for services of a given type (mDNS browse)
    std::vector<BrowseResult> browse(const std::string& service_type) const;

    // Resolve a specific service by name
    std::optional<BrowseResult> resolve(const std::string& service_name) const;

    // Expire stale registrations
    void expireStale(TimePoint now);

    // Get all registered services
    const std::vector<ServiceRecord>& records() const { return records_; }

    size_t size() const { return records_.size(); }

private:
    std::vector<ServiceRecord> records_;
};

// Discovery client (runs on Phone / controller node)
class DiscoveryClient {
public:
    // Browse for commissionable devices
    std::vector<BrowseResult> browseCommissionable(const ServiceRegistry& registry) const;

    // Browse for operational devices on a fabric
    std::vector<BrowseResult> browseOperational(const ServiceRegistry& registry) const;

    // Resolve a specific device by service name
    std::optional<BrowseResult> resolveDevice(const ServiceRegistry& registry,
                                               const std::string& name) const;

    // Set callback for discovery events
    void onDeviceDiscovered(DiscoveryCallback cb) { on_discovered_ = std::move(cb); }

    // Trigger a discovery scan (calls callback for each found device)
    void scan(const ServiceRegistry& registry);

private:
    DiscoveryCallback on_discovered_;
    std::vector<std::string> discovered_names_;
};

} // namespace mt
