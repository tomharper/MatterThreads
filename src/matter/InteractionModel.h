#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/DataModel.h"
#include "matter/Session.h"
#include "matter/Exchange.h"
#include "matter/SubscriptionManager.h"
#include <vector>

namespace mt {

// Interaction Model opcodes
namespace IMOpcodes {
    static constexpr uint8_t ReadRequest     = 0x02;
    static constexpr uint8_t ReportData      = 0x05;
    static constexpr uint8_t SubscribeRequest = 0x03;
    static constexpr uint8_t SubscribeResponse = 0x04;
    static constexpr uint8_t WriteRequest    = 0x06;
    static constexpr uint8_t WriteResponse   = 0x07;
    static constexpr uint8_t InvokeRequest   = 0x08;
    static constexpr uint8_t InvokeResponse  = 0x09;
    static constexpr uint8_t StatusResponse  = 0x01;
}

struct AttributeReport {
    AttributePath path;
    AttributeValue value;
    bool has_error = false;
    int status_code = 0;
};

struct ReportData {
    std::vector<AttributeReport> attribute_reports;
    SubscriptionId subscription_id = 0;
    bool suppress_response = false;
};

struct WriteRequestData {
    std::vector<std::pair<AttributePath, AttributeValue>> writes;
    bool timed = false;
};

struct InvokeRequestData {
    CommandPath command;
    std::vector<uint8_t> command_fields;
    bool timed = false;
};

struct InvokeResponseData {
    CommandPath command;
    int status_code = 0;
    std::vector<uint8_t> response_fields;
};

class InteractionModel {
    DataModel& data_model_;
    SessionManager& sessions_;
    SubscriptionManager& subscriptions_;
    ExchangeManager& exchanges_;

public:
    InteractionModel(DataModel& dm, SessionManager& sm,
                      SubscriptionManager& sub, ExchangeManager& ex)
        : data_model_(dm), sessions_(sm), subscriptions_(sub), exchanges_(ex) {}

    // Initiator-side operations
    Result<ReportData> sendRead(SessionId session_id, const std::vector<AttributePath>& paths);

    Result<SubscriptionId> sendSubscribe(SessionId session_id,
                                          const std::vector<AttributePath>& paths,
                                          Duration min_interval, Duration max_interval);

    Result<void> sendWrite(SessionId session_id, const WriteRequestData& req);

    Result<InvokeResponseData> sendInvoke(SessionId session_id, const InvokeRequestData& req);

    // Responder-side handlers (called when messages arrive)
    ReportData handleReadRequest(SessionId session_id, const std::vector<AttributePath>& paths);
    void handleWriteRequest(SessionId session_id, const WriteRequestData& req);

    // Tick: process exchanges, subscriptions
    void tick(TimePoint now);
};

} // namespace mt
