#include "SimulationModel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Minimal JSON parsing helpers (avoid adding nlohmann/json to iOS target)
// These parse the specific JSON shapes returned by the Dashboard/Gateway APIs

namespace {

// Simple JSON value extractor for known flat objects
std::string extractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";

    // Find the colon after key
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        // String value
        pos++;
        auto end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        // Number or boolean
        auto end = json.find_first_of(",}]\n", pos);
        if (end == std::string::npos) end = json.size();
        std::string val = json.substr(pos, end - pos);
        // Trim whitespace
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
            val.pop_back();
        return val;
    }
}

int extractInt(const std::string& json, const std::string& key) {
    std::string val = extractString(json, key);
    if (val.empty()) return 0;
    try { return std::stoi(val); } catch (...) { return 0; }
}

float extractFloat(const std::string& json, const std::string& key) {
    std::string val = extractString(json, key);
    if (val.empty()) return 0.0f;
    try { return std::stof(val); } catch (...) { return 0.0f; }
}

bool extractBool(const std::string& json, const std::string& key) {
    std::string val = extractString(json, key);
    return val == "true" || val == "1";
}

uint64_t extractUint64(const std::string& json, const std::string& key) {
    std::string val = extractString(json, key);
    if (val.empty()) return 0;
    try { return std::stoull(val); } catch (...) { return 0; }
}

// Split JSON array of objects into individual object strings
std::vector<std::string> splitJsonArray(const std::string& json) {
    std::vector<std::string> objects;
    int depth = 0;
    size_t objStart = 0;
    bool inArray = false;

    for (size_t i = 0; i < json.size(); i++) {
        char c = json[i];
        if (c == '[' && !inArray) { inArray = true; continue; }
        if (!inArray) continue;

        if (c == '{') {
            if (depth == 0) objStart = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                objects.push_back(json.substr(objStart, i - objStart + 1));
            }
        }
    }
    return objects;
}

// Extract a JSON array field value as raw string
std::string extractArray(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "[]";

    pos = json.find('[', pos + search.size());
    if (pos == std::string::npos) return "[]";

    int depth = 0;
    size_t start = pos;
    for (size_t i = pos; i < json.size(); i++) {
        if (json[i] == '[') depth++;
        else if (json[i] == ']') {
            depth--;
            if (depth == 0) return json.substr(start, i - start + 1);
        }
    }
    return "[]";
}

std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

} // anonymous namespace

namespace matter {
namespace sim {

void SimulationState::updateNodes(const std::string& statusJson) {
    nodes_.clear();
    std::string nodesArray = extractArray(statusJson, "nodes");
    auto objects = splitJsonArray(nodesArray);

    for (const auto& obj : objects) {
        NodeState node;
        node.nodeId = static_cast<uint16_t>(extractInt(obj, "id"));
        node.role = extractString(obj, "role");
        node.state = extractString(obj, "state");
        node.pid = extractInt(obj, "pid");
        node.reachable = (node.state == "running");
        nodes_.push_back(std::move(node));
    }
}

void SimulationState::updateTopology(const std::string& topologyJson) {
    std::string linksArray = extractArray(topologyJson, "links");

    // The topology is a 4x4 matrix of link info objects
    // Parse row by row
    int depth = 0;
    int row = 0, col = 0;
    size_t objStart = 0;
    bool inOuterArray = false;

    for (size_t i = 0; i < linksArray.size() && row < 4; i++) {
        char c = linksArray[i];

        if (c == '[') {
            depth++;
            if (depth == 2) { col = 0; } // start of a row
            continue;
        }
        if (c == ']') {
            depth--;
            if (depth == 1) { row++; } // end of a row
            continue;
        }
        if (c == '{') {
            objStart = i;
        }
        if (c == '}' && row < 4 && col < 4) {
            std::string obj = linksArray.substr(objStart, i - objStart + 1);
            auto& link = topology_[row][col];
            link.lossPercent = extractFloat(obj, "loss");
            link.latencyMs = extractFloat(obj, "latency");
            link.up = extractBool(obj, "up");
            link.lqi = static_cast<uint8_t>(extractInt(obj, "lqi"));
            link.rssi = static_cast<int8_t>(extractInt(obj, "rssi"));
            col++;
        }
    }
}

void SimulationState::updateTimeline(const std::string& timelineJson) {
    timeline_.clear();
    auto objects = splitJsonArray(timelineJson);

    for (const auto& obj : objects) {
        TimelineEvent ev;
        ev.timeMs = extractUint64(obj, "time_ms");
        ev.nodeId = static_cast<uint16_t>(extractInt(obj, "node"));
        ev.category = extractString(obj, "category");
        ev.event = extractString(obj, "event");
        ev.detail = extractString(obj, "detail");
        timeline_.push_back(std::move(ev));
    }

    // Keep last 100 events
    if (timeline_.size() > 100) {
        timeline_.erase(timeline_.begin(),
                        timeline_.begin() + (timeline_.size() - 100));
    }
}

void SimulationState::updateMetrics(const std::string& metricsJson) {
    std::string counters = extractArray(metricsJson, "counters");
    // Parse key counters
    metrics_.framesSent = extractUint64(metricsJson, "frames_sent");
    metrics_.framesReceived = extractUint64(metricsJson, "frames_received");
    metrics_.framesDropped = extractUint64(metricsJson, "frames_dropped");
    metrics_.avgLatencyMs = static_cast<double>(extractFloat(metricsJson, "avg_latency_ms"));
}

void SimulationState::updateVans(const std::string& vansJson) {
    vans_.clear();
    auto objects = splitJsonArray(vansJson);

    for (const auto& obj : objects) {
        VanState van;
        van.vanId = extractString(obj, "van_id");
        van.name = extractString(obj, "name");
        van.state = extractString(obj, "state");
        van.deviceId = extractString(obj, "device_id");
        van.locked = extractBool(obj, "locked");
        vans_.push_back(std::move(van));
    }
}

void SimulationState::updateFleetStatus(const std::string& fleetJson) {
    // Fleet status may update van states
    auto objects = splitJsonArray(fleetJson);
    for (const auto& obj : objects) {
        std::string vanId = extractString(obj, "van_id");
        std::string state = extractString(obj, "state");
        for (auto& van : vans_) {
            if (van.vanId == vanId) {
                van.state = state;
                break;
            }
        }
    }
}

void SimulationState::updateAlerts(const std::string& alertsJson) {
    alerts_.clear();
    auto objects = splitJsonArray(alertsJson);

    for (const auto& obj : objects) {
        FleetAlert alert;
        alert.vanId = extractString(obj, "van_id");
        alert.severity = extractString(obj, "severity");
        alert.message = extractString(obj, "message");
        alert.timestampMs = extractUint64(obj, "timestamp_ms");
        alerts_.push_back(std::move(alert));
    }
}

// --- Analysis ---

bool SimulationState::isHealthy() const {
    for (const auto& node : nodes_) {
        if (node.state != "running") return false;
    }
    return !nodes_.empty();
}

int SimulationState::onlineVanCount() const {
    int count = 0;
    for (const auto& van : vans_) {
        if (van.state == "Online") count++;
    }
    return count;
}

int SimulationState::totalVanCount() const {
    return static_cast<int>(vans_.size());
}

std::string SimulationState::nodeDescription(uint16_t nodeId) const {
    for (const auto& node : nodes_) {
        if (node.nodeId == nodeId) {
            std::ostringstream oss;
            oss << "Node " << node.nodeId << " (" << node.role << "): "
                << node.state;
            if (node.pid > 0) oss << " [PID " << node.pid << "]";
            return oss.str();
        }
    }
    return "Node " + std::to_string(nodeId) + ": not found";
}

std::string SimulationState::meshSummary() const {
    std::ostringstream oss;

    if (nodes_.empty()) {
        return "No simulation running. Start MatterThreads first.";
    }

    oss << "Mesh Network (" << nodes_.size() << " nodes):\n";
    for (const auto& node : nodes_) {
        oss << "  Node " << node.nodeId << " [" << node.role << "] — "
            << node.state << "\n";
    }

    // Link summary
    int upLinks = 0, downLinks = 0;
    float totalLoss = 0;
    int linkCount = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i == j) continue;
            if (topology_[i][j].up) {
                upLinks++;
                totalLoss += topology_[i][j].lossPercent;
                linkCount++;
            } else {
                downLinks++;
            }
        }
    }
    oss << "\nLinks: " << upLinks << " up, " << downLinks << " down";
    if (linkCount > 0) {
        oss << " (avg loss: " << std::fixed << std::setprecision(1)
            << (totalLoss / linkCount) << "%)";
    }
    oss << "\n";

    // Metrics
    if (metrics_.framesSent > 0) {
        oss << "\nTraffic: " << metrics_.framesSent << " sent, "
            << metrics_.framesReceived << " received, "
            << metrics_.framesDropped << " dropped\n";
    }

    return oss.str();
}

std::string SimulationState::fleetSummary() const {
    if (vans_.empty()) {
        return "No vans registered. Use the Gateway API to add vans.";
    }

    std::ostringstream oss;
    oss << "Fleet (" << onlineVanCount() << "/" << totalVanCount() << " online):\n";
    for (const auto& van : vans_) {
        oss << "  " << van.name << " [" << van.vanId << "] — " << van.state;
        if (van.locked) oss << " (locked)";
        else oss << " (UNLOCKED)";
        oss << "\n";
    }

    if (!alerts_.empty()) {
        oss << "\nAlerts:\n";
        for (const auto& alert : alerts_) {
            oss << "  [" << alert.severity << "] " << alert.message << "\n";
        }
    }

    return oss.str();
}

std::vector<std::string> SimulationState::activeAlertMessages() const {
    std::vector<std::string> messages;
    for (const auto& alert : alerts_) {
        messages.push_back("[" + alert.severity + "] " + alert.vanId + ": " + alert.message);
    }
    return messages;
}

std::string SimulationState::answerSimulationQuery(const std::string& query) const {
    std::string lower = toLower(query);

    if (lower.find("mesh") != std::string::npos ||
        lower.find("node") != std::string::npos ||
        lower.find("network") != std::string::npos) {
        return meshSummary();
    }

    if (lower.find("fleet") != std::string::npos ||
        lower.find("van") != std::string::npos) {
        return fleetSummary();
    }

    if (lower.find("alert") != std::string::npos ||
        lower.find("problem") != std::string::npos ||
        lower.find("issue") != std::string::npos) {
        auto msgs = activeAlertMessages();
        if (msgs.empty()) return "No active alerts.";
        std::ostringstream oss;
        oss << msgs.size() << " active alert" << (msgs.size() > 1 ? "s" : "") << ":\n";
        for (const auto& m : msgs) oss << "  " << m << "\n";
        return oss.str();
    }

    if (lower.find("healthy") != std::string::npos ||
        lower.find("health") != std::string::npos ||
        lower.find("ok") != std::string::npos) {
        if (isHealthy()) {
            return "All " + std::to_string(nodes_.size()) +
                   " nodes are running. Network is healthy.";
        } else {
            std::ostringstream oss;
            oss << "Network issues detected:\n";
            for (const auto& node : nodes_) {
                if (node.state != "running") {
                    oss << "  Node " << node.nodeId << " (" << node.role
                        << ") is " << node.state << "\n";
                }
            }
            return oss.str();
        }
    }

    if (lower.find("topology") != std::string::npos ||
        lower.find("link") != std::string::npos) {
        std::ostringstream oss;
        oss << "Link Quality Matrix:\n";
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i == j) continue;
                const auto& link = topology_[i][j];
                if (!link.up) {
                    oss << "  " << i << " -> " << j << ": DOWN\n";
                } else if (link.lossPercent > 10.0f) {
                    oss << "  " << i << " -> " << j << ": "
                        << link.lossPercent << "% loss, "
                        << link.latencyMs << "ms\n";
                }
            }
        }
        return oss.str().empty() ? "All links healthy." : oss.str();
    }

    if (lower.find("traffic") != std::string::npos ||
        lower.find("metric") != std::string::npos) {
        std::ostringstream oss;
        oss << "Traffic metrics:\n"
            << "  Frames sent: " << metrics_.framesSent << "\n"
            << "  Frames received: " << metrics_.framesReceived << "\n"
            << "  Frames dropped: " << metrics_.framesDropped << "\n";
        if (metrics_.avgLatencyMs > 0) {
            oss << "  Avg latency: " << std::fixed << std::setprecision(1)
                << metrics_.avgLatencyMs << "ms\n";
        }
        return oss.str();
    }

    // Default: return both summaries
    return meshSummary() + "\n" + fleetSummary();
}

} // namespace sim
} // namespace matter
