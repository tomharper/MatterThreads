#include "matter/PASE.h"
#include "core/Log.h"

namespace mt {

Result<void> PASESession::startPairing(uint32_t setup_code, NodeId peer_node_id) {
    setup_code_ = setup_code;
    peer_node_id_ = peer_node_id;
    state_ = State::PBKDFParamRequest;
    MT_DEBUG("pase", "Starting PASE pairing with node " + std::to_string(peer_node_id) +
             " (setup code: " + std::to_string(setup_code) + ")");
    return Result<void>::success();
}

Result<void> PASESession::handlePBKDFParamRequest() {
    if (state_ != State::Idle) return Error("Invalid state for PBKDFParamRequest");
    state_ = State::PBKDFParamResponse;
    MT_TRACE("pase", "Received PBKDFParamRequest, sending response");
    return Result<void>::success();
}

Result<void> PASESession::handlePBKDFParamResponse() {
    if (state_ != State::PBKDFParamRequest) return Error("Invalid state for PBKDFParamResponse");
    state_ = State::PASE1;
    MT_TRACE("pase", "Received PBKDFParamResponse, sending PASE1");
    return Result<void>::success();
}

Result<void> PASESession::handlePASE1() {
    if (state_ != State::PBKDFParamResponse) return Error("Invalid state for PASE1");
    state_ = State::PASE2;
    MT_TRACE("pase", "Received PASE1, sending PASE2");
    return Result<void>::success();
}

Result<void> PASESession::handlePASE2() {
    if (state_ != State::PASE1) return Error("Invalid state for PASE2");
    state_ = State::PASE3;
    MT_TRACE("pase", "Received PASE2, sending PASE3");
    return Result<void>::success();
}

Result<void> PASESession::handlePASE3() {
    if (state_ != State::PASE2) return Error("Invalid state for PASE3");
    state_ = State::Complete;
    MT_INFO("pase", "PASE session established with node " + std::to_string(peer_node_id_));
    return Result<void>::success();
}

std::optional<SessionParams> PASESession::getEstablishedSession() const {
    if (state_ != State::Complete) return std::nullopt;
    SessionParams params;
    params.type = SessionType::PASE;
    params.peer_node_id = peer_node_id_;
    params.active = true;
    return params;
}

} // namespace mt
