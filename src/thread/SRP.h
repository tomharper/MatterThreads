#pragma once

#include "core/Types.h"
#include "net/Discovery.h"
#include <vector>
#include <functional>
#include <string>

namespace mt {

// SRP lease state
enum class SRPLeaseState {
    Active,
    Expiring,  // Within 10% of TTL remaining
    Expired,
    Removed
};

// A single SRP lease managed by the server
struct SRPLease {
    std::string hostname;           // e.g. "<eui64>.local"
    NodeId node_id;
    RLOC16 rloc16;
    ServiceRecord service;          // The DNS-SD service record
    TimePoint lease_start{};
    Duration lease_duration{7200000}; // 2 hours default
    Duration key_lease{1209600000};   // 14 days default
    uint32_t renewal_count = 0;

    SRPLeaseState state(TimePoint now) const {
        auto elapsed = std::chrono::duration_cast<Duration>(now - lease_start);
        if (elapsed > lease_duration) return SRPLeaseState::Expired;
        auto remaining = lease_duration - elapsed;
        auto threshold = Duration(lease_duration.count() / 10);
        if (remaining < threshold) return SRPLeaseState::Expiring;
        return SRPLeaseState::Active;
    }

    bool isExpired(TimePoint now) const {
        return state(now) == SRPLeaseState::Expired;
    }
};

using SRPEventCallback = std::function<void(const std::string& event, NodeId node_id)>;

// SRP Server — runs on the Border Router (Node 0)
// Accepts registrations from Thread devices, maintains the DNS-SD service registry
class SRPServer {
public:
    explicit SRPServer(ServiceRegistry& registry) : registry_(registry) {}

    // Process an SRP registration from a device
    bool registerLease(NodeId node_id, RLOC16 rloc16, const std::string& hostname,
                       const ServiceRecord& service, TimePoint now);

    // Renew an existing lease
    bool renewLease(NodeId node_id, TimePoint now);

    // Remove all leases for a node
    void removeLeasesForNode(NodeId node_id);

    // Check for expiring/expired leases
    void tick(TimePoint now);

    // Force-expire a lease (fault injection)
    void forceExpireLease(NodeId node_id);

    // Update RLOC for a node's leases (after re-attach)
    void updateNodeRLOC(NodeId node_id, RLOC16 new_rloc, TimePoint now);

    // Set event callback
    void onEvent(SRPEventCallback cb) { event_cb_ = std::move(cb); }

    // Query
    const std::vector<SRPLease>& leases() const { return leases_; }
    size_t activeLeaseCount(TimePoint now) const;
    bool hasActiveLease(NodeId node_id, TimePoint now) const;

private:
    void emitEvent(const std::string& event, NodeId node_id);

    ServiceRegistry& registry_;
    std::vector<SRPLease> leases_;
    SRPEventCallback event_cb_;
};

// SRP Client — runs on each Thread device
// Registers and renews services with the Border Router's SRP server
class SRPClient {
public:
    SRPClient(NodeId node_id, uint64_t ext_addr);

    // Generate hostname from EUI-64
    std::string hostname() const { return hostname_; }

    // Register with the SRP server
    bool registerWithServer(SRPServer& server, RLOC16 rloc16,
                            const std::string& service_name,
                            const std::string& service_type,
                            uint16_t port, TimePoint now);

    // Renew registration
    bool renewWithServer(SRPServer& server, TimePoint now);

    // Check if re-registration is needed (called in tick)
    bool needsRenewal(TimePoint now) const;

    // Track RLOC changes
    void onRLOCChanged(RLOC16 new_rloc) { current_rloc_ = new_rloc; rloc_changed_ = true; }
    bool hasRLOCChanged() const { return rloc_changed_; }
    void clearRLOCChanged() { rloc_changed_ = false; }

    // Config
    void setRenewalInterval(Duration interval) { renewal_interval_ = interval; }

private:
    NodeId node_id_;
    std::string hostname_;
    RLOC16 current_rloc_ = INVALID_RLOC16;
    bool rloc_changed_ = false;
    bool registered_ = false;
    TimePoint last_registration_{};
    Duration renewal_interval_{60000}; // 1 minute default (well within 2hr lease)
};

} // namespace mt
