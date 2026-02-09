#pragma once

#include "core/Types.h"
#include "core/Result.h"

#include <string>
#include <vector>
#include <optional>
#include <utility>

#include <unistd.h>

namespace mt::hw {

struct ProcessOutput {
    int exit_code = -1;
    std::string stdout_data;
    std::string stderr_data;
    Duration elapsed{0};
    bool timed_out = false;
};

struct ProcessConfig {
    std::string binary_path;
    std::vector<std::string> args;
    Duration timeout = Duration(30000);
    std::string working_dir;
    std::vector<std::pair<std::string, std::string>> env_vars;
};

class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager();

    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;
    ProcessManager(ProcessManager&& other) noexcept;
    ProcessManager& operator=(ProcessManager&& other) noexcept;

    Result<ProcessOutput> run(const ProcessConfig& config);

    Result<void> start(const ProcessConfig& config);
    std::optional<ProcessOutput> poll();
    Result<ProcessOutput> wait();

    void kill();
    bool isRunning() const;
    pid_t pid() const { return pid_; }

private:
    pid_t pid_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;
    TimePoint start_time_{};
    Duration timeout_{30000};
    bool running_ = false;

    void cleanup();
    std::string readFd(int fd);
    Result<void> spawnProcess(const ProcessConfig& config);
};

} // namespace mt::hw
