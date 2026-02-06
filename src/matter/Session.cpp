#include "matter/Session.h"
#include <algorithm>

namespace mt {

SessionId SessionManager::createSession(SessionType type, NodeId peer_node_id, FabricIndex fabric) {
    SessionParams params;
    params.local_session_id = next_session_id_++;
    params.type = type;
    params.peer_node_id = peer_node_id;
    params.fabric_index = fabric;
    params.active = true;
    sessions_.push_back(params);
    return params.local_session_id;
}

void SessionManager::destroySession(SessionId id) {
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [id](const SessionParams& s) { return s.local_session_id == id; }),
        sessions_.end());
}

SessionParams* SessionManager::findSession(SessionId id) {
    for (auto& s : sessions_) {
        if (s.local_session_id == id && s.active) return &s;
    }
    return nullptr;
}

const SessionParams* SessionManager::findSession(SessionId id) const {
    for (const auto& s : sessions_) {
        if (s.local_session_id == id && s.active) return &s;
    }
    return nullptr;
}

SessionParams* SessionManager::findByPeer(NodeId peer_node_id, SessionType type) {
    for (auto& s : sessions_) {
        if (s.peer_node_id == peer_node_id && s.type == type && s.active) return &s;
    }
    return nullptr;
}

void SessionManager::updateActivity(SessionId id, TimePoint now) {
    if (auto* s = findSession(id)) {
        s->last_activity = now;
    }
}

void SessionManager::expireIdleSessions(TimePoint now) {
    for (auto& s : sessions_) {
        if (!s.active) continue;
        auto elapsed = now - s.last_activity;
        if (elapsed > s.idle_timeout) {
            s.active = false;
        }
    }
}

uint32_t SessionManager::nextMessageCounter(SessionId id) {
    if (auto* s = findSession(id)) {
        return s->message_counter++;
    }
    return 0;
}

size_t SessionManager::activeSessionCount() const {
    size_t count = 0;
    for (const auto& s : sessions_) {
        if (s.active) ++count;
    }
    return count;
}

} // namespace mt
