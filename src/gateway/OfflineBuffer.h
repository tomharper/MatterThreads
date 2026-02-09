#pragma once

#include "gateway/GatewayTypes.h"

#include <nlohmann/json.hpp>
#include <deque>
#include <unordered_map>
#include <vector>

namespace mt::gateway {

struct BufferedEvent {
    uint64_t sequence_id = 0;
    VanId van_id;
    TimePoint timestamp{};
    std::string event_type;
    nlohmann::json payload;
};

class OfflineBuffer {
public:
    explicit OfflineBuffer(size_t max_events_per_van = 10000,
                           size_t max_total_events = 100000);

    void push(const VanId& van_id, const std::string& event_type,
              nlohmann::json payload, TimePoint timestamp);

    std::vector<BufferedEvent> drain(const VanId& van_id,
                                     uint64_t since_sequence = 0) const;
    std::vector<BufferedEvent> drainSince(const VanId& van_id,
                                          TimePoint since) const;
    std::vector<BufferedEvent> drainAll(uint64_t since_sequence = 0) const;

    void clear(const VanId& van_id);
    void clearAll();

    void evict(Duration max_age, TimePoint now);

    size_t eventCount(const VanId& van_id) const;
    size_t totalEventCount() const;
    uint64_t latestSequenceId() const { return next_sequence_id_ - 1; }

private:
    size_t max_per_van_;
    size_t max_total_;
    uint64_t next_sequence_id_ = 1;
    size_t total_count_ = 0;

    std::unordered_map<VanId, std::deque<BufferedEvent>> buffers_;

    void enforcePerVanLimit(const VanId& van_id);
    void enforceTotalLimit();
};

} // namespace mt::gateway
