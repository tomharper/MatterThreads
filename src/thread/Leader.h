#pragma once

#include "core/Types.h"
#include <array>
#include <optional>

namespace mt {

static constexpr size_t MAX_ROUTER_IDS = 63;

class LeaderData {
    uint8_t partition_id_ = 1;
    uint8_t weighting_ = 64;
    uint8_t data_version_ = 0;
    uint8_t stable_data_version_ = 0;
    uint8_t leader_router_id_ = 0;

    // Router ID assignment table: true = assigned
    std::array<bool, MAX_ROUTER_IDS> router_id_table_{};

public:
    uint8_t partitionId() const { return partition_id_; }
    uint8_t leaderRouterId() const { return leader_router_id_; }
    void setLeaderRouterId(uint8_t id) { leader_router_id_ = id; }

    // Assign a router ID. Returns the assigned ID or nullopt if full.
    std::optional<uint8_t> assignRouterId();

    // Release a router ID
    void releaseRouterId(uint8_t id);

    // Check if a router ID is assigned
    bool isRouterIdAssigned(uint8_t id) const;

    // Count of assigned router IDs
    size_t assignedRouterCount() const;

    // Increment data version (call when network data changes)
    void bumpDataVersion() { ++data_version_; }
    uint8_t dataVersion() const { return data_version_; }
};

// Simplified leader election: highest extended address wins
struct LeaderElection {
    static NodeId electLeader(const std::array<uint64_t, 3>& ext_addrs);
};

} // namespace mt
