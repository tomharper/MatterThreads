#include <gtest/gtest.h>
#include "gateway/CommandRelay.h"

using namespace mt;
using namespace mt::gateway;

class CommandRelayTest : public ::testing::Test {
protected:
    std::shared_ptr<hw::ChipToolDriver> driver;
    std::unique_ptr<CASESessionPool> pool;

    void SetUp() override {
        hw::ChipToolConfig config;
        config.binary_path = "/bin/echo";
        config.storage_dir = "/tmp/mt-cmd-relay-test";
        config.command_timeout = Duration(2000);
        driver = std::make_shared<hw::ChipToolDriver>(config);
        pool = std::make_unique<CASESessionPool>(driver);
    }
};

TEST_F(CommandRelayTest, InvokeOnDisconnectedVan) {
    CommandRelay relay(driver, *pool);
    auto result = relay.invoke("VAN-OFFLINE", 1, 1, 0x0006, 0x0002);
    ASSERT_TRUE(result.ok()); // Returns InvokeResult with success=false
    EXPECT_FALSE(result->success);
    EXPECT_NE(result->error_message.find("not connected"), std::string::npos);
}

TEST_F(CommandRelayTest, InvokeOnUnknownVan) {
    CommandRelay relay(driver, *pool);
    auto result = relay.invoke("VAN-UNKNOWN", 99, 1, 0x0006, 0x0001);
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->success);
}

TEST_F(CommandRelayTest, TimedInvokeOnDisconnectedVan) {
    CommandRelay relay(driver, *pool);
    auto result = relay.timedInvoke("VAN-X", 1, 1, 0x0101, 0x0000,
                                     Duration(30000));
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->success);
}

TEST_F(CommandRelayTest, ReadAttributeOnDisconnectedVan) {
    CommandRelay relay(driver, *pool);
    auto result = relay.readAttribute("VAN-X", 1, 0, 0x0028, 0x0001);
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->success);
    EXPECT_NE(result->error_message.find("not connected"), std::string::npos);
}

TEST_F(CommandRelayTest, WriteAttributeOnDisconnectedVan) {
    CommandRelay relay(driver, *pool);
    auto result = relay.writeAttribute("VAN-X", 1, 0, 0x0028, 0x0005, "TestLabel");
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->success);
}

TEST_F(CommandRelayTest, CommandResultPropagation) {
    CommandRelay relay(driver, *pool);
    // Even without connection, the relay should produce a valid result struct
    auto result = relay.invoke("VAN-NOPE", 1, 1, 0x0006, 0x0002);
    ASSERT_TRUE(result.ok());
    // The error_message should contain meaningful info
    EXPECT_FALSE(result->error_message.empty());
}
