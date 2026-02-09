#include "gateway/GatewayServer.h"
#include "core/Log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>

namespace mt::gateway {

GatewayServer::GatewayServer(const GatewayConfig& config,
                             VanRegistry& registry,
                             CASESessionPool& sessions,
                             FleetSubscriptionManager& subscriptions,
                             CommandRelay& commands,
                             OfflineBuffer& buffer,
                             FabricManager& fabrics)
    : config_(config)
    , registry_(registry)
    , sessions_(sessions)
    , subscriptions_(subscriptions)
    , commands_(commands)
    , buffer_(buffer)
    , fabrics_(fabrics)
{
    registerRoutes();
}

// ── Path utilities ──────────────────────────────────────────────────────────

std::vector<std::string> GatewayServer::splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::istringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) segments.push_back(segment);
    }
    return segments;
}

void GatewayServer::addRoute(const std::string& method, const std::string& pattern,
                             RouteHandler handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    route.handler = std::move(handler);
    route.segments = splitPath(pattern);

    for (size_t i = 0; i < route.segments.size(); ++i) {
        const auto& seg = route.segments[i];
        if (seg.size() >= 3 && seg.front() == '{' && seg.back() == '}') {
            route.param_indices.emplace_back(i, seg.substr(1, seg.size() - 2));
        }
    }

    routes_.push_back(std::move(route));
}

bool GatewayServer::matchRoute(const Route& route, const std::string& method,
                               const std::string& path,
                               std::unordered_map<std::string, std::string>& params) {
    if (route.method != method) return false;

    auto path_segments = splitPath(path);
    if (path_segments.size() != route.segments.size()) return false;

    params.clear();
    for (size_t i = 0; i < route.segments.size(); ++i) {
        const auto& pattern_seg = route.segments[i];
        if (pattern_seg.size() >= 3 && pattern_seg.front() == '{' && pattern_seg.back() == '}') {
            std::string name = pattern_seg.substr(1, pattern_seg.size() - 2);
            params[name] = path_segments[i];
        } else if (pattern_seg != path_segments[i]) {
            return false;
        }
    }
    return true;
}

// ── Route registration ──────────────────────────────────────────────────────

void GatewayServer::registerRoutes() {
    // Vans CRUD
    addRoute("GET", "/api/vans", [this](const HttpRequest& r) { return handleListVans(r); });
    addRoute("POST", "/api/vans", [this](const HttpRequest& r) { return handleRegisterVan(r); });
    addRoute("GET", "/api/vans/{van_id}", [this](const HttpRequest& r) { return handleGetVan(r); });
    addRoute("PUT", "/api/vans/{van_id}", [this](const HttpRequest& r) { return handleUpdateVan(r); });
    addRoute("DELETE", "/api/vans/{van_id}", [this](const HttpRequest& r) { return handleDeleteVan(r); });

    // Commands
    addRoute("GET", "/api/vans/{van_id}/endpoints/{ep}/clusters/{cl}/attributes/{attr}",
             [this](const HttpRequest& r) { return handleReadAttribute(r); });
    addRoute("POST", "/api/vans/{van_id}/endpoints/{ep}/clusters/{cl}/commands/{cmd}",
             [this](const HttpRequest& r) { return handleInvokeCommand(r); });
    addRoute("POST", "/api/vans/{van_id}/lock", [this](const HttpRequest& r) { return handleLockVan(r); });
    addRoute("POST", "/api/vans/{van_id}/unlock", [this](const HttpRequest& r) { return handleUnlockVan(r); });

    // Subscriptions
    addRoute("GET", "/api/vans/{van_id}/subscriptions",
             [this](const HttpRequest& r) { return handleListVanSubscriptions(r); });
    addRoute("POST", "/api/subscriptions", [this](const HttpRequest& r) { return handleCreateSubscription(r); });
    addRoute("DELETE", "/api/subscriptions/{sub_id}",
             [this](const HttpRequest& r) { return handleDeleteSubscription(r); });

    // Fleet
    addRoute("GET", "/api/fleet/status", [this](const HttpRequest& r) { return handleFleetStatus(r); });
    addRoute("GET", "/api/fleet/alerts", [this](const HttpRequest& r) { return handleFleetAlerts(r); });
    addRoute("GET", "/api/fleet/telemetry", [this](const HttpRequest& r) { return handleFleetTelemetry(r); });

    // Events
    addRoute("GET", "/api/vans/{van_id}/events", [this](const HttpRequest& r) { return handleVanEvents(r); });
    addRoute("GET", "/api/events", [this](const HttpRequest& r) { return handleFleetEvents(r); });

    // Tenants
    addRoute("GET", "/api/tenants", [this](const HttpRequest& r) { return handleListTenants(r); });
    addRoute("POST", "/api/tenants", [this](const HttpRequest& r) { return handleCreateTenant(r); });
    addRoute("GET", "/api/tenants/{id}/fabric", [this](const HttpRequest& r) { return handleGetFabric(r); });
    addRoute("POST", "/api/tenants/{id}/fabric/noc", [this](const HttpRequest& r) { return handleIssueNOC(r); });

    // Operational
    addRoute("GET", "/api/health", [this](const HttpRequest& r) { return handleHealth(r); });
    addRoute("GET", "/api/metrics", [this](const HttpRequest& r) { return handleMetrics(r); });
}

// ── Server lifecycle ────────────────────────────────────────────────────────

Result<void> GatewayServer::start() {
    auto result = Socket::listen(config_.api_port);
    if (!result) {
        return Error("Gateway server bind failed on port " +
                     std::to_string(config_.api_port) + ": " + result.error().message);
    }

    listen_socket_ = std::move(*result);
    auto nb = listen_socket_.setNonBlocking(true);
    if (!nb) return Error("Failed to set non-blocking: " + nb.error().message);

    running_ = true;
    MT_INFO("gateway", "Fleet Gateway listening on http://localhost:" +
            std::to_string(config_.api_port));
    return Result<void>::success();
}

void GatewayServer::poll() {
    if (!running_) return;

    auto result = listen_socket_.accept();
    if (!result) return;  // EAGAIN in non-blocking mode

    handleClient(std::move(*result));
}

void GatewayServer::stop() {
    running_ = false;
    listen_socket_.close();
    MT_INFO("gateway", "Fleet Gateway stopped");
}

// ── HTTP parsing ────────────────────────────────────────────────────────────

HttpRequest GatewayServer::parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;

    // Request line: METHOD /path?query HTTP/1.1
    if (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream rl(line);
        std::string full_path, version;
        rl >> req.method >> full_path >> version;

        // Split path and query
        auto qpos = full_path.find('?');
        if (qpos != std::string::npos) {
            req.path = full_path.substr(0, qpos);
            std::string query = full_path.substr(qpos + 1);
            // Parse query params
            std::istringstream qs(query);
            std::string pair;
            while (std::getline(qs, pair, '&')) {
                auto eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.query_params[pair.substr(0, eq)] = pair.substr(eq + 1);
                }
            }
        } else {
            req.path = full_path;
        }
    }

    // Headers
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading whitespace from value
            auto start = value.find_first_not_of(' ');
            if (start != std::string::npos) value = value.substr(start);
            // Lowercase the key for case-insensitive lookup
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = value;
        }
    }

    // Body: everything after blank line
    auto body_start = raw.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        req.body = raw.substr(body_start + 4);
    }

    return req;
}

HttpResponse GatewayServer::routeRequest(const HttpRequest& request) {
    for (const auto& route : routes_) {
        std::unordered_map<std::string, std::string> params;
        if (matchRoute(route, request.method, request.path, params)) {
            HttpRequest req_with_params = request;
            req_with_params.path_params = std::move(params);
            return route.handler(req_with_params);
        }
    }
    return HttpResponse::error(404, "Not Found");
}

void GatewayServer::handleClient(Socket client) {
    char buf[8192];
    ssize_t n = ::recv(client.fd(), buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    auto request = parseRequest(std::string(buf, static_cast<size_t>(n)));
    auto response = routeRequest(request);
    sendResponse(client, response);
}

void GatewayServer::sendResponse(Socket& client, const HttpResponse& response) {
    std::string status_text;
    switch (response.status_code) {
        case 200: status_text = "OK"; break;
        case 201: status_text = "Created"; break;
        case 204: status_text = "No Content"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 409: status_text = "Conflict"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Error"; break;
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << response.status_code << " " << status_text << "\r\n"
         << "Content-Type: " << response.content_type << "\r\n"
         << "Content-Length: " << response.body.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << response.body;

    std::string raw = resp.str();
    ::send(client.fd(), raw.data(), raw.size(), 0);
}

std::string GatewayServer::extractTenantId(const HttpRequest& req) {
    auto it = req.headers.find("x-tenant-id");
    if (it != req.headers.end()) return it->second;
    return "";
}

// ── Route handlers: Vans ────────────────────────────────────────────────────

HttpResponse GatewayServer::handleListVans(const HttpRequest& req) {
    auto tenant = extractTenantId(req);
    std::vector<VanRegistration> vans;
    if (!tenant.empty()) {
        uint32_t tid = 0;
        try { tid = static_cast<uint32_t>(std::stoul(tenant)); } catch (...) {}
        vans = registry_.listVansByTenant(tid);
    } else {
        vans = registry_.listVans();
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& v : vans) {
        arr.push_back(registry_.vanToJson(v));
    }
    return HttpResponse::json(arr);
}

HttpResponse GatewayServer::handleRegisterVan(const HttpRequest& req) {
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); } catch (...) {
        return HttpResponse::error(400, "Invalid JSON body");
    }

    VanRegistration reg;
    if (body.contains("van_id")) reg.van_id = body["van_id"].get<std::string>();
    if (body.contains("device_id")) reg.device_id = body["device_id"].get<uint64_t>();
    if (body.contains("tenant_id")) reg.tenant_id = body["tenant_id"].get<uint32_t>();
    if (body.contains("name")) reg.name = body["name"].get<std::string>();

    auto result = registry_.registerVan(reg);
    if (!result.ok()) return HttpResponse::error(409, result.error().message);

    return HttpResponse::json(registry_.vanToJson(reg), 201);
}

HttpResponse GatewayServer::handleGetVan(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto* van = registry_.getVan(van_id);
    if (!van) return HttpResponse::error(404, "Van not found: " + van_id);
    return HttpResponse::json(registry_.vanToJson(*van));
}

HttpResponse GatewayServer::handleUpdateVan(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); } catch (...) {
        return HttpResponse::error(400, "Invalid JSON body");
    }

    auto* existing = registry_.getVan(van_id);
    if (!existing) return HttpResponse::error(404, "Van not found: " + van_id);

    VanRegistration updated = *existing;
    if (body.contains("name")) updated.name = body["name"].get<std::string>();
    if (body.contains("device_id")) updated.device_id = body["device_id"].get<uint64_t>();

    registry_.updateVan(van_id, updated);
    return HttpResponse::json(registry_.vanToJson(updated));
}

HttpResponse GatewayServer::handleDeleteVan(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    registry_.deregisterVan(van_id);
    sessions_.disconnect(van_id);
    subscriptions_.unsubscribeVan(van_id);
    return HttpResponse::ok();
}

// ── Route handlers: Commands ────────────────────────────────────────────────

HttpResponse GatewayServer::handleReadAttribute(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto ep = static_cast<EndpointId>(std::stoul(req.path_params.at("ep")));
    auto cl = static_cast<ClusterId>(std::stoul(req.path_params.at("cl")));
    auto attr = static_cast<AttributeId>(std::stoul(req.path_params.at("attr")));

    auto* van = registry_.getVan(van_id);
    if (!van) return HttpResponse::error(404, "Van not found");

    auto result = commands_.readAttribute(van_id, van->device_id, ep, cl, attr);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    nlohmann::json j = {
        {"success", result->success},
        {"error_message", result->error_message}
    };
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleInvokeCommand(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto ep = static_cast<EndpointId>(std::stoul(req.path_params.at("ep")));
    auto cl = static_cast<ClusterId>(std::stoul(req.path_params.at("cl")));
    auto cmd = static_cast<CommandId>(std::stoul(req.path_params.at("cmd")));

    auto* van = registry_.getVan(van_id);
    if (!van) return HttpResponse::error(404, "Van not found");

    std::string payload;
    if (!req.body.empty()) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("payload")) payload = body["payload"].get<std::string>();
        } catch (...) {}
    }

    auto result = commands_.invoke(van_id, van->device_id, ep, cl, cmd, payload);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    nlohmann::json j = {
        {"success", result->success},
        {"error_message", result->error_message}
    };
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleLockVan(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto* van = registry_.getVan(van_id);
    if (!van) return HttpResponse::error(404, "Van not found");

    // DoorLock cluster 0x0101, LockDoor command 0x0000
    auto result = commands_.invoke(van_id, van->device_id, 1, 0x0101, 0x0000);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    nlohmann::json j = {{"success", result->success}, {"action", "lock"}};
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleUnlockVan(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto* van = registry_.getVan(van_id);
    if (!van) return HttpResponse::error(404, "Van not found");

    // DoorLock cluster 0x0101, UnlockDoor command 0x0001
    auto result = commands_.invoke(van_id, van->device_id, 1, 0x0101, 0x0001);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    nlohmann::json j = {{"success", result->success}, {"action", "unlock"}};
    return HttpResponse::json(j);
}

// ── Route handlers: Subscriptions ───────────────────────────────────────────

HttpResponse GatewayServer::handleListVanSubscriptions(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    auto subs = subscriptions_.getVanSubscriptions(van_id);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : subs) {
        arr.push_back({
            {"van_id", s.van_id},
            {"rule_id", s.rule_id},
            {"active", s.active},
            {"report_count", s.report_count},
            {"missed_reports", s.missed_reports}
        });
    }
    return HttpResponse::json(arr);
}

HttpResponse GatewayServer::handleCreateSubscription(const HttpRequest& req) {
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); } catch (...) {
        return HttpResponse::error(400, "Invalid JSON body");
    }

    SubscriptionRule rule;
    if (body.contains("name")) rule.name = body["name"].get<std::string>();
    if (body.contains("endpoint")) rule.endpoint = body["endpoint"].get<EndpointId>();
    if (body.contains("cluster")) rule.cluster = body["cluster"].get<ClusterId>();
    if (body.contains("attribute")) rule.attribute = body["attribute"].get<AttributeId>();
    if (body.contains("min_interval_ms")) rule.min_interval = Duration(body["min_interval_ms"].get<uint32_t>());
    if (body.contains("max_interval_ms")) rule.max_interval = Duration(body["max_interval_ms"].get<uint32_t>());
    if (body.contains("critical")) rule.critical = body["critical"].get<bool>();

    auto id = subscriptions_.addRule(rule);
    nlohmann::json j = {{"rule_id", id}, {"name", rule.name}};
    return HttpResponse::json(j, 201);
}

HttpResponse GatewayServer::handleDeleteSubscription(const HttpRequest& req) {
    auto sub_id = static_cast<uint32_t>(std::stoul(req.path_params.at("sub_id")));
    subscriptions_.removeRule(sub_id);
    return HttpResponse::ok();
}

// ── Route handlers: Fleet ───────────────────────────────────────────────────

HttpResponse GatewayServer::handleFleetStatus(const HttpRequest&) {
    auto vans = registry_.listVans();
    size_t online = 0, offline = 0;
    for (const auto& v : vans) {
        if (v.state == VanState::Online) ++online;
        else ++offline;
    }

    nlohmann::json j = {
        {"total_vans", vans.size()},
        {"online", online},
        {"offline", offline},
        {"active_subscriptions", subscriptions_.activeSubscriptionCount()},
        {"buffered_events", buffer_.totalEventCount()},
        {"connected_sessions", sessions_.connectedVans().size()}
    };
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleFleetAlerts(const HttpRequest&) {
    // Alerts based on van states
    nlohmann::json alerts = nlohmann::json::array();
    for (const auto& v : registry_.listVans()) {
        if (v.state == VanState::Unreachable) {
            alerts.push_back({
                {"van_id", v.van_id},
                {"type", "unreachable"},
                {"message", "Van " + v.van_id + " is unreachable"}
            });
        }
    }
    return HttpResponse::json(alerts);
}

HttpResponse GatewayServer::handleFleetTelemetry(const HttpRequest&) {
    nlohmann::json j = {
        {"total_reports", subscriptions_.totalReportCount()},
        {"active_subscriptions", subscriptions_.activeSubscriptionCount()},
        {"buffered_events", buffer_.totalEventCount()}
    };
    return HttpResponse::json(j);
}

// ── Route handlers: Events ──────────────────────────────────────────────────

HttpResponse GatewayServer::handleVanEvents(const HttpRequest& req) {
    auto van_id = req.path_params.at("van_id");
    uint64_t since = 0;
    auto it = req.query_params.find("since");
    if (it != req.query_params.end()) {
        try { since = std::stoull(it->second); } catch (...) {}
    }

    auto events = buffer_.drain(van_id, since);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : events) {
        arr.push_back({
            {"sequence_id", e.sequence_id},
            {"van_id", e.van_id},
            {"event_type", e.event_type},
            {"payload", e.payload},
            {"timestamp_ms", e.timestamp.time_since_epoch().count()}
        });
    }
    return HttpResponse::json(arr);
}

HttpResponse GatewayServer::handleFleetEvents(const HttpRequest& req) {
    uint64_t since = 0;
    auto it = req.query_params.find("since");
    if (it != req.query_params.end()) {
        try { since = std::stoull(it->second); } catch (...) {}
    }

    auto events = buffer_.drainAll(since);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : events) {
        arr.push_back({
            {"sequence_id", e.sequence_id},
            {"van_id", e.van_id},
            {"event_type", e.event_type},
            {"payload", e.payload},
            {"timestamp_ms", e.timestamp.time_since_epoch().count()}
        });
    }
    return HttpResponse::json(arr);
}

// ── Route handlers: Tenants ─────────────────────────────────────────────────

HttpResponse GatewayServer::handleListTenants(const HttpRequest&) {
    auto tenants = fabrics_.listTenants();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : tenants) {
        arr.push_back({
            {"tenant_id", t.tenant_id},
            {"name", t.name},
            {"fabric_id", t.fabric_id},
            {"max_vans", t.max_vans}
        });
    }
    return HttpResponse::json(arr);
}

HttpResponse GatewayServer::handleCreateTenant(const HttpRequest& req) {
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); } catch (...) {
        return HttpResponse::error(400, "Invalid JSON body");
    }

    std::string name = body.value("name", "unnamed");
    auto result = fabrics_.createTenant(name);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    TenantId tid = *result;
    auto* tenant = fabrics_.getTenant(tid);
    nlohmann::json j = {
        {"tenant_id", tid},
        {"name", tenant ? tenant->name : name},
        {"fabric_id", tenant ? tenant->fabric_id : 0}
    };
    return HttpResponse::json(j, 201);
}

HttpResponse GatewayServer::handleGetFabric(const HttpRequest& req) {
    auto tenant_id = static_cast<uint32_t>(std::stoul(req.path_params.at("id")));
    auto* tenant = fabrics_.getTenant(tenant_id);
    if (!tenant) return HttpResponse::error(404, "Tenant not found");

    nlohmann::json j = {
        {"tenant_id", tenant->tenant_id},
        {"fabric_id", tenant->fabric_id},
        {"root_cert_size", tenant->root_cert.size()},
        {"ipk_size", tenant->ipk.size()},
        {"credential_count", fabrics_.credentialCount(tenant_id)}
    };
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleIssueNOC(const HttpRequest& req) {
    auto tenant_id = static_cast<uint32_t>(std::stoul(req.path_params.at("id")));
    nlohmann::json body;
    try { body = nlohmann::json::parse(req.body); } catch (...) {
        return HttpResponse::error(400, "Invalid JSON body");
    }

    std::string van_id = body.value("van_id", "");
    uint64_t device_id = body.value("device_id", static_cast<uint64_t>(0));

    auto result = fabrics_.issueNOC(tenant_id, van_id, device_id);
    if (!result.ok()) return HttpResponse::error(500, result.error().message);

    nlohmann::json j = {
        {"van_id", result->van_id},
        {"node_id", result->node_id},
        {"noc_size", result->noc.size()}
    };
    return HttpResponse::json(j, 201);
}

// ── Route handlers: Operational ─────────────────────────────────────────────

HttpResponse GatewayServer::handleHealth(const HttpRequest&) {
    nlohmann::json j = {
        {"status", "healthy"},
        {"connected_vans", sessions_.connectedVans().size()},
        {"running", running_}
    };
    return HttpResponse::json(j);
}

HttpResponse GatewayServer::handleMetrics(const HttpRequest&) {
    nlohmann::json j = {
        {"active_subscriptions", subscriptions_.activeSubscriptionCount()},
        {"total_reports", subscriptions_.totalReportCount()},
        {"buffered_events", buffer_.totalEventCount()},
        {"registered_vans", registry_.listVans().size()},
        {"connected_sessions", sessions_.connectedVans().size()}
    };
    return HttpResponse::json(j);
}

} // namespace mt::gateway
