#include "thread/Leader.h"
#include <algorithm>

namespace mt {

std::optional<uint8_t> LeaderData::assignRouterId() {
    for (uint8_t i = 0; i < MAX_ROUTER_IDS; ++i) {
        if (!router_id_table_[i]) {
            router_id_table_[i] = true;
            return i;
        }
    }
    return std::nullopt;
}

void LeaderData::releaseRouterId(uint8_t id) {
    if (id < MAX_ROUTER_IDS) {
        router_id_table_[id] = false;
    }
}

bool LeaderData::isRouterIdAssigned(uint8_t id) const {
    return id < MAX_ROUTER_IDS && router_id_table_[id];
}

size_t LeaderData::assignedRouterCount() const {
    size_t count = 0;
    for (bool assigned : router_id_table_) {
        if (assigned) ++count;
    }
    return count;
}

NodeId LeaderElection::electLeader(const std::array<uint64_t, 3>& ext_addrs) {
    NodeId leader = 0;
    for (NodeId i = 1; i < 3; ++i) {
        if (ext_addrs[i] > ext_addrs[leader]) {
            leader = i;
        }
    }
    return leader;
}

} // namespace mt
