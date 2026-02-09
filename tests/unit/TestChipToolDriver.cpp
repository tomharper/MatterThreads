#include <gtest/gtest.h>
#include "hw/ChipToolDriver.h"

using namespace mt;
using namespace mt::hw;

using mt::Duration;

class ChipToolDriverTest : public ::testing::Test {
protected:
    ChipToolConfig config;
    void SetUp() override {
        config.binary_path = "chip-tool";
        config.commissioner_name = "test-ctrl";
        config.storage_dir = "/tmp/mt-test-storage";
    }
};

TEST_F(ChipToolDriverTest, BuildPairingCommand) {
    ChipToolDriver driver(config);
    auto args = driver.buildPairingArgs(1, 20202021, "", 5540);

    ASSERT_GE(args.size(), 3u);
    EXPECT_EQ(args[0], "pairing");
    EXPECT_EQ(args[1], "onnetwork");
    EXPECT_EQ(args[2], "1");
    EXPECT_EQ(args[3], "20202021");

    // Should include base args
    bool has_commissioner = false;
    bool has_storage = false;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--commissioner-name" && i + 1 < args.size()) {
            EXPECT_EQ(args[i + 1], "test-ctrl");
            has_commissioner = true;
        }
        if (args[i] == "--storage-directory" && i + 1 < args.size()) {
            EXPECT_EQ(args[i + 1], "/tmp/mt-test-storage");
            has_storage = true;
        }
    }
    EXPECT_TRUE(has_commissioner);
    EXPECT_TRUE(has_storage);
}

TEST_F(ChipToolDriverTest, BuildReadCommandKnownCluster) {
    ChipToolDriver driver(config);
    // OnOff cluster (0x0006), on-off attribute (0x0000)
    auto args = driver.buildReadArgs(1, 1, 0x0006, 0x0000);

    ASSERT_GE(args.size(), 4u);
    EXPECT_EQ(args[0], "onoff");
    EXPECT_EQ(args[1], "read");
    EXPECT_EQ(args[2], "on-off");
    EXPECT_EQ(args[3], "1"); // node_id
    EXPECT_EQ(args[4], "1"); // endpoint
}

TEST_F(ChipToolDriverTest, BuildReadCommandUnknownCluster) {
    ChipToolDriver driver(config);
    // Unknown cluster 0x9999
    auto args = driver.buildReadArgs(1, 1, 0x9999, 0x0001);

    ASSERT_GE(args.size(), 4u);
    EXPECT_EQ(args[0], "any");
    EXPECT_EQ(args[1], "read-by-id");
    EXPECT_EQ(args[2], "0x9999");
    EXPECT_EQ(args[3], "0x1");
}

TEST_F(ChipToolDriverTest, BuildWriteCommand) {
    ChipToolDriver driver(config);
    auto args = driver.buildWriteArgs(1, 1, 0x0006, 0x0000, "1");

    ASSERT_GE(args.size(), 5u);
    EXPECT_EQ(args[0], "onoff");
    EXPECT_EQ(args[1], "write");
    EXPECT_EQ(args[2], "on-off");
    EXPECT_EQ(args[3], "1"); // value
    EXPECT_EQ(args[4], "1"); // node_id
    EXPECT_EQ(args[5], "1"); // endpoint
}

TEST_F(ChipToolDriverTest, BuildSubscribeCommand) {
    ChipToolDriver driver(config);
    auto args = driver.buildSubscribeArgs(1, 1, 0x0006, 0x0000,
                                           Duration(5000), Duration(60000));

    ASSERT_GE(args.size(), 6u);
    EXPECT_EQ(args[0], "onoff");
    EXPECT_EQ(args[1], "subscribe");
    EXPECT_EQ(args[2], "on-off");
    EXPECT_EQ(args[3], "5");  // min interval in seconds
    EXPECT_EQ(args[4], "60"); // max interval in seconds
    EXPECT_EQ(args[5], "1");  // node_id
    EXPECT_EQ(args[6], "1");  // endpoint
}

TEST_F(ChipToolDriverTest, BuildInvokeCommandKnown) {
    ChipToolDriver driver(config);
    // OnOff cluster, On command (0x0001)
    auto args = driver.buildInvokeArgs(1, 1, 0x0006, 0x0001, "");

    ASSERT_GE(args.size(), 3u);
    EXPECT_EQ(args[0], "onoff");
    EXPECT_EQ(args[1], "on");
    EXPECT_EQ(args[2], "1"); // node_id
    EXPECT_EQ(args[3], "1"); // endpoint
}
