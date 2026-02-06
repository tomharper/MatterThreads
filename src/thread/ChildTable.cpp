#include "thread/ChildTable.h"
#include "core/Types.h"
#include <algorithm>

namespace mt {

uint16_t ChildTable::addChild(uint64_t ext_addr, uint8_t parent_router_id,
                                bool sleepy, Duration poll_period, TimePoint now) {
    uint16_t cid = next_child_id_++;
    RLOC16 rloc = makeRLOC16(parent_router_id, cid);

    children_.push_back(ChildEntry{
        .child_id = cid,
        .ext_addr = ext_addr,
        .rloc16 = rloc,
        .sleepy = sleepy,
        .poll_period = poll_period,
        .last_heard = now,
        .frame_counter = 0
    });

    return cid;
}

void ChildTable::removeChild(uint16_t child_id) {
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
            [child_id](const ChildEntry& c) { return c.child_id == child_id; }),
        children_.end());
}

ChildEntry* ChildTable::findByExtAddr(uint64_t ext_addr) {
    for (auto& c : children_) {
        if (c.ext_addr == ext_addr) return &c;
    }
    return nullptr;
}

ChildEntry* ChildTable::findByRLOC16(RLOC16 rloc16) {
    for (auto& c : children_) {
        if (c.rloc16 == rloc16) return &c;
    }
    return nullptr;
}

void ChildTable::markHeard(uint16_t child_id, TimePoint now) {
    for (auto& c : children_) {
        if (c.child_id == child_id) {
            c.last_heard = now;
            return;
        }
    }
}

void ChildTable::expireStale(TimePoint now, Duration timeout) {
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
            [now, timeout](const ChildEntry& c) { return (now - c.last_heard) > timeout; }),
        children_.end());
}

} // namespace mt
