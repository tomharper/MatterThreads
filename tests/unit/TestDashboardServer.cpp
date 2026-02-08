#include <gtest/gtest.h>
#include "metrics/DashboardServer.h"
#include "metrics/Collector.h"
#include "net/Socket.h"

#include <thread>
#include <chrono>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace mt;

// Helper: send an HTTP GET request and return the full response
static std::string httpGet(uint16_t port, const std::string& path) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return "";
    }

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    ::send(fd, req.data(), req.size(), 0);

    // Read response
    std::string response;
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);
    return response;
}

static std::string getBody(const std::string& response) {
    auto pos = response.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return response.substr(pos + 4);
}

class DashboardServerTest : public ::testing::Test {
protected:
    Collector collector;
    // Use a high port to avoid conflicts
    static constexpr uint16_t TEST_PORT = 18080;

    void SetUp() override {
        collector.reset();
    }
};

TEST_F(DashboardServerTest, StartsAndListens) {
    DashboardServer server(collector, TEST_PORT);
    auto result = server.start();
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_TRUE(server.running());
    EXPECT_EQ(server.port(), TEST_PORT);
    server.stop();
    EXPECT_FALSE(server.running());
}

TEST_F(DashboardServerTest, ServesHtmlOnRoot) {
    DashboardServer server(collector, TEST_PORT + 1);
    ASSERT_TRUE(server.start().ok());

    // Give server a moment, then make request
    // Need to poll in between to handle the connection
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 1, "/");
        EXPECT_TRUE(resp.find("200 OK") != std::string::npos) << "Response: " << resp.substr(0, 100);
        EXPECT_TRUE(resp.find("text/html") != std::string::npos);
        auto body = getBody(resp);
        EXPECT_TRUE(body.find("MatterThreads Dashboard") != std::string::npos);
    });

    // Poll to handle the incoming connection
    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}

TEST_F(DashboardServerTest, ServesJsonMetrics) {
    DashboardServer server(collector, TEST_PORT + 2);
    ASSERT_TRUE(server.start().ok());

    // Add some metrics data
    collector.increment("test.packets_sent", 42);
    collector.record("test.latency_ms", 5.5);
    collector.record("test.latency_ms", 10.2);

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 2, "/api/metrics");
        EXPECT_TRUE(resp.find("200 OK") != std::string::npos);
        EXPECT_TRUE(resp.find("application/json") != std::string::npos);
        auto body = getBody(resp);
        EXPECT_TRUE(body.find("counters") != std::string::npos);
        EXPECT_TRUE(body.find("histograms") != std::string::npos);
        EXPECT_TRUE(body.find("test.packets_sent") != std::string::npos);
    });

    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}

TEST_F(DashboardServerTest, ServesJsonTimeline) {
    DashboardServer server(collector, TEST_PORT + 3);
    ASSERT_TRUE(server.start().ok());

    collector.event(TimePoint{Duration{1000}}, 0, "test", "started", "unit test");

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 3, "/api/timeline");
        EXPECT_TRUE(resp.find("200 OK") != std::string::npos);
        EXPECT_TRUE(resp.find("application/json") != std::string::npos);
        auto body = getBody(resp);
        EXPECT_TRUE(body.find("started") != std::string::npos);
    });

    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}

TEST_F(DashboardServerTest, Returns404ForUnknownPath) {
    DashboardServer server(collector, TEST_PORT + 4);
    ASSERT_TRUE(server.start().ok());

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 4, "/nonexistent");
        EXPECT_TRUE(resp.find("404") != std::string::npos);
    });

    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}

TEST_F(DashboardServerTest, ServesStatusWithProvider) {
    DashboardServer server(collector, TEST_PORT + 5);

    server.setNodeStatusProvider([]() -> std::vector<NodeStatus> {
        return {
            {0, "leader", "running", 1234},
            {1, "router", "running", 1235},
            {2, "sed", "stopped", 0},
            {3, "phone", "running", 1237}
        };
    });

    ASSERT_TRUE(server.start().ok());

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 5, "/api/status");
        EXPECT_TRUE(resp.find("200 OK") != std::string::npos);
        auto body = getBody(resp);
        EXPECT_TRUE(body.find("leader") != std::string::npos);
        EXPECT_TRUE(body.find("phone") != std::string::npos);
        EXPECT_TRUE(body.find("1234") != std::string::npos);
    });

    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}

TEST_F(DashboardServerTest, ServesTopologyWithProvider) {
    DashboardServer server(collector, TEST_PORT + 6);

    server.setTopologyProvider([]() -> TopologyMatrix {
        TopologyMatrix m{};
        m[0][1] = {0.01f, 5.0f, true, 200, -60};
        m[1][0] = {0.01f, 5.0f, true, 200, -60};
        m[0][3] = {0.02f, 120.0f, true, 150, -80};  // phone backhaul
        m[3][0] = {0.02f, 120.0f, true, 150, -80};
        return m;
    });

    ASSERT_TRUE(server.start().ok());

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto resp = httpGet(TEST_PORT + 6, "/api/topology");
        EXPECT_TRUE(resp.find("200 OK") != std::string::npos);
        auto body = getBody(resp);
        EXPECT_TRUE(body.find("links") != std::string::npos);
        EXPECT_TRUE(body.find("loss") != std::string::npos);
        EXPECT_TRUE(body.find("latency") != std::string::npos);
    });

    for (int i = 0; i < 20; ++i) {
        server.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    client_thread.join();
    server.stop();
}
