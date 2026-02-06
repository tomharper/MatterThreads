#pragma once

#include "core/Types.h"
#include <array>
#include <vector>
#include <optional>

namespace mt {

enum class SessionType : uint8_t {
    Unsecured = 0,
    PASE = 1,
    CASE = 2
};

struct SessionParams {
    SessionId local_session_id = 0;
    SessionId peer_session_id = 0;
    SessionType type = SessionType::Unsecured;
    NodeId peer_node_id = INVALID_NODE;
    FabricIndex fabric_index = 0;
    std::array<uint8_t, 32> encryption_key{};
    uint32_t message_counter = 0;
    TimePoint last_activity{};
    Duration idle_timeout = Duration(60000);
    Duration active_timeout = Duration(300000);
    bool active = true;
};

class SessionManager {
    std::vector<SessionParams> sessions_;
    SessionId next_session_id_ = 1;

public:
    SessionId createSession(SessionType type, NodeId peer_node_id, FabricIndex fabric = 0);
    void destroySession(SessionId id);

    SessionParams* findSession(SessionId id);
    const SessionParams* findSession(SessionId id) const;

    SessionParams* findByPeer(NodeId peer_node_id, SessionType type);

    void updateActivity(SessionId id, TimePoint now);
    void expireIdleSessions(TimePoint now);

    uint32_t nextMessageCounter(SessionId id);

    const std::vector<SessionParams>& sessions() const { return sessions_; }
    size_t activeSessionCount() const;
};

} // namespace mt
