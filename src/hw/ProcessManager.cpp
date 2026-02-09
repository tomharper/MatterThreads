#include "hw/ProcessManager.h"
#include "core/Log.h"

#include <cerrno>
#include <cstring>
#include <chrono>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>

namespace mt::hw {

ProcessManager::~ProcessManager() {
    if (running_) {
        kill();
    }
    cleanup();
}

ProcessManager::ProcessManager(ProcessManager&& other) noexcept
    : pid_(other.pid_), stdout_fd_(other.stdout_fd_), stderr_fd_(other.stderr_fd_),
      start_time_(other.start_time_), timeout_(other.timeout_), running_(other.running_) {
    other.pid_ = -1;
    other.stdout_fd_ = -1;
    other.stderr_fd_ = -1;
    other.running_ = false;
}

ProcessManager& ProcessManager::operator=(ProcessManager&& other) noexcept {
    if (this != &other) {
        if (running_) kill();
        cleanup();
        pid_ = other.pid_;
        stdout_fd_ = other.stdout_fd_;
        stderr_fd_ = other.stderr_fd_;
        start_time_ = other.start_time_;
        timeout_ = other.timeout_;
        running_ = other.running_;
        other.pid_ = -1;
        other.stdout_fd_ = -1;
        other.stderr_fd_ = -1;
        other.running_ = false;
    }
    return *this;
}

Result<void> ProcessManager::spawnProcess(const ProcessConfig& config) {
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0) {
        return Error("Failed to create stdout pipe: " + std::string(strerror(errno)));
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return Error("Failed to create stderr pipe: " + std::string(strerror(errno)));
    }

    pid_t child = fork();
    if (child < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return Error("Fork failed: " + std::string(strerror(errno)));
    }

    if (child == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (!config.working_dir.empty()) {
            if (chdir(config.working_dir.c_str()) != 0) {
                _exit(127);
            }
        }

        for (const auto& [key, val] : config.env_vars) {
            setenv(key.c_str(), val.c_str(), 1);
        }

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(config.binary_path.c_str());
        for (const auto& arg : config.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(config.binary_path.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127); // exec failed
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Set pipes to non-blocking
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    pid_ = child;
    stdout_fd_ = stdout_pipe[0];
    stderr_fd_ = stderr_pipe[0];
    timeout_ = config.timeout;
    start_time_ = SteadyClock::now();
    running_ = true;

    return Result<void>::success();
}

Result<ProcessOutput> ProcessManager::run(const ProcessConfig& config) {
    auto spawn_result = start(config);
    if (!spawn_result.ok()) {
        return Error(spawn_result.error().code, spawn_result.error().message);
    }
    return wait();
}

Result<void> ProcessManager::start(const ProcessConfig& config) {
    if (running_) {
        return Error("Process already running");
    }
    return spawnProcess(config);
}

std::optional<ProcessOutput> ProcessManager::poll() {
    if (!running_ || pid_ < 0) {
        return std::nullopt;
    }

    int status = 0;
    pid_t result = waitpid(pid_, &status, WNOHANG);

    if (result == 0) {
        // Still running — check timeout
        auto elapsed = SteadyClock::now() - start_time_;
        if (elapsed > timeout_) {
            ::kill(pid_, SIGTERM);
            // Give it a moment, then SIGKILL
            usleep(100000); // 100ms
            waitpid(pid_, &status, WNOHANG);
            ::kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);

            ProcessOutput output;
            output.timed_out = true;
            output.exit_code = -1;
            output.stdout_data = readFd(stdout_fd_);
            output.stderr_data = readFd(stderr_fd_);
            output.elapsed = std::chrono::duration_cast<Duration>(elapsed);
            running_ = false;
            cleanup();
            return output;
        }
        return std::nullopt;
    }

    // Process finished
    ProcessOutput output;
    output.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    output.stdout_data = readFd(stdout_fd_);
    output.stderr_data = readFd(stderr_fd_);
    output.elapsed = std::chrono::duration_cast<Duration>(SteadyClock::now() - start_time_);
    output.timed_out = false;
    running_ = false;
    pid_ = -1;
    cleanup();
    return output;
}

Result<ProcessOutput> ProcessManager::wait() {
    if (!running_ || pid_ < 0) {
        return Error("No process running");
    }

    // Poll loop with timeout checking
    while (true) {
        auto result = poll();
        if (result.has_value()) {
            return result.value();
        }

        // Check if we timed out inside poll()
        if (!running_) {
            return Error("Process timed out");
        }

        usleep(10000); // 10ms between polls
    }
}

void ProcessManager::kill() {
    if (pid_ > 0 && running_) {
        ::kill(pid_, SIGTERM);
        usleep(100000); // 100ms grace
        int status = 0;
        pid_t result = waitpid(pid_, &status, WNOHANG);
        if (result == 0) {
            ::kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
        running_ = false;
        pid_ = -1;
    }
}

bool ProcessManager::isRunning() const {
    return running_;
}

void ProcessManager::cleanup() {
    if (stdout_fd_ >= 0) { close(stdout_fd_); stdout_fd_ = -1; }
    if (stderr_fd_ >= 0) { close(stderr_fd_); stderr_fd_ = -1; }
}

std::string ProcessManager::readFd(int fd) {
    if (fd < 0) return "";

    std::string data;
    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            data.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            break; // EOF
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Non-blocking, no more data
            }
            break; // Error
        }
    }
    return data;
}

} // namespace mt::hw
