#include "matter/CASE.h"
#include "core/Log.h"

namespace mt {

Result<void> CASESession::startSession(NodeId peer_node_id, FabricIndex fabric_index) {
    peer_node_id_ = peer_node_id;
    fabric_index_ = fabric_index;
    state_ = State::Sigma1;
    MT_DEBUG("case", "Starting CASE session with node " + std::to_string(peer_node_id));
    return Result<void>::success();
}

Result<void> CASESession::handleSigma1() {
    if (state_ != State::Idle) return Error("Invalid state for Sigma1");
    state_ = State::Sigma2;
    MT_TRACE("case", "Received Sigma1, sending Sigma2");
    return Result<void>::success();
}

Result<void> CASESession::handleSigma2() {
    if (state_ != State::Sigma1) return Error("Invalid state for Sigma2");
    state_ = State::Sigma3;
    MT_TRACE("case", "Received Sigma2, sending Sigma3");
    return Result<void>::success();
}

Result<void> CASESession::handleSigma3() {
    if (state_ != State::Sigma2) return Error("Invalid state for Sigma3");
    state_ = State::Complete;
    MT_INFO("case", "CASE session established with node " + std::to_string(peer_node_id_));
    return Result<void>::success();
}

std::optional<SessionParams> CASESession::getEstablishedSession() const {
    if (state_ != State::Complete) return std::nullopt;
    SessionParams params;
    params.type = SessionType::CASE;
    params.peer_node_id = peer_node_id_;
    params.fabric_index = fabric_index_;
    params.active = true;
    return params;
}

} // namespace mt
