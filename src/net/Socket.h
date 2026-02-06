#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include <string>
#include <string_view>
#include <functional>

namespace mt {

// RAII TCP socket wrapper
class Socket {
    int fd_ = -1;
public:
    Socket() = default;
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const { return fd_; }
    bool valid() const { return fd_ >= 0; }
    void close();

    // TCP client: connect to host:port
    static Result<Socket> connect(const std::string& host, uint16_t port);

    // TCP server: bind and listen on port
    static Result<Socket> listen(uint16_t port, int backlog = 16);

    // Accept a connection (blocking)
    Result<Socket> accept();

    // Send exactly len bytes (blocking)
    Result<void> sendAll(const uint8_t* data, size_t len);

    // Receive exactly len bytes (blocking)
    Result<void> recvAll(uint8_t* data, size_t len);

    // Set socket to non-blocking mode
    Result<void> setNonBlocking(bool nb);

    // Unix domain socket variants
    static Result<Socket> connectUnix(const std::string& path);
    static Result<Socket> listenUnix(const std::string& path, int backlog = 16);
};

} // namespace mt
