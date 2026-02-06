#include "net/Socket.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

namespace mt {

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Result<Socket> Socket::connect(const std::string& host, uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return Error("socket() failed: " + std::string(strerror(errno)));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        ::close(fd);
        return Error("Invalid address: " + host);
    }

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return Error("connect() failed: " + std::string(strerror(errno)));
    }

    return Socket(fd);
}

Result<Socket> Socket::listen(uint16_t port, int backlog) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return Error("socket() failed: " + std::string(strerror(errno)));

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return Error("bind() failed: " + std::string(strerror(errno)));
    }

    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        return Error("listen() failed: " + std::string(strerror(errno)));
    }

    return Socket(fd);
}

Result<Socket> Socket::accept() {
    struct sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    int client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
    if (client_fd < 0) return Error("accept() failed: " + std::string(strerror(errno)));
    return Socket(client_fd);
}

Result<void> Socket::sendAll(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd_, data + sent, len - sent, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return Error("send() failed: " + std::string(strerror(errno)));
        }
        sent += static_cast<size_t>(n);
    }
    return Result<void>::success();
}

Result<void> Socket::recvAll(uint8_t* data, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = ::recv(fd_, data + received, len - received, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            if (n == 0) return Error("Connection closed");
            return Error("recv() failed: " + std::string(strerror(errno)));
        }
        received += static_cast<size_t>(n);
    }
    return Result<void>::success();
}

Result<void> Socket::setNonBlocking(bool nb) {
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) return Error("fcntl(F_GETFL) failed");
    if (nb)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(fd_, F_SETFL, flags) < 0) return Error("fcntl(F_SETFL) failed");
    return Result<void>::success();
}

Result<Socket> Socket::connectUnix(const std::string& path) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return Error("socket(AF_UNIX) failed: " + std::string(strerror(errno)));

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return Error("Unix socket path too long");
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return Error("connect(unix) failed: " + std::string(strerror(errno)));
    }

    return Socket(fd);
}

Result<Socket> Socket::listenUnix(const std::string& path, int backlog) {
    ::unlink(path.c_str());

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return Error("socket(AF_UNIX) failed: " + std::string(strerror(errno)));

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        ::close(fd);
        return Error("Unix socket path too long");
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return Error("bind(unix) failed: " + std::string(strerror(errno)));
    }

    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        return Error("listen(unix) failed: " + std::string(strerror(errno)));
    }

    return Socket(fd);
}

} // namespace mt
