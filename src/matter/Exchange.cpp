#include "matter/Exchange.h"
#include "core/Log.h"
#include <string>

namespace mt {

ExchangeId ExchangeManager::initiateExchange(SessionId session_id, uint8_t protocol_id, uint8_t opcode) {
    ExchangeEntry entry;
    entry.exchange_id = next_id_++;
    entry.session_id = session_id;
    entry.initiator = true;
    entry.protocol_id = protocol_id;
    entry.opcode = opcode;
    entry.awaiting_response = true;

    auto id = entry.exchange_id;
    exchanges_[id] = std::move(entry);
    return id;
}

void ExchangeManager::setResponseHandler(ExchangeId id,
                                           std::function<void(const std::vector<uint8_t>&)> handler) {
    if (auto* ex = findExchange(id)) {
        ex->on_response = std::move(handler);
    }
}

void ExchangeManager::setTimeoutHandler(ExchangeId id, std::function<void()> handler) {
    if (auto* ex = findExchange(id)) {
        ex->on_timeout = std::move(handler);
    }
}

void ExchangeManager::onMessageReceived(ExchangeId id, const std::vector<uint8_t>& payload) {
    auto* ex = findExchange(id);
    if (!ex) return;

    ex->awaiting_response = false;
    if (ex->on_response) {
        ex->on_response(payload);
    }
}

void ExchangeManager::completeExchange(ExchangeId id) {
    auto it = exchanges_.find(id);
    if (it != exchanges_.end()) {
        it->second.completed = true;
    }
}

void ExchangeManager::tick(TimePoint now) {
    std::vector<ExchangeId> to_remove;

    for (auto& [id, ex] : exchanges_) {
        if (ex.completed) {
            to_remove.push_back(id);
            continue;
        }

        if (!ex.awaiting_response) continue;

        auto elapsed = now - ex.sent_time;
        if (elapsed > ex.timeout) {
            if (ex.retransmit_count < ExchangeEntry::MAX_RETRANSMITS) {
                ++ex.retransmit_count;
                ex.sent_time = now;
                MT_DEBUG("exchange", "Retransmit #" + std::to_string(ex.retransmit_count) +
                         " for exchange " + std::to_string(id));
            } else {
                // Timeout — give up
                MT_WARN("exchange", "Exchange " + std::to_string(id) + " timed out");
                if (ex.on_timeout) {
                    ex.on_timeout();
                }
                to_remove.push_back(id);
            }
        }
    }

    for (auto id : to_remove) {
        exchanges_.erase(id);
    }
}

ExchangeEntry* ExchangeManager::findExchange(ExchangeId id) {
    auto it = exchanges_.find(id);
    return (it != exchanges_.end()) ? &it->second : nullptr;
}

size_t ExchangeManager::activeCount() const {
    size_t count = 0;
    for (const auto& [_, ex] : exchanges_) {
        if (!ex.completed) ++count;
    }
    return count;
}

} // namespace mt
