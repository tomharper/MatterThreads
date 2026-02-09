#include "hw/OTBRClient.h"
#include "core/Log.h"

#include <curl/curl.h>
#include <stdexcept>

namespace mt::hw {

namespace {
    size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        auto* response = static_cast<std::string*>(userdata);
        response->append(ptr, size * nmemb);
        return size * nmemb;
    }
} // anonymous namespace

OTBRClient::OTBRClient(const std::string& base_url) : base_url_(base_url) {}

Result<std::string> OTBRClient::httpRequest(const std::string& method, const std::string& path,
                                             const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return Error("Failed to initialize libcurl");
    }

    std::string url = base_url_ + path;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_.count()));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeout_.count() / 2));

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        } else {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::string err = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return Error("HTTP " + method + " " + path + " failed: " + err);
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code >= 400) {
        return Error("HTTP " + std::to_string(http_code) + " from " + path + ": " + response);
    }

    return response;
}

Result<std::string> OTBRClient::httpGet(const std::string& path) {
    return httpRequest("GET", path);
}

Result<std::string> OTBRClient::httpPut(const std::string& path, const std::string& body) {
    return httpRequest("PUT", path, body);
}

Result<std::string> OTBRClient::httpPost(const std::string& path, const std::string& body) {
    return httpRequest("POST", path, body);
}

Result<void> OTBRClient::ping() {
    auto result = httpGet("/node/state");
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

BorderRouterState OTBRClient::parseBRState(const nlohmann::json& j) {
    BorderRouterState state;
    if (j.contains("State"))       state.state = j["State"].get<std::string>();
    if (j.contains("RLOC16"))      state.rloc16 = j["RLOC16"].get<uint16_t>();
    if (j.contains("ExtAddress"))  state.ext_address = j["ExtAddress"].get<uint64_t>();
    if (j.contains("Version"))     state.version = j["Version"].get<std::string>();
    if (j.contains("PartitionId")) state.partition_id = j["PartitionId"].get<uint32_t>();
    if (j.contains("LeaderRouterId")) state.leader_router_id = j["LeaderRouterId"].get<uint8_t>();
    return state;
}

ThreadNetworkInfo OTBRClient::parseDataset(const nlohmann::json& j) {
    ThreadNetworkInfo info;
    if (j.contains("NetworkName"))     info.network_name = j["NetworkName"].get<std::string>();
    if (j.contains("Channel"))         info.channel = j["Channel"].get<uint16_t>();
    if (j.contains("PanId"))           info.pan_id = j["PanId"].get<uint16_t>();
    if (j.contains("ExtPanId"))        info.extended_pan_id = j["ExtPanId"].get<std::string>();
    if (j.contains("MeshLocalPrefix")) info.mesh_local_prefix = j["MeshLocalPrefix"].get<std::string>();
    if (j.contains("NetworkKey"))      info.network_key = j["NetworkKey"].get<std::string>();
    if (j.contains("ActiveDatasetTlvs")) info.active_dataset_tlv = j["ActiveDatasetTlvs"].get<std::string>();
    return info;
}

ThreadDeviceInfo OTBRClient::parseDeviceInfo(const nlohmann::json& j) {
    ThreadDeviceInfo info;
    if (j.contains("Rloc16"))           info.rloc16 = j["Rloc16"].get<uint16_t>();
    if (j.contains("ExtAddress"))       info.ext_address = j["ExtAddress"].get<uint64_t>();
    if (j.contains("Mode"))             info.mode = j["Mode"].get<std::string>();
    if (j.contains("IsChild"))          info.is_child = j["IsChild"].get<bool>();
    if (j.contains("AverageRssi"))      info.avg_rssi = j["AverageRssi"].get<int8_t>();
    if (j.contains("LastRssi"))         info.last_rssi = j["LastRssi"].get<int8_t>();
    if (j.contains("FrameErrorRate"))   info.frame_error_rate = j["FrameErrorRate"].get<uint32_t>();
    if (j.contains("MessageErrorRate")) info.message_error_rate = j["MessageErrorRate"].get<uint32_t>();
    return info;
}

Result<BorderRouterState> OTBRClient::getState() {
    auto result = httpGet("/node/state");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return parseBRState(j);
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse state: ") + e.what());
    }
}

Result<ThreadNetworkInfo> OTBRClient::getActiveDataset() {
    auto result = httpGet("/node/dataset/active");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return parseDataset(j);
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse dataset: ") + e.what());
    }
}

Result<void> OTBRClient::setActiveDataset(const std::string& dataset_tlv_hex) {
    nlohmann::json body;
    body["ActiveDatasetTlvs"] = dataset_tlv_hex;
    auto result = httpPut("/node/dataset/active", body.dump());
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<ThreadNetworkInfo> OTBRClient::getPendingDataset() {
    auto result = httpGet("/node/dataset/pending");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return parseDataset(j);
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse pending dataset: ") + e.what());
    }
}

Result<std::vector<ThreadDeviceInfo>> OTBRClient::getNeighborTable() {
    auto result = httpGet("/diagnostics");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        std::vector<ThreadDeviceInfo> devices;
        auto& neighbors = j.contains("NeighborTable") ? j["NeighborTable"] : j;
        if (neighbors.is_array()) {
            for (const auto& item : neighbors) {
                devices.push_back(parseDeviceInfo(item));
            }
        }
        return devices;
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse neighbor table: ") + e.what());
    }
}

Result<std::vector<ThreadDeviceInfo>> OTBRClient::getChildTable() {
    auto result = httpGet("/diagnostics");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        std::vector<ThreadDeviceInfo> devices;
        if (j.contains("ChildTable") && j["ChildTable"].is_array()) {
            for (const auto& item : j["ChildTable"]) {
                auto dev = parseDeviceInfo(item);
                dev.is_child = true;
                devices.push_back(dev);
            }
        }
        return devices;
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse child table: ") + e.what());
    }
}

Result<uint16_t> OTBRClient::getRLOC16() {
    auto result = httpGet("/node/rloc16");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return j.get<uint16_t>();
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse RLOC16: ") + e.what());
    }
}

Result<uint64_t> OTBRClient::getExtAddress() {
    auto result = httpGet("/node/ext-address");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return j.get<uint64_t>();
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse ext address: ") + e.what());
    }
}

Result<void> OTBRClient::enableThread() {
    auto result = httpPost("/node/enable");
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<void> OTBRClient::disableThread() {
    auto result = httpPost("/node/disable");
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<std::string> OTBRClient::getBorderAgentId() {
    auto result = httpGet("/node/ba-id");
    if (!result.ok()) return Error(result.error().message);
    try {
        auto j = nlohmann::json::parse(*result);
        return j.get<std::string>();
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse Border Agent ID: ") + e.what());
    }
}

Result<void> OTBRClient::startCommissioner() {
    auto result = httpPost("/node/commissioner/start");
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<void> OTBRClient::stopCommissioner() {
    auto result = httpPost("/node/commissioner/stop");
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<void> OTBRClient::addJoiner(const std::string& eui64, const std::string& pskd) {
    nlohmann::json body;
    body["EUI64"] = eui64;
    body["PSKd"] = pskd;
    auto result = httpPost("/node/commissioner/joiner/add", body.dump());
    if (!result.ok()) return Error(result.error().message);
    return Result<void>::success();
}

Result<nlohmann::json> OTBRClient::getSRPHosts() {
    auto result = httpGet("/node/srp-server/hosts");
    if (!result.ok()) return Error(result.error().message);
    try {
        return nlohmann::json::parse(*result);
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse SRP hosts: ") + e.what());
    }
}

Result<nlohmann::json> OTBRClient::getSRPServices() {
    auto result = httpGet("/node/srp-server/services");
    if (!result.ok()) return Error(result.error().message);
    try {
        return nlohmann::json::parse(*result);
    } catch (const std::exception& e) {
        return Error(std::string("Failed to parse SRP services: ") + e.what());
    }
}

} // namespace mt::hw
