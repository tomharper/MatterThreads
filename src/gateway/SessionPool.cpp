#include "gateway/SessionPool.h"
#include "core/Log.h"

#include <algorithm>

namespace mt::gateway {

CASESessionPool::CASESessionPool(std::shared_ptr<hw::ChipToolDriver> driver,
                                   Duration keepalive_interval,
                                   Duration session_timeout)
    : driver_(std::move(driver))
    , keepalive_interval_(keepalive_interval)
    , session_timeout_(session_timeout) {}

Result<void> CASESessionPool::connect(const VanId& van_id, uint64_t device_id) {
    VanSession session;
    session.van_id = van_id;
    session.device_id = device_id;
    session.state = VanSessionState::Connecting;

    auto result = driver_->establishCASE(device_id);
    if (result.ok() && result->success) {
        session.state = VanSessionState::Connected;
        auto now = SteadyClock::now();
        session.connected_at = now;
        session.last_activity = now;
        session.last_keepalive = now;
        MT_INFO("gateway", "CASE session established for van " + van_id);
    } else {
        session.state = VanSessionState::Failed;
        session.next_reconnect = SteadyClock::now() + reconnect_base_;
        MT_WARN("gateway", "CASE session failed for van " + van_id);
    }

    sessions_[van_id] = session;
    notifyEvent(van_id, session.state);

    if (session.state == VanSessionState::Failed) {
        return Error{-1, "CASE session establishment failed"};
    }
    return Result<void>::success();
}

void CASESessionPool::disconnect(const VanId& van_id) {
    auto it = sessions_.find(van_id);
    if (it != sessions_.end()) {
        it->second.state = VanSessionState::Disconnected;
        notifyEvent(van_id, VanSessionState::Disconnected);
        sessions_.erase(it);
    }
}

VanSessionState CASESessionPool::sessionState(const VanId& van_id) const {
    auto it = sessions_.find(van_id);
    return it != sessions_.end() ? it->second.state : VanSessionState::Disconnected;
}

bool CASESessionPool::isConnected(const VanId& van_id) const {
    return sessionState(van_id) == VanSessionState::Connected;
}

std::vector<VanId> CASESessionPool::connectedVans() const {
    std::vector<VanId> result;
    for (const auto& [id, session] : sessions_) {
        if (session.state == VanSessionState::Connected) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<VanId> CASESessionPool::offlineVans() const {
    std::vector<VanId> result;
    for (const auto& [id, session] : sessions_) {
        if (session.state != VanSessionState::Connected) {
            result.push_back(id);
        }
    }
    return result;
}

void CASESessionPool::touchActivity(const VanId& van_id, TimePoint now) {
    auto it = sessions_.find(van_id);
    if (it != sessions_.end()) {
        it->second.last_activity = now;
    }
}

void CASESessionPool::tick(TimePoint now) {
    for (auto& [van_id, session] : sessions_) {
        if (session.state == VanSessionState::Connected) {
            // Check keepalive
            if (now - session.last_keepalive >= keepalive_interval_) {
                sendKeepalive(session, now);
            }
            // Check idle timeout
            if (now - session.last_activity >= session_timeout_) {
                session.state = VanSessionState::Disconnected;
                notifyEvent(van_id, VanSessionState::Disconnected);
                MT_INFO("gateway", "Session timed out for van " + van_id);
            }
        } else if (session.state == VanSessionState::Reconnecting) {
            if (now >= session.next_reconnect) {
                attemptReconnect(session, now);
            }
        }
    }
}

void CASESessionPool::onSessionEvent(SessionEventCallback cb) {
    event_callbacks_.push_back(std::move(cb));
}

size_t CASESessionPool::connectedCount() const {
    size_t count = 0;
    for (const auto& [id, s] : sessions_) {
        if (s.state == VanSessionState::Connected) ++count;
    }
    return count;
}

size_t CASESessionPool::reconnectingCount() const {
    size_t count = 0;
    for (const auto& [id, s] : sessions_) {
        if (s.state == VanSessionState::Reconnecting) ++count;
    }
    return count;
}

void CASESessionPool::attemptReconnect(VanSession& session, TimePoint now) {
    session.reconnect_attempts++;
    if (session.reconnect_attempts > max_reconnect_attempts_) {
        session.state = VanSessionState::Failed;
        notifyEvent(session.van_id, VanSessionState::Failed);
        MT_WARN("gateway", "Max reconnect attempts reached for van " + session.van_id);
        return;
    }

    auto result = driver_->establishCASE(session.device_id);
    if (result.ok() && result->success) {
        session.state = VanSessionState::Connected;
        session.connected_at = now;
        session.last_activity = now;
        session.last_keepalive = now;
        session.reconnect_attempts = 0;
        session.total_reconnects++;
        notifyEvent(session.van_id, VanSessionState::Connected);
        MT_INFO("gateway", "Reconnected van " + session.van_id);
    } else {
        session.next_reconnect = now + calculateBackoff(session.reconnect_attempts);
    }
}

Duration CASESessionPool::calculateBackoff(uint32_t attempt) const {
    auto delay = reconnect_base_.count();
    for (uint32_t i = 1; i < attempt && delay < reconnect_max_.count(); ++i) {
        delay *= 2;
    }
    return Duration(std::min(delay, reconnect_max_.count()));
}

void CASESessionPool::sendKeepalive(VanSession& session, TimePoint now) {
    // Lightweight read of BasicInformation:SoftwareVersion (ep 0, cluster 0x0028, attr 0x0009)
    auto result = driver_->readAttribute(session.device_id, 0, 0x0028, 0x0009);
    session.last_keepalive = now;
    if (!result.ok() || !result->success) {
        session.state = VanSessionState::Reconnecting;
        session.next_reconnect = now + reconnect_base_;
        session.reconnect_attempts = 0;
        notifyEvent(session.van_id, VanSessionState::Reconnecting);
        MT_WARN("gateway", "Keepalive failed for van " + session.van_id);
    } else {
        session.last_activity = now;
    }
}

void CASESessionPool::notifyEvent(const VanId& van_id, VanSessionState state) {
    for (const auto& cb : event_callbacks_) {
        cb(van_id, state);
    }
}

} // namespace mt::gateway
