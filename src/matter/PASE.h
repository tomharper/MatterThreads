#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/Session.h"
#include <optional>

namespace mt {

class PASESession {
public:
    enum class State : uint8_t {
        Idle = 0,
        PBKDFParamRequest = 1,
        PBKDFParamResponse = 2,
        PASE1 = 3,
        PASE2 = 4,
        PASE3 = 5,
        Complete = 6,
        Failed = 7
    };

    // Initiator side
    Result<void> startPairing(uint32_t setup_code, NodeId peer_node_id);

    // Responder side
    Result<void> handlePBKDFParamRequest();
    Result<void> handlePASE1();
    Result<void> handlePASE3();

    // Initiator receives responses
    Result<void> handlePBKDFParamResponse();
    Result<void> handlePASE2();

    State state() const { return state_; }
    bool isComplete() const { return state_ == State::Complete; }
    bool isFailed() const { return state_ == State::Failed; }

    std::optional<SessionParams> getEstablishedSession() const;

    NodeId peerNodeId() const { return peer_node_id_; }
    uint32_t setupCode() const { return setup_code_; }

    // Timing
    void setStartTime(TimePoint t) { start_time_ = t; }
    TimePoint startTime() const { return start_time_; }

private:
    State state_ = State::Idle;
    uint32_t setup_code_ = 0;
    NodeId peer_node_id_ = INVALID_NODE;
    SessionId established_session_id_ = 0;
    TimePoint start_time_{};
};

} // namespace mt
