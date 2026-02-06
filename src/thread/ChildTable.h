#pragma once

#include "core/Types.h"
#include <vector>

namespace mt {

struct ChildEntry {
    uint16_t child_id;
    uint64_t ext_addr;
    RLOC16   rloc16;
    bool     sleepy;       // SED = true, MED = false
    Duration poll_period;  // SED polling interval
    TimePoint last_heard;
    uint32_t frame_counter;
};

class ChildTable {
    std::vector<ChildEntry> children_;
    uint16_t next_child_id_ = 1;

public:
    // Add a child. Returns assigned child ID.
    uint16_t addChild(uint64_t ext_addr, uint8_t parent_router_id, bool sleepy,
                       Duration poll_period, TimePoint now);

    // Remove a child by child ID
    void removeChild(uint16_t child_id);

    // Find child by extended address
    ChildEntry* findByExtAddr(uint64_t ext_addr);

    // Find child by RLOC16
    ChildEntry* findByRLOC16(RLOC16 rloc16);

    // Update last-heard time
    void markHeard(uint16_t child_id, TimePoint now);

    // Remove children not heard from within timeout
    void expireStale(TimePoint now, Duration timeout);

    const std::vector<ChildEntry>& children() const { return children_; }
    size_t size() const { return children_.size(); }
};

} // namespace mt
