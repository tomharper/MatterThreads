#include <gtest/gtest.h>
#include "hw/ChipToolOutputParser.h"

using namespace mt;
using namespace mt::hw;

// ---- Pairing output samples ----

static const char* PAIRING_SUCCESS_OUTPUT = R"(
[1706000000.000] CHIP:SC: Sent PBKDF param request
[1706000000.100] CHIP:SC: Received PBKDF param response
[1706000000.200] CHIP:SC: PASE session established
[1706000000.300] CHIP:CTL: Device commissioning completed with success
[1706000000.400] CHIP:TOO: Commissioning complete, node id 0x0000000000000001
)";

static const char* PAIRING_FAILURE_OUTPUT = R"(
[1706000000.000] CHIP:SC: Sent PBKDF param request
)";

static const char* PAIRING_FAILURE_STDERR = R"(
[1706000005.000] CHIP Error 0x0000002F: Timeout
[1706000005.100] CHIP:SC: PASESession establishment failed
)";

TEST(ChipToolOutputParser, ParsePairingSuccess) {
    auto result = ChipToolOutputParser::parsePairingOutput(
        PAIRING_SUCCESS_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->session_type, SessionType::PASE);
    EXPECT_EQ(result->node_id, 1u);
}

TEST(ChipToolOutputParser, ParsePairingFailure) {
    auto result = ChipToolOutputParser::parsePairingOutput(
        PAIRING_FAILURE_OUTPUT, PAIRING_FAILURE_STDERR, 1);
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->success);
    EXPECT_FALSE(result->error_message.empty());
}

// ---- Read output samples ----

static const char* READ_ONOFF_OUTPUT = R"(
[1706000000.000] CHIP:DMG: Received Read response
[1706000000.100] CHIP:TOO: Endpoint: 1 Cluster: 0x0000_0006 Attribute 0x0000_0000 DataVersion: 12345
[1706000000.200] CHIP:TOO:   OnOff: TRUE
)";

static const char* READ_TEMP_OUTPUT = R"(
[1706000000.000] CHIP:DMG: Received Read response
[1706000000.100] CHIP:TOO: Endpoint: 1 Cluster: 0x0000_0402 Attribute 0x0000_0000 DataVersion: 456
[1706000000.200] CHIP:TOO:   MeasuredValue: 2150
)";

TEST(ChipToolOutputParser, ParseReadOnOff) {
    auto result = ChipToolOutputParser::parseReadOutput(READ_ONOFF_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_TRUE(std::holds_alternative<bool>(result->value));
    EXPECT_EQ(std::get<bool>(result->value), true);
}

TEST(ChipToolOutputParser, ParseReadTemperature) {
    auto result = ChipToolOutputParser::parseReadOutput(READ_TEMP_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_TRUE(std::holds_alternative<uint32_t>(result->value));
    EXPECT_EQ(std::get<uint32_t>(result->value), 2150u);
}

// ---- Write output samples ----

static const char* WRITE_SUCCESS_OUTPUT = R"(
[1706000000.000] CHIP:DMG: WriteResponseMessage =
[1706000000.100] CHIP:DMG: status = 0x0 (SUCCESS)
)";

TEST(ChipToolOutputParser, ParseWriteSuccess) {
    auto result = ChipToolOutputParser::parseWriteOutput(WRITE_SUCCESS_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->status_code, 0);
}

// ---- Invoke output samples ----

static const char* INVOKE_SUCCESS_OUTPUT = R"(
[1706000000.000] CHIP:DMG: InvokeResponseMessage =
[1706000000.100] CHIP:DMG: status = 0x0 (SUCCESS)
)";

TEST(ChipToolOutputParser, ParseInvokeSuccess) {
    auto result = ChipToolOutputParser::parseInvokeOutput(INVOKE_SUCCESS_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->status_code, 0);
}

// ---- Subscribe output samples ----

static const char* SUBSCRIBE_OUTPUT = R"(
[1706000000.000] CHIP:DMG: Subscription established with SubscriptionId = 42
[1706000000.100] CHIP:DMG: SubscribeResponse received
)";

TEST(ChipToolOutputParser, ParseSubscribeInitialReport) {
    auto result = ChipToolOutputParser::parseSubscribeOutput(SUBSCRIBE_OUTPUT, "", 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->subscription_id, 42u);
}

// ---- Error extraction ----

static const char* CHIP_ERROR_STDERR = R"(
[1706000000.000] CHIP:SC: PASESession establishment failed
[1706000000.100] CHIP Error 0x0000002F: Timeout
)";

TEST(ChipToolOutputParser, ParseChipError) {
    EXPECT_TRUE(ChipToolOutputParser::hasChipError(CHIP_ERROR_STDERR));
    auto code = ChipToolOutputParser::extractChipErrorCode(CHIP_ERROR_STDERR);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(*code, 0x2F);
}
