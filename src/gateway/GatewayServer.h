#pragma once

#include "gateway/GatewayTypes.h"
#include "gateway/VanRegistry.h"
#include "gateway/SessionPool.h"
#include "gateway/FleetSubscriptionManager.h"
#include "gateway/CommandRelay.h"
#include "gateway/OfflineBuffer.h"
#include "gateway/FabricManager.h"
#include "core/Result.h"
#include "net/Socket.h"
#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace mt::gateway {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;
};

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "application/json";
    std::string body;

    static HttpResponse json(const nlohmann::json& j, int code = 200) {
        return {code, "application/json", j.dump()};
    }
    static HttpResponse error(int code, const std::string& message) {
        nlohmann::json j = {{"error", message}};
        return {code, "application/json", j.dump()};
    }
    static HttpResponse ok() {
        nlohmann::json j = {{"status", "ok"}};
        return {200, "application/json", j.dump()};
    }
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

struct Route {
    std::string method;
    std::string pattern;  // e.g. "/api/vans/{van_id}/endpoints/{ep}"
    RouteHandler handler;

    // Pattern segments for matching
    std::vector<std::string> segments;
    std::vector<std::pair<size_t, std::string>> param_indices;  // (index, name)
};

class GatewayServer {
public:
    GatewayServer(const GatewayConfig& config,
                  VanRegistry& registry,
                  CASESessionPool& sessions,
                  FleetSubscriptionManager& subscriptions,
                  CommandRelay& commands,
                  OfflineBuffer& buffer,
                  FabricManager& fabrics);

    Result<void> start();
    void poll();
    void stop();

    uint16_t port() const { return config_.api_port; }
    bool running() const { return running_; }

    // For testing: register custom routes
    void addRoute(const std::string& method, const std::string& pattern, RouteHandler handler);

    // Route matching (exposed for testing)
    static bool matchRoute(const Route& route, const std::string& method,
                           const std::string& path,
                           std::unordered_map<std::string, std::string>& params);

    static std::vector<std::string> splitPath(const std::string& path);

private:
    GatewayConfig config_;
    VanRegistry& registry_;
    CASESessionPool& sessions_;
    FleetSubscriptionManager& subscriptions_;
    CommandRelay& commands_;
    OfflineBuffer& buffer_;
    FabricManager& fabrics_;

    Socket listen_socket_;
    bool running_ = false;
    std::vector<Route> routes_;

    void registerRoutes();

    void handleClient(Socket client);
    HttpRequest parseRequest(const std::string& raw);
    HttpResponse routeRequest(const HttpRequest& request);

    void sendResponse(Socket& client, const HttpResponse& response);

    // Route handlers — Vans
    HttpResponse handleListVans(const HttpRequest& req);
    HttpResponse handleRegisterVan(const HttpRequest& req);
    HttpResponse handleGetVan(const HttpRequest& req);
    HttpResponse handleUpdateVan(const HttpRequest& req);
    HttpResponse handleDeleteVan(const HttpRequest& req);

    // Route handlers — Commands
    HttpResponse handleReadAttribute(const HttpRequest& req);
    HttpResponse handleInvokeCommand(const HttpRequest& req);
    HttpResponse handleLockVan(const HttpRequest& req);
    HttpResponse handleUnlockVan(const HttpRequest& req);

    // Route handlers — Subscriptions
    HttpResponse handleListVanSubscriptions(const HttpRequest& req);
    HttpResponse handleCreateSubscription(const HttpRequest& req);
    HttpResponse handleDeleteSubscription(const HttpRequest& req);

    // Route handlers — Fleet
    HttpResponse handleFleetStatus(const HttpRequest& req);
    HttpResponse handleFleetAlerts(const HttpRequest& req);
    HttpResponse handleFleetTelemetry(const HttpRequest& req);

    // Route handlers — Events
    HttpResponse handleVanEvents(const HttpRequest& req);
    HttpResponse handleFleetEvents(const HttpRequest& req);

    // Route handlers — Tenants
    HttpResponse handleListTenants(const HttpRequest& req);
    HttpResponse handleCreateTenant(const HttpRequest& req);
    HttpResponse handleGetFabric(const HttpRequest& req);
    HttpResponse handleIssueNOC(const HttpRequest& req);

    // Route handlers — Operational
    HttpResponse handleHealth(const HttpRequest& req);
    HttpResponse handleMetrics(const HttpRequest& req);

    static std::string extractTenantId(const HttpRequest& req);
};

} // namespace mt::gateway
