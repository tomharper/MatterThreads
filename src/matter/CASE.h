#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/Session.h"
#include <optional>

namespace mt {

class CASESession {
public:
    enum class State : uint8_t {
        Idle = 0,
        Sigma1 = 1,
        Sigma2 = 2,
        Sigma3 = 3,
        Complete = 4,
        Failed = 5
    };

    // Initiator
    Result<void> startSession(NodeId peer_node_id, FabricIndex fabric_index);

    // Responder
    Result<void> handleSigma1();
    Result<void> handleSigma3();

    // Initiator receives
    Result<void> handleSigma2();

    State state() const { return state_; }
    bool isComplete() const { return state_ == State::Complete; }
    bool isFailed() const { return state_ == State::Failed; }

    std::optional<SessionParams> getEstablishedSession() const;

    NodeId peerNodeId() const { return peer_node_id_; }
    FabricIndex fabricIndex() const { return fabric_index_; }

private:
    State state_ = State::Idle;
    NodeId peer_node_id_ = INVALID_NODE;
    FabricIndex fabric_index_ = 0;
};

} // namespace mt
