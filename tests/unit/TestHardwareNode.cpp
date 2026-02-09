#include <gtest/gtest.h>
#include "hw/HardwareNode.h"

using namespace mt;
using namespace mt::hw;

using mt::Duration;

class HardwareNodeTest : public ::testing::Test {
protected:
    std::shared_ptr<ChipToolDriver> driver;
    std::shared_ptr<OTBRClient> otbr;

    void SetUp() override {
        ChipToolConfig config;
        config.binary_path = "/bin/echo"; // Won't actually do chip-tool ops
        config.storage_dir = "/tmp/mt-hw-test";
        config.command_timeout = Duration(2000);
        driver = std::make_shared<ChipToolDriver>(config);
        otbr = std::make_shared<OTBRClient>("http://127.0.0.1:19999");
    }
};

TEST_F(HardwareNodeTest, IdentityFields) {
    HardwareNode node(42, "test-sensor", driver, otbr);
    EXPECT_EQ(node.deviceId(), 42u);
    EXPECT_EQ(node.name(), "test-sensor");
}

TEST_F(HardwareNodeTest, InitiallyNotCommissioned) {
    HardwareNode node(1, "light", driver);
    EXPECT_FALSE(node.isCommissioned());
}

TEST_F(HardwareNodeTest, UncommissionedReadFails) {
    HardwareNode node(1, "light", driver);
    auto result = node.readAttribute(1, 0x0006, 0x0000);
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("not commissioned"), std::string::npos);
}

TEST_F(HardwareNodeTest, UncommissionedWriteFails) {
    HardwareNode node(1, "light", driver);
    auto result = node.writeAttribute(1, 0x0006, 0x0000, AttributeValue{true});
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("not commissioned"), std::string::npos);
}

TEST_F(HardwareNodeTest, UncommissionedInvokeFails) {
    HardwareNode node(1, "light", driver);
    auto result = node.invokeCommand(1, 0x0006, 0x0001, {});
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("not commissioned"), std::string::npos);
}

TEST_F(HardwareNodeTest, CASEWithoutCommissionFails) {
    HardwareNode node(1, "light", driver);
    auto result = node.openCASESession();
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("not commissioned"), std::string::npos);
}
