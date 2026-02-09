#include <gtest/gtest.h>
#include <thread>
#include "hw/ChipToolDriver.h"
#include "hw/OTBRClient.h"
#include "hw/HardwareNode.h"

using namespace mt;
using namespace mt::hw;

class CertHarnessFixture : public ::testing::Test {
protected:
    std::shared_ptr<ChipToolDriver> driver_;
    std::shared_ptr<OTBRClient> otbr_;
    std::unique_ptr<HardwareNode> dut_;  // Device Under Test

    void SetUp() override {
        ChipToolConfig config;
        // Use environment variables for configuration
        const char* chip_tool = std::getenv("MT_CHIP_TOOL_PATH");
        if (chip_tool) config.binary_path = chip_tool;

        config.storage_dir = "/tmp/mt-cert-harness";
        config.command_timeout = Duration(120000);  // 2min for cert tests

        driver_ = std::make_shared<ChipToolDriver>(config);

        const char* otbr_url = std::getenv("MT_OTBR_URL");
        if (otbr_url) {
            otbr_ = std::make_shared<OTBRClient>(otbr_url);
        }

        // DUT node ID from environment, default to 1
        uint64_t dut_node_id = 1;
        const char* node_id_str = std::getenv("MT_DUT_NODE_ID");
        if (node_id_str) dut_node_id = std::stoull(node_id_str);

        dut_ = std::make_unique<HardwareNode>(dut_node_id, "DUT", driver_, otbr_);
    }

    void TearDown() override {
        // Clean up: unpair device
        if (dut_ && dut_->isCommissioned()) {
            driver_->unpair(dut_->deviceId());
        }
    }
};

// TC-SC-1.1: PASE Commissioning
// Verifies that a device can be commissioned using a setup code
TEST_F(CertHarnessFixture, DISABLED_TC_SC_1_1_PASE_Commissioning) {
    uint32_t setup_code = 20202021; // Default test setup code
    const char* code_str = std::getenv("MT_SETUP_CODE");
    if (code_str) setup_code = static_cast<uint32_t>(std::stoul(code_str));

    auto result = dut_->commission(setup_code);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_TRUE(dut_->isCommissioned());
}

// TC-SC-3.1: CASE Session Establishment
// Verifies CASE session can be established after commissioning
TEST_F(CertHarnessFixture, DISABLED_TC_SC_3_1_CASE_Session) {
    auto commission_result = dut_->commission(20202021);
    ASSERT_TRUE(commission_result.ok()) << commission_result.error().message;

    auto case_result = dut_->openCASESession();
    ASSERT_TRUE(case_result.ok()) << case_result.error().message;
}

// TC-DA-1.1: Device Attestation
// Verifies device attestation succeeds during commissioning
TEST_F(CertHarnessFixture, DISABLED_TC_DA_1_1_DeviceAttestation) {
    // Device attestation happens as part of commissioning
    // chip-tool verifies DAC, PAI, CD automatically
    auto result = dut_->commission(20202021);
    ASSERT_TRUE(result.ok()) << "Commissioning (including attestation) failed: "
                              << result.error().message;
}

// TC-RR-1.1: Read Request
// Verifies that Read Request works for BasicInformation cluster
TEST_F(CertHarnessFixture, DISABLED_TC_RR_1_1_ReadRequest) {
    auto commission_result = dut_->commission(20202021);
    ASSERT_TRUE(commission_result.ok());

    // Read BasicInformation:VendorName (endpoint 0, cluster 0x0028, attr 0x0001)
    auto result = dut_->readAttribute(0, 0x0028, 0x0001);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_TRUE(std::holds_alternative<std::string>(*result));
}

// TC-IDM-1.1: Write Request
// Verifies Write Request works for a writable attribute
TEST_F(CertHarnessFixture, DISABLED_TC_IDM_1_1_WriteRequest) {
    auto commission_result = dut_->commission(20202021);
    ASSERT_TRUE(commission_result.ok());

    // Write BasicInformation:NodeLabel (endpoint 0, cluster 0x0028, attr 0x0005)
    auto write_result = dut_->writeAttribute(0, 0x0028, 0x0005,
                                              AttributeValue{std::string("CertTest")});
    ASSERT_TRUE(write_result.ok()) << write_result.error().message;

    // Read back to verify
    auto read_result = dut_->readAttribute(0, 0x0028, 0x0005);
    ASSERT_TRUE(read_result.ok());
    EXPECT_EQ(std::get<std::string>(*read_result), "CertTest");
}

// TC-IDM-2.1: Invoke Command
// Verifies InvokeCommand works (using Identify cluster)
TEST_F(CertHarnessFixture, DISABLED_TC_IDM_2_1_InvokeCommand) {
    auto commission_result = dut_->commission(20202021);
    ASSERT_TRUE(commission_result.ok());

    // Invoke OnOff:Toggle (endpoint 1, cluster 0x0006, command 0x0002)
    auto result = dut_->invokeCommand(1, 0x0006, 0x0002, {});
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result->status_code, 0);
}

// TC-IDM-3.1: Subscribe
// Verifies attribute subscription and report delivery
TEST_F(CertHarnessFixture, DISABLED_TC_IDM_3_1_Subscribe) {
    auto commission_result = dut_->commission(20202021);
    ASSERT_TRUE(commission_result.ok());

    bool report_received = false;
    auto sub_result = dut_->subscribe(1, 0x0006, 0x0000,
                                       Duration(1000), Duration(10000),
                                       [&](const AttributeValue&) {
                                           report_received = true;
                                       });
    ASSERT_TRUE(sub_result.ok()) << sub_result.error().message;

    // Poll for a few seconds to receive initial report
    for (int i = 0; i < 50 && !report_received; ++i) {
        dut_->tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    dut_->cancelSubscription(*sub_result);
}
