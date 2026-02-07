#pragma once

#include "core/Types.h"
#include "thread/Routing.h"
#include "net/Discovery.h"
#include <vector>
#include <unordered_map>
#include <optional>

namespace mt {

// Represents a proxied controller session through the Border Router
struct ProxyEntry {
    NodeId controller_id;   // e.g. phone node ID
    NodeId device_id;       // target device on the mesh
    RLOC16 device_rloc16;   // current RLOC16 of the device
    SessionId session_id;
    TimePoint created_at{};
    TimePoint last_activity{};
    Duration idle_timeout{300000}; // 5 minutes
    bool active = true;

    bool isIdle(TimePoint now) const {
        return (now - last_activity) > idle_timeout;
    }
};

// Simulated Border Router proxy table
// Maps controller (phone/cloud) sessions to mesh device addresses
class BorderRouterProxy {
public:
    explicit BorderRouterProxy(size_t max_entries = 32)
        : max_entries_(max_entries) {}

    // Create a proxy entry (returns false if table is full)
    bool addEntry(NodeId controller, NodeId device, RLOC16 device_rloc,
                  SessionId session, TimePoint now);

    // Remove a proxy entry
    void removeEntry(NodeId controller, NodeId device);

    // Remove all entries for a device (e.g. device crashed)
    void removeDevice(NodeId device);

    // Remove all entries for a controller
    void removeController(NodeId controller);

    // Look up the RLOC16 for a device (EID-to-RLOC resolution)
    std::optional<RLOC16> resolveDevice(NodeId device) const;

    // Update RLOC16 for a device (after re-attach)
    void updateDeviceRLOC(NodeId device, RLOC16 new_rloc);

    // Refresh the EID-to-RLOC mappings from the routing table
    void refreshFromRouting(const RoutingTable& routing, TimePoint now);

    // Mark activity on a session
    void touchSession(NodeId controller, NodeId device, TimePoint now);

    // Expire idle sessions
    void expireIdle(TimePoint now);

    // Get all entries
    const std::vector<ProxyEntry>& entries() const { return entries_; }

    // Stats
    size_t size() const { return entries_.size(); }
    size_t maxSize() const { return max_entries_; }
    bool isFull() const { return entries_.size() >= max_entries_; }
    size_t expiredCount() const { return expired_count_; }
    size_t rejectedCount() const { return rejected_count_; }

private:
    std::vector<ProxyEntry> entries_;
    size_t max_entries_;
    size_t expired_count_ = 0;
    size_t rejected_count_ = 0;
};

} // namespace mt
