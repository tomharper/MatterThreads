#include "thread/SRP.h"
#include "core/Log.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace mt {

// --- SRPServer ---

bool SRPServer::registerLease(NodeId node_id, RLOC16 rloc16, const std::string& hostname,
                               const ServiceRecord& service, TimePoint now) {
    // Check for existing lease and update
    for (auto& lease : leases_) {
        if (lease.node_id == node_id && lease.service.service_name == service.service_name) {
            lease.rloc16 = rloc16;
            lease.hostname = hostname;
            lease.service = service;
            lease.service.registered_at = now;
            lease.lease_start = now;
            lease.renewal_count++;
            registry_.registerService(service);
            emitEvent("lease_renewed", node_id);
            MT_DEBUG("srp", "Renewed lease for node " + std::to_string(node_id) +
                     " service=" + service.service_name);
            return true;
        }
    }

    // New lease
    SRPLease lease;
    lease.hostname = hostname;
    lease.node_id = node_id;
    lease.rloc16 = rloc16;
    lease.service = service;
    lease.service.registered_at = now;
    lease.lease_start = now;
    leases_.push_back(lease);

    // Register in the DNS-SD service registry
    registry_.registerService(service);
    emitEvent("lease_created", node_id);

    MT_INFO("srp", "New SRP lease: node=" + std::to_string(node_id) +
            " host=" + hostname + " service=" + service.service_name +
            " type=" + service.service_type);
    return true;
}

bool SRPServer::renewLease(NodeId node_id, TimePoint now) {
    bool renewed = false;
    for (auto& lease : leases_) {
        if (lease.node_id == node_id) {
            lease.lease_start = now;
            lease.service.registered_at = now;
            lease.renewal_count++;
            registry_.registerService(lease.service);
            renewed = true;
        }
    }
    if (renewed) {
        emitEvent("lease_bulk_renewed", node_id);
        MT_DEBUG("srp", "Bulk renewed leases for node " + std::to_string(node_id));
    }
    return renewed;
}

void SRPServer::removeLeasesForNode(NodeId node_id) {
    auto it = std::remove_if(leases_.begin(), leases_.end(),
        [node_id](const SRPLease& l) { return l.node_id == node_id; });
    if (it != leases_.end()) {
        leases_.erase(it, leases_.end());
        registry_.unregisterNode(node_id);
        emitEvent("leases_removed", node_id);
        MT_INFO("srp", "Removed all leases for node " + std::to_string(node_id));
    }
}

void SRPServer::tick(TimePoint now) {
    std::vector<size_t> expired_indices;

    for (size_t i = 0; i < leases_.size(); ++i) {
        auto state = leases_[i].state(now);
        if (state == SRPLeaseState::Expired) {
            expired_indices.push_back(i);
            MT_INFO("srp", "Lease expired: node=" + std::to_string(leases_[i].node_id) +
                    " service=" + leases_[i].service.service_name);
            emitEvent("lease_expired", leases_[i].node_id);
        } else if (state == SRPLeaseState::Expiring) {
            MT_DEBUG("srp", "Lease expiring soon: node=" + std::to_string(leases_[i].node_id) +
                     " service=" + leases_[i].service.service_name);
        }
    }

    // Remove expired leases (reverse order to maintain indices)
    for (auto it = expired_indices.rbegin(); it != expired_indices.rend(); ++it) {
        auto& lease = leases_[*it];
        registry_.unregisterService(lease.service.service_name, lease.node_id);
        leases_.erase(leases_.begin() + static_cast<long>(*it));
    }
}

void SRPServer::forceExpireLease(NodeId node_id) {
    for (auto& lease : leases_) {
        if (lease.node_id == node_id) {
            // Set lease_start far in the past
            lease.lease_start = lease.lease_start - lease.lease_duration - Duration(1);
        }
    }
    MT_INFO("srp", "Force-expired leases for node " + std::to_string(node_id));
    emitEvent("lease_force_expired", node_id);
}

void SRPServer::updateNodeRLOC(NodeId node_id, RLOC16 new_rloc, TimePoint now) {
    for (auto& lease : leases_) {
        if (lease.node_id == node_id) {
            lease.rloc16 = new_rloc;
            lease.service.rloc16 = new_rloc;
            lease.service.registered_at = now;
            lease.lease_start = now;
            registry_.registerService(lease.service);
        }
    }
    emitEvent("rloc_updated", node_id);
    MT_INFO("srp", "Updated RLOC for node " + std::to_string(node_id) +
            " to 0x" + std::to_string(new_rloc));
}

size_t SRPServer::activeLeaseCount(TimePoint now) const {
    size_t count = 0;
    for (const auto& l : leases_) {
        if (!l.isExpired(now)) ++count;
    }
    return count;
}

bool SRPServer::hasActiveLease(NodeId node_id, TimePoint now) const {
    for (const auto& l : leases_) {
        if (l.node_id == node_id && !l.isExpired(now)) return true;
    }
    return false;
}

void SRPServer::emitEvent(const std::string& event, NodeId node_id) {
    if (event_cb_) event_cb_(event, node_id);
}

// --- SRPClient ---

SRPClient::SRPClient(NodeId node_id, uint64_t ext_addr)
    : node_id_(node_id) {
    // Generate hostname from EUI-64
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << ext_addr << ".local";
    hostname_ = oss.str();
}

bool SRPClient::registerWithServer(SRPServer& server, RLOC16 rloc16,
                                    const std::string& service_name,
                                    const std::string& service_type,
                                    uint16_t port, TimePoint now) {
    current_rloc_ = rloc16;

    ServiceRecord rec;
    rec.service_name = service_name;
    rec.service_type = service_type;
    rec.node_id = node_id_;
    rec.rloc16 = rloc16;
    rec.port = port;
    rec.registered_at = now;

    bool ok = server.registerLease(node_id_, rloc16, hostname_, rec, now);
    if (ok) {
        registered_ = true;
        last_registration_ = now;
        rloc_changed_ = false;
    }
    return ok;
}

bool SRPClient::renewWithServer(SRPServer& server, TimePoint now) {
    if (!registered_) return false;

    bool ok = server.renewLease(node_id_, now);
    if (ok) {
        last_registration_ = now;
    }
    return ok;
}

bool SRPClient::needsRenewal(TimePoint now) const {
    if (!registered_) return false;
    if (rloc_changed_) return true;
    auto elapsed = std::chrono::duration_cast<Duration>(now - last_registration_);
    return elapsed > renewal_interval_;
}

} // namespace mt
