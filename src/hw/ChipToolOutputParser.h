#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/DataModel.h"
#include "matter/Session.h"

#include <string>
#include <vector>
#include <optional>

namespace mt::hw {

struct PairingResult {
    bool success = false;
    uint64_t node_id = 0;
    std::string error_message;
    SessionType session_type = SessionType::Unsecured;
};

struct ReadAttributeResult {
    bool success = false;
    AttributePath path{};
    AttributeValue value;
    int status_code = 0;
    std::string error_message;
};

struct WriteAttributeResult {
    bool success = false;
    int status_code = 0;
    std::string error_message;
};

struct InvokeResult {
    bool success = false;
    int status_code = 0;
    std::vector<uint8_t> response_data;
    std::string error_message;
};

struct SubscribeResult {
    bool success = false;
    SubscriptionId subscription_id = 0;
    std::string error_message;
};

class ChipToolOutputParser {
public:
    static Result<PairingResult> parsePairingOutput(
        const std::string& stdout_data, const std::string& stderr_data, int exit_code);

    static Result<ReadAttributeResult> parseReadOutput(
        const std::string& stdout_data, const std::string& stderr_data, int exit_code);

    static Result<WriteAttributeResult> parseWriteOutput(
        const std::string& stdout_data, const std::string& stderr_data, int exit_code);

    static Result<InvokeResult> parseInvokeOutput(
        const std::string& stdout_data, const std::string& stderr_data, int exit_code);

    static Result<SubscribeResult> parseSubscribeOutput(
        const std::string& stdout_data, const std::string& stderr_data, int exit_code);

    static std::optional<uint64_t> extractNodeId(const std::string& output);

    static Result<AttributeValue> parseTLVValue(
        const std::string& type_hint, const std::string& raw_value);

    static bool hasChipError(const std::string& stderr_data);
    static std::optional<int> extractChipErrorCode(const std::string& stderr_data);

private:
    static std::string findLine(const std::string& data, const std::string& pattern);
    static std::string extractAfter(const std::string& line, const std::string& prefix);
};

} // namespace mt::hw
