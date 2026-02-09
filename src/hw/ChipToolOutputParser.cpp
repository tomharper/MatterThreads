#include "hw/ChipToolOutputParser.h"
#include "core/Log.h"

#include <regex>
#include <sstream>
#include <algorithm>

namespace mt::hw {

std::string ChipToolOutputParser::findLine(const std::string& data, const std::string& pattern) {
    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(pattern) != std::string::npos) {
            return line;
        }
    }
    return "";
}

std::string ChipToolOutputParser::extractAfter(const std::string& line, const std::string& prefix) {
    auto pos = line.find(prefix);
    if (pos == std::string::npos) return "";
    auto start = pos + prefix.size();
    // Skip leading whitespace
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    // Trim trailing whitespace
    auto end = line.size();
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r')) --end;
    return line.substr(start, end - start);
}

Result<PairingResult> ChipToolOutputParser::parsePairingOutput(
    const std::string& stdout_data, const std::string& stderr_data, int exit_code) {

    PairingResult result;

    if (exit_code != 0) {
        result.success = false;
        if (hasChipError(stderr_data)) {
            result.error_message = findLine(stderr_data, "CHIP Error");
        } else {
            result.error_message = "chip-tool exited with code " + std::to_string(exit_code);
        }
        return result;
    }

    // Check for PASE session establishment
    std::string combined = stdout_data + "\n" + stderr_data;
    if (combined.find("PASE session established") != std::string::npos ||
        combined.find("PASESession") != std::string::npos) {
        result.session_type = SessionType::PASE;
        result.success = true;
    }

    // Check for CASE session
    if (combined.find("CASE session established") != std::string::npos ||
        combined.find("CASESession") != std::string::npos) {
        result.session_type = SessionType::CASE;
        result.success = true;
    }

    // Check for general pairing success
    if (combined.find("Device commissioning completed with success") != std::string::npos ||
        combined.find("Successfully finished commissioning") != std::string::npos) {
        result.success = true;
    }

    // Extract node ID
    auto node_id = extractNodeId(combined);
    if (node_id.has_value()) {
        result.node_id = *node_id;
    }

    if (!result.success) {
        result.error_message = "Pairing did not complete successfully";
    }

    return result;
}

Result<ReadAttributeResult> ChipToolOutputParser::parseReadOutput(
    const std::string& stdout_data, const std::string& stderr_data, int exit_code) {

    ReadAttributeResult result;

    if (exit_code != 0) {
        result.success = false;
        result.error_message = "chip-tool read exited with code " + std::to_string(exit_code);
        return result;
    }

    std::string combined = stdout_data + "\n" + stderr_data;

    // Look for "CHIP:TOO:" lines with attribute values
    // Pattern: "CHIP:TOO:   OnOff: TRUE" or "CHIP:TOO:   CurrentLevel: 42"
    std::istringstream stream(combined);
    std::string line;
    bool found_report = false;

    while (std::getline(stream, line)) {
        // Check for report data marker
        if (line.find("Received Read response") != std::string::npos ||
            line.find("ReportDataMessage") != std::string::npos) {
            found_report = true;
        }

        // Parse CHIP:TOO attribute lines
        auto too_pos = line.find("CHIP:TOO:");
        if (too_pos != std::string::npos) {
            auto content = line.substr(too_pos + 9);
            // Trim leading whitespace
            auto start = content.find_first_not_of(" \t");
            if (start != std::string::npos) {
                content = content.substr(start);
            }

            // Skip header/marker lines
            if (content.find("Endpoint:") != std::string::npos) continue;
            if (content.find("cluster:") != std::string::npos) continue;

            // Parse "Name: Value" pattern
            auto colon = content.find(':');
            if (colon != std::string::npos && colon + 1 < content.size()) {
                auto type_hint = content.substr(0, colon);
                auto raw_val = content.substr(colon + 1);
                // Trim
                while (!raw_val.empty() && raw_val[0] == ' ') raw_val = raw_val.substr(1);

                auto parsed = parseTLVValue(type_hint, raw_val);
                if (parsed.ok()) {
                    result.value = *parsed;
                    result.success = true;
                }
            }
        }
    }

    if (!result.success && found_report) {
        result.error_message = "Report received but could not parse attribute value";
    } else if (!result.success) {
        result.error_message = "No report data in chip-tool output";
    }

    return result;
}

Result<WriteAttributeResult> ChipToolOutputParser::parseWriteOutput(
    const std::string& stdout_data, const std::string& stderr_data, int exit_code) {

    WriteAttributeResult result;

    if (exit_code != 0) {
        result.success = false;
        result.error_message = "chip-tool write exited with code " + std::to_string(exit_code);
        return result;
    }

    std::string combined = stdout_data + "\n" + stderr_data;

    if (combined.find("WriteResponseMessage") != std::string::npos ||
        combined.find("status = 0x0") != std::string::npos ||
        combined.find("SUCCESS") != std::string::npos) {
        result.success = true;
        result.status_code = 0;
    } else {
        result.success = false;
        result.error_message = "Write response not confirmed";
    }

    return result;
}

Result<InvokeResult> ChipToolOutputParser::parseInvokeOutput(
    const std::string& stdout_data, const std::string& stderr_data, int exit_code) {

    InvokeResult result;

    if (exit_code != 0) {
        result.success = false;
        result.error_message = "chip-tool invoke exited with code " + std::to_string(exit_code);
        return result;
    }

    std::string combined = stdout_data + "\n" + stderr_data;

    if (combined.find("InvokeResponseMessage") != std::string::npos ||
        combined.find("status = 0x0") != std::string::npos ||
        combined.find("SUCCESS") != std::string::npos) {
        result.success = true;
        result.status_code = 0;
    } else {
        result.success = false;
        result.error_message = "Invoke response not confirmed";
    }

    return result;
}

Result<SubscribeResult> ChipToolOutputParser::parseSubscribeOutput(
    const std::string& stdout_data, const std::string& stderr_data, int exit_code) {

    SubscribeResult result;

    std::string combined = stdout_data + "\n" + stderr_data;

    // For subscribe, the process stays running. We check the initial output
    // for subscription confirmation
    if (combined.find("SubscribeResponse") != std::string::npos ||
        combined.find("Subscription established") != std::string::npos) {
        result.success = true;

        // Try to extract subscription ID
        std::regex sub_id_regex(R"(SubscriptionId\s*=\s*(\d+))");
        std::smatch match;
        if (std::regex_search(combined, match, sub_id_regex)) {
            result.subscription_id = static_cast<SubscriptionId>(std::stoul(match[1].str()));
        }
    } else if (exit_code != 0) {
        result.success = false;
        result.error_message = "chip-tool subscribe exited with code " + std::to_string(exit_code);
    } else {
        // Process might still be starting
        result.success = false;
        result.error_message = "Subscription not yet confirmed";
    }

    return result;
}

std::optional<uint64_t> ChipToolOutputParser::extractNodeId(const std::string& output) {
    // Pattern: "node id 0x..." or "Node ID: ..." or "NodeId = ..."
    std::regex node_regex(R"((?:node\s*id|NodeId)\s*[=:\s]\s*(?:0x)?([0-9a-fA-F]+))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(output, match, node_regex)) {
        try {
            return std::stoull(match[1].str(), nullptr, 16);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

Result<AttributeValue> ChipToolOutputParser::parseTLVValue(
    const std::string& type_hint, const std::string& raw_value) {

    // Boolean values
    if (raw_value == "TRUE" || raw_value == "true" || raw_value == "1") {
        return AttributeValue{true};
    }
    if (raw_value == "FALSE" || raw_value == "false" || raw_value == "0") {
        return AttributeValue{false};
    }

    // NULL
    if (raw_value == "NULL" || raw_value == "null") {
        return Error("Null attribute value");
    }

    // String values (quoted)
    if (raw_value.size() >= 2 && raw_value.front() == '"' && raw_value.back() == '"') {
        return AttributeValue{raw_value.substr(1, raw_value.size() - 2)};
    }

    // Numeric values
    try {
        // Check for float/double
        if (raw_value.find('.') != std::string::npos) {
            return AttributeValue{std::stod(raw_value)};
        }

        // Check for negative integer
        if (!raw_value.empty() && raw_value[0] == '-') {
            return AttributeValue{static_cast<int32_t>(std::stol(raw_value))};
        }

        // Unsigned integer (hex or decimal)
        if (raw_value.size() > 2 && raw_value[0] == '0' && raw_value[1] == 'x') {
            auto val = std::stoull(raw_value, nullptr, 16);
            if (val <= UINT32_MAX) {
                return AttributeValue{static_cast<uint32_t>(val)};
            }
            return AttributeValue{static_cast<uint64_t>(val)};
        }

        auto val = std::stoull(raw_value);
        if (val <= UINT32_MAX) {
            return AttributeValue{static_cast<uint32_t>(val)};
        }
        return AttributeValue{static_cast<uint64_t>(val)};

    } catch (...) {
        // Fall through to string
    }

    // Default: treat as string
    return AttributeValue{raw_value};
}

bool ChipToolOutputParser::hasChipError(const std::string& stderr_data) {
    return stderr_data.find("CHIP Error") != std::string::npos ||
           stderr_data.find("CHIP:SC: PASESession") != std::string::npos;
}

std::optional<int> ChipToolOutputParser::extractChipErrorCode(const std::string& stderr_data) {
    std::regex error_regex(R"(CHIP Error\s*(?:0x)?([0-9a-fA-F]+))");
    std::smatch match;
    if (std::regex_search(stderr_data, match, error_regex)) {
        try {
            return std::stoi(match[1].str(), nullptr, 16);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace mt::hw
