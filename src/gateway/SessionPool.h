#pragma once

#include "gateway/GatewayTypes.h"
#include "core/Result.h"
#include "hw/ChipToolDriver.h"

#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace mt::gateway {

enum class VanSessionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

inline std::string sessionStateToString(VanSessionState s) {
    switch (s) {
        case VanSessionState::Disconnected:  return "disconnected";
        case VanSessionState::Connecting:    return "connecting";
        case VanSessionState::Connected:     return "connected";
        case VanSessionState::Reconnecting:  return "reconnecting";
        case VanSessionState::Failed:        return "failed";
    }
    return "unknown";
}

struct VanSession {
    VanId van_id;
    uint64_t device_id = 0;
    VanSessionState state = VanSessionState::Disconnected;
    TimePoint connected_at{};
    TimePoint last_activity{};
    TimePoint last_keepalive{};
    TimePoint next_reconnect{};
    uint32_t reconnect_attempts = 0;
    uint32_t total_reconnects = 0;
};

using SessionEventCallback = std::function<void(const VanId&, VanSessionState)>;

class CASESessionPool {
public:
    explicit CASESessionPool(std::shared_ptr<hw::ChipToolDriver> driver,
                              Duration keepalive_interval = Duration(60000),
                              Duration session_timeout = Duration(300000));

    Result<void> connect(const VanId& van_id, uint64_t device_id);
    void disconnect(const VanId& van_id);

    VanSessionState sessionState(const VanId& van_id) const;
    bool isConnected(const VanId& van_id) const;

    std::vector<VanId> connectedVans() const;
    std::vector<VanId> offlineVans() const;

    void touchActivity(const VanId& van_id, TimePoint now);
    void tick(TimePoint now);

    void onSessionEvent(SessionEventCallback cb);

    size_t totalSessions() const { return sessions_.size(); }
    size_t connectedCount() const;
    size_t reconnectingCount() const;

    // Reconnect config
    void setReconnectBase(Duration base) { reconnect_base_ = base; }
    void setReconnectMax(Duration max) { reconnect_max_ = max; }
    void setMaxReconnectAttempts(uint32_t max) { max_reconnect_attempts_ = max; }

private:
    std::shared_ptr<hw::ChipToolDriver> driver_;
    std::unordered_map<VanId, VanSession> sessions_;
    std::vector<SessionEventCallback> event_callbacks_;

    Duration keepalive_interval_;
    Duration session_timeout_;
    Duration reconnect_base_{5000};
    Duration reconnect_max_{300000};
    uint32_t max_reconnect_attempts_ = 10;

    void attemptReconnect(VanSession& session, TimePoint now);
    Duration calculateBackoff(uint32_t attempt) const;
    void sendKeepalive(VanSession& session, TimePoint now);
    void notifyEvent(const VanId& van_id, VanSessionState state);
};

} // namespace mt::gateway
