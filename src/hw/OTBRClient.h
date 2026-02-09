#pragma once

#include "core/Types.h"
#include "core/Result.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace mt::hw {

struct ThreadNetworkInfo {
    std::string network_name;
    uint16_t channel = 0;
    uint16_t pan_id = 0;
    std::string extended_pan_id;
    std::string mesh_local_prefix;
    std::string network_key;
    std::string active_dataset_tlv;
};

struct ThreadDeviceInfo {
    uint16_t rloc16 = 0;
    uint64_t ext_address = 0;
    std::string mode;
    bool is_child = false;
    int8_t avg_rssi = 0;
    int8_t last_rssi = 0;
    uint32_t frame_error_rate = 0;
    uint32_t message_error_rate = 0;
};

struct BorderRouterState {
    std::string state;
    uint16_t rloc16 = 0;
    uint64_t ext_address = 0;
    std::string version;
    uint32_t partition_id = 0;
    uint8_t leader_router_id = 0;
};

class OTBRClient {
public:
    explicit OTBRClient(const std::string& base_url = "http://localhost:8081");

    Result<BorderRouterState> getState();
    Result<ThreadNetworkInfo> getActiveDataset();
    Result<void> setActiveDataset(const std::string& dataset_tlv_hex);
    Result<ThreadNetworkInfo> getPendingDataset();

    Result<std::vector<ThreadDeviceInfo>> getNeighborTable();
    Result<std::vector<ThreadDeviceInfo>> getChildTable();
    Result<uint16_t> getRLOC16();
    Result<uint64_t> getExtAddress();

    Result<void> enableThread();
    Result<void> disableThread();
    Result<std::string> getBorderAgentId();

    Result<void> startCommissioner();
    Result<void> stopCommissioner();
    Result<void> addJoiner(const std::string& eui64, const std::string& pskd);

    Result<nlohmann::json> getSRPHosts();
    Result<nlohmann::json> getSRPServices();

    void setBaseUrl(const std::string& url) { base_url_ = url; }
    const std::string& baseUrl() const { return base_url_; }
    void setTimeout(Duration timeout) { timeout_ = timeout; }

    Result<void> ping();

private:
    std::string base_url_;
    Duration timeout_ = Duration(10000);

    Result<std::string> httpGet(const std::string& path);
    Result<std::string> httpPut(const std::string& path, const std::string& body);
    Result<std::string> httpPost(const std::string& path, const std::string& body = "");
    Result<std::string> httpRequest(const std::string& method, const std::string& path,
                                     const std::string& body = "");

    static BorderRouterState parseBRState(const nlohmann::json& j);
    static ThreadNetworkInfo parseDataset(const nlohmann::json& j);
    static ThreadDeviceInfo parseDeviceInfo(const nlohmann::json& j);
};

} // namespace mt::hw
