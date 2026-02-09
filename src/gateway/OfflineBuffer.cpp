#include "gateway/OfflineBuffer.h"

#include <algorithm>

namespace mt::gateway {

OfflineBuffer::OfflineBuffer(size_t max_events_per_van, size_t max_total_events)
    : max_per_van_(max_events_per_van), max_total_(max_total_events) {}

void OfflineBuffer::push(const VanId& van_id, const std::string& event_type,
                          nlohmann::json payload, TimePoint timestamp) {
    BufferedEvent evt;
    evt.sequence_id = next_sequence_id_++;
    evt.van_id = van_id;
    evt.timestamp = timestamp;
    evt.event_type = event_type;
    evt.payload = std::move(payload);

    buffers_[van_id].push_back(std::move(evt));
    ++total_count_;

    enforcePerVanLimit(van_id);
    enforceTotalLimit();
}

std::vector<BufferedEvent> OfflineBuffer::drain(const VanId& van_id,
                                                 uint64_t since_sequence) const {
    std::vector<BufferedEvent> result;
    auto it = buffers_.find(van_id);
    if (it == buffers_.end()) return result;

    for (const auto& evt : it->second) {
        if (evt.sequence_id > since_sequence) {
            result.push_back(evt);
        }
    }
    return result;
}

std::vector<BufferedEvent> OfflineBuffer::drainSince(const VanId& van_id,
                                                      TimePoint since) const {
    std::vector<BufferedEvent> result;
    auto it = buffers_.find(van_id);
    if (it == buffers_.end()) return result;

    for (const auto& evt : it->second) {
        if (evt.timestamp > since) {
            result.push_back(evt);
        }
    }
    return result;
}

std::vector<BufferedEvent> OfflineBuffer::drainAll(uint64_t since_sequence) const {
    std::vector<BufferedEvent> result;
    for (const auto& [van_id, deq] : buffers_) {
        for (const auto& evt : deq) {
            if (evt.sequence_id > since_sequence) {
                result.push_back(evt);
            }
        }
    }
    // Sort by sequence_id for consistent ordering
    std::sort(result.begin(), result.end(),
              [](const BufferedEvent& a, const BufferedEvent& b) {
                  return a.sequence_id < b.sequence_id;
              });
    return result;
}

void OfflineBuffer::clear(const VanId& van_id) {
    auto it = buffers_.find(van_id);
    if (it != buffers_.end()) {
        total_count_ -= it->second.size();
        buffers_.erase(it);
    }
}

void OfflineBuffer::clearAll() {
    buffers_.clear();
    total_count_ = 0;
}

void OfflineBuffer::evict(Duration max_age, TimePoint now) {
    auto cutoff = now - max_age;
    for (auto& [van_id, deq] : buffers_) {
        size_t removed = 0;
        while (!deq.empty() && deq.front().timestamp < cutoff) {
            deq.pop_front();
            ++removed;
        }
        total_count_ -= removed;
    }
}

size_t OfflineBuffer::eventCount(const VanId& van_id) const {
    auto it = buffers_.find(van_id);
    return it != buffers_.end() ? it->second.size() : 0;
}

size_t OfflineBuffer::totalEventCount() const {
    return total_count_;
}

void OfflineBuffer::enforcePerVanLimit(const VanId& van_id) {
    auto& deq = buffers_[van_id];
    while (deq.size() > max_per_van_) {
        deq.pop_front();
        --total_count_;
    }
}

void OfflineBuffer::enforceTotalLimit() {
    // Evict oldest events across all vans when total exceeds limit
    while (total_count_ > max_total_) {
        // Find the van with the oldest front event
        VanId oldest_van;
        uint64_t oldest_seq = UINT64_MAX;
        for (auto& [van_id, deq] : buffers_) {
            if (!deq.empty() && deq.front().sequence_id < oldest_seq) {
                oldest_seq = deq.front().sequence_id;
                oldest_van = van_id;
            }
        }
        if (oldest_van.empty()) break;
        buffers_[oldest_van].pop_front();
        --total_count_;
    }
}

} // namespace mt::gateway
