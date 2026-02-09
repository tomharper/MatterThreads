#include <gtest/gtest.h>
#include "hw/ProcessManager.h"

using namespace mt;
using namespace mt::hw;

TEST(ProcessManager, RunEchoCommand) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/bin/echo";
    config.args = {"hello", "world"};
    config.timeout = Duration(5000);

    auto result = pm.run(config);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->exit_code, 0);
    EXPECT_EQ(result->stdout_data, "hello world\n");
    EXPECT_TRUE(result->stderr_data.empty());
    EXPECT_FALSE(result->timed_out);
}

TEST(ProcessManager, RunFailingCommand) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/bin/sh";
    config.args = {"-c", "exit 42"};
    config.timeout = Duration(5000);

    auto result = pm.run(config);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->exit_code, 42);
    EXPECT_FALSE(result->timed_out);
}

TEST(ProcessManager, TimeoutKillsProcess) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/bin/sleep";
    config.args = {"100"};
    config.timeout = Duration(500);  // 500ms timeout

    auto result = pm.run(config);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result->timed_out);
    EXPECT_LT(result->elapsed.count(), 5000);  // Should not take 100 seconds
}

TEST(ProcessManager, AsyncStartAndPoll) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/bin/echo";
    config.args = {"async-test"};
    config.timeout = Duration(5000);

    auto start_result = pm.start(config);
    ASSERT_TRUE(start_result.ok());
    EXPECT_TRUE(pm.isRunning());

    auto wait_result = pm.wait();
    ASSERT_TRUE(wait_result.ok());
    EXPECT_EQ(wait_result->exit_code, 0);
    EXPECT_EQ(wait_result->stdout_data, "async-test\n");
    EXPECT_FALSE(pm.isRunning());
}

TEST(ProcessManager, KillRunningProcess) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/bin/sleep";
    config.args = {"100"};
    config.timeout = Duration(60000);

    auto start_result = pm.start(config);
    ASSERT_TRUE(start_result.ok());
    EXPECT_TRUE(pm.isRunning());

    pm.kill();
    EXPECT_FALSE(pm.isRunning());
}

TEST(ProcessManager, NonExistentBinaryFails) {
    ProcessManager pm;
    ProcessConfig config;
    config.binary_path = "/nonexistent/binary/path";
    config.timeout = Duration(5000);

    auto result = pm.run(config);
    // exec fails, child exits with 127
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->exit_code, 127);
}
