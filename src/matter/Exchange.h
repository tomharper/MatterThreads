#pragma once

#include "core/Types.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace mt {

static constexpr uint8_t PROTOCOL_SECURE_CHANNEL = 0x00;
static constexpr uint8_t PROTOCOL_INTERACTION_MODEL = 0x01;

struct ExchangeEntry {
    ExchangeId exchange_id;
    SessionId session_id;
    bool initiator;
    uint8_t protocol_id;
    uint8_t opcode;
    TimePoint sent_time{};
    Duration timeout = Duration(30000);
    uint8_t retransmit_count = 0;
    static constexpr uint8_t MAX_RETRANSMITS = 3;
    bool awaiting_response = false;
    bool completed = false;
    std::function<void(const std::vector<uint8_t>&)> on_response;
    std::function<void()> on_timeout;
};

class ExchangeManager {
    std::unordered_map<ExchangeId, ExchangeEntry> exchanges_;
    ExchangeId next_id_ = 1;

public:
    ExchangeId initiateExchange(SessionId session_id, uint8_t protocol_id, uint8_t opcode);

    void setResponseHandler(ExchangeId id,
                             std::function<void(const std::vector<uint8_t>&)> handler);
    void setTimeoutHandler(ExchangeId id, std::function<void()> handler);

    void onMessageReceived(ExchangeId id, const std::vector<uint8_t>& payload);
    void completeExchange(ExchangeId id);

    // Check timeouts, trigger retransmissions
    void tick(TimePoint now);

    ExchangeEntry* findExchange(ExchangeId id);
    size_t activeCount() const;
};

} // namespace mt
