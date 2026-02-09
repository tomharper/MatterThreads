#include <gtest/gtest.h>
#include "gateway/GatewayServer.h"
#include "gateway/VanRegistry.h"
#include "gateway/SessionPool.h"
#include "gateway/FleetSubscriptionManager.h"
#include "gateway/CommandRelay.h"
#include "gateway/OfflineBuffer.h"
#include "gateway/FabricManager.h"

#include <thread>
#include <chrono>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace mt;
using namespace mt::gateway;

// ── HTTP helper (same pattern as TestDashboardServer) ───────────────────────

static std::string httpRequest(uint16_t port, const std::string& method,
                               const std::string& path,
                               const std::string& body = "",
                               const std::string& extra_headers = "") {
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

    std::string req = method + " " + path + " HTTP/1.1\r\n"
                      "Host: localhost\r\n"
                      "Connection: close\r\n";
    if (!body.empty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += extra_headers;
    req += "\r\n" + body;

    ::send(fd, req.data(), req.size(), 0);

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

static int getStatusCode(const std::string& response) {
    // "HTTP/1.1 200 OK" → extract 200
    auto sp1 = response.find(' ');
    if (sp1 == std::string::npos) return 0;
    auto sp2 = response.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return 0;
    return std::stoi(response.substr(sp1 + 1, sp2 - sp1 - 1));
}

// ── Test fixture ────────────────────────────────────────────────────────────

class GatewayServerTest : public ::testing::Test {
protected:
    std::shared_ptr<hw::ChipToolDriver> driver;
    std::unique_ptr<VanRegistry> registry;
    std::unique_ptr<CASESessionPool> sessions;
    std::unique_ptr<FleetSubscriptionManager> subscriptions;
    std::unique_ptr<CommandRelay> commands;
    std::unique_ptr<OfflineBuffer> buffer;
    std::unique_ptr<FabricManager> fabrics;

    static constexpr uint16_t BASE_PORT = 19080;

    void SetUp() override {
        hw::ChipToolConfig config;
        config.binary_path = "/bin/echo";
        config.storage_dir = "/tmp/mt-gw-server-test";
        config.command_timeout = Duration(2000);
        driver = std::make_shared<hw::ChipToolDriver>(config);

        registry = std::make_unique<VanRegistry>();
        sessions = std::make_unique<CASESessionPool>(driver);
        subscriptions = std::make_unique<FleetSubscriptionManager>(driver, *sessions);
        commands = std::make_unique<CommandRelay>(driver, *sessions);
        buffer = std::make_unique<OfflineBuffer>();
        fabrics = std::make_unique<FabricManager>();
    }

    std::unique_ptr<GatewayServer> makeServer(uint16_t port) {
        GatewayConfig config;
        config.api_port = port;
        return std::make_unique<GatewayServer>(config, *registry, *sessions,
                                                *subscriptions, *commands,
                                                *buffer, *fabrics);
    }

    // Poll server while a client thread makes a request
    std::string serverRoundTrip(GatewayServer& server, const std::string& method,
                                const std::string& path, const std::string& body = "",
                                const std::string& extra_headers = "") {
        std::string response;
        std::thread client([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            response = httpRequest(server.port(), method, path, body, extra_headers);
        });

        for (int i = 0; i < 20; ++i) {
            server.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        client.join();
        return response;
    }
};

// ── Unit tests: route matching (no server needed) ───────────────────────────

TEST_F(GatewayServerTest, SplitPath) {
    auto segs = GatewayServer::splitPath("/api/vans/VAN-1/endpoints/2");
    ASSERT_EQ(segs.size(), 5u);
    EXPECT_EQ(segs[0], "api");
    EXPECT_EQ(segs[1], "vans");
    EXPECT_EQ(segs[2], "VAN-1");
    EXPECT_EQ(segs[3], "endpoints");
    EXPECT_EQ(segs[4], "2");
}

TEST_F(GatewayServerTest, RouteMatchExact) {
    Route route;
    route.method = "GET";
    route.pattern = "/api/health";
    route.segments = GatewayServer::splitPath(route.pattern);

    std::unordered_map<std::string, std::string> params;
    EXPECT_TRUE(GatewayServer::matchRoute(route, "GET", "/api/health", params));
    EXPECT_TRUE(params.empty());
    EXPECT_FALSE(GatewayServer::matchRoute(route, "POST", "/api/health", params));
    EXPECT_FALSE(GatewayServer::matchRoute(route, "GET", "/api/metrics", params));
}

TEST_F(GatewayServerTest, RouteMatchWithParams) {
    Route route;
    route.method = "GET";
    route.pattern = "/api/vans/{van_id}/endpoints/{ep}";
    route.segments = GatewayServer::splitPath(route.pattern);

    std::unordered_map<std::string, std::string> params;
    EXPECT_TRUE(GatewayServer::matchRoute(route, "GET",
                "/api/vans/VAN-42/endpoints/3", params));
    EXPECT_EQ(params["van_id"], "VAN-42");
    EXPECT_EQ(params["ep"], "3");

    // Wrong segment count
    EXPECT_FALSE(GatewayServer::matchRoute(route, "GET", "/api/vans/VAN-42", params));
}

TEST_F(GatewayServerTest, RouteMatchNoFalsePositive) {
    Route route;
    route.method = "GET";
    route.pattern = "/api/vans/{van_id}/subscriptions";
    route.segments = GatewayServer::splitPath(route.pattern);

    std::unordered_map<std::string, std::string> params;
    // Should not match a different route with same segment count
    EXPECT_FALSE(GatewayServer::matchRoute(route, "GET",
                 "/api/vans/VAN-1/endpoints", params));
}

// ── Integration tests: server round-trips ───────────────────────────────────

TEST_F(GatewayServerTest, HealthEndpoint) {
    auto server = makeServer(BASE_PORT);
    ASSERT_TRUE(server->start().ok());

    auto resp = serverRoundTrip(*server, "GET", "/api/health");
    EXPECT_EQ(getStatusCode(resp), 200);
    auto body = getBody(resp);
    EXPECT_NE(body.find("healthy"), std::string::npos);
    EXPECT_NE(body.find("running"), std::string::npos);

    server->stop();
}

TEST_F(GatewayServerTest, RegisterAndGetVan) {
    auto server = makeServer(BASE_PORT + 1);
    ASSERT_TRUE(server->start().ok());

    // Register a van
    std::string body = R"({"van_id":"VAN-TEST","device_id":12345,"tenant_id":1,"name":"Test Van"})";
    auto resp = serverRoundTrip(*server, "POST", "/api/vans", body);
    EXPECT_EQ(getStatusCode(resp), 201);
    EXPECT_NE(getBody(resp).find("VAN-TEST"), std::string::npos);

    // Get the van
    resp = serverRoundTrip(*server, "GET", "/api/vans/VAN-TEST");
    EXPECT_EQ(getStatusCode(resp), 200);
    EXPECT_NE(getBody(resp).find("VAN-TEST"), std::string::npos);
    EXPECT_NE(getBody(resp).find("Test Van"), std::string::npos);

    server->stop();
}

TEST_F(GatewayServerTest, ListVansEmpty) {
    auto server = makeServer(BASE_PORT + 2);
    ASSERT_TRUE(server->start().ok());

    auto resp = serverRoundTrip(*server, "GET", "/api/vans");
    EXPECT_EQ(getStatusCode(resp), 200);
    EXPECT_EQ(getBody(resp), "[]");

    server->stop();
}

TEST_F(GatewayServerTest, NotFoundReturns404) {
    auto server = makeServer(BASE_PORT + 3);
    ASSERT_TRUE(server->start().ok());

    auto resp = serverRoundTrip(*server, "GET", "/nonexistent");
    EXPECT_EQ(getStatusCode(resp), 404);

    server->stop();
}

TEST_F(GatewayServerTest, FleetStatusEndpoint) {
    auto server = makeServer(BASE_PORT + 4);
    ASSERT_TRUE(server->start().ok());

    // Register two vans first
    VanRegistration v1;
    v1.van_id = "VAN-A";
    v1.device_id = 100;
    registry->registerVan(v1);

    VanRegistration v2;
    v2.van_id = "VAN-B";
    v2.device_id = 200;
    registry->registerVan(v2);

    auto resp = serverRoundTrip(*server, "GET", "/api/fleet/status");
    EXPECT_EQ(getStatusCode(resp), 200);
    auto body = getBody(resp);
    EXPECT_NE(body.find("\"total_vans\":2"), std::string::npos);

    server->stop();
}

TEST_F(GatewayServerTest, CreateAndListTenants) {
    auto server = makeServer(BASE_PORT + 5);
    ASSERT_TRUE(server->start().ok());

    // Create a tenant
    std::string body = R"({"name":"Acme Delivery"})";
    auto resp = serverRoundTrip(*server, "POST", "/api/tenants", body);
    EXPECT_EQ(getStatusCode(resp), 201);
    EXPECT_NE(getBody(resp).find("Acme Delivery"), std::string::npos);

    // List tenants
    resp = serverRoundTrip(*server, "GET", "/api/tenants");
    EXPECT_EQ(getStatusCode(resp), 200);
    EXPECT_NE(getBody(resp).find("Acme Delivery"), std::string::npos);

    server->stop();
}
