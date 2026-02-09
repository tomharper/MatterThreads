#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/DataModel.h"
#include "matter/Session.h"
#include "matter/InteractionModel.h"
#include "hw/ProcessManager.h"
#include "hw/ChipToolOutputParser.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace mt::hw {

struct ChipToolConfig {
    std::string binary_path =
#ifdef CHIP_TOOL_DEFAULT_PATH
        CHIP_TOOL_DEFAULT_PATH;
#else
        "chip-tool";
#endif
    std::string storage_dir;
    std::string commissioner_name = "mt-ctrl";
    Duration command_timeout = Duration(60000);
    uint16_t pase_port = 5540;
    std::string log_level = "detail";
};

struct SubscriptionHandle {
    SubscriptionId id = 0;
    ProcessManager process;
    AttributePath path{};
    std::function<void(const AttributeValue&)> on_report;
    std::function<void(const Error&)> on_error;
};

class ChipToolDriver {
public:
    explicit ChipToolDriver(ChipToolConfig config = {});
    ~ChipToolDriver();

    Result<std::string> version();

    // Commission / Session
    Result<PairingResult> pairOnNetwork(
        uint64_t node_id, uint32_t setup_code,
        const std::string& device_ip = "", uint16_t port = 5540);
    Result<PairingResult> pairBLE(
        uint64_t node_id, uint32_t setup_code, uint16_t discriminator,
        const std::string& thread_dataset = "");
    Result<PairingResult> establishCASE(uint64_t node_id);
    Result<void> unpair(uint64_t node_id);

    // IM Operations
    Result<ReadAttributeResult> readAttribute(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute);
    Result<WriteAttributeResult> writeAttribute(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute,
        const std::string& value_str);
    Result<InvokeResult> invokeCommand(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, CommandId command,
        const std::string& payload = "");
    Result<InvokeResult> timedInvoke(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, CommandId command,
        Duration timed_interaction_timeout,
        const std::string& payload = "");

    // Subscriptions
    Result<SubscriptionId> subscribe(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute,
        Duration min_interval, Duration max_interval,
        std::function<void(const AttributeValue&)> on_report);
    void cancelSubscription(SubscriptionId id);

    // Fabric
    Result<std::vector<uint8_t>> readFabrics(uint64_t node_id);
    Result<void> removeFabric(uint64_t node_id, FabricIndex index);

    bool isAvailable() const;
    void tick();
    size_t activeSubscriptionCount() const;

    const ChipToolConfig& config() const { return config_; }

    // Exposed for testing: build command args without running
    std::vector<std::string> buildReadArgs(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute) const;
    std::vector<std::string> buildWriteArgs(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute,
        const std::string& value_str) const;
    std::vector<std::string> buildPairingArgs(
        uint64_t node_id, uint32_t setup_code,
        const std::string& device_ip, uint16_t port) const;
    std::vector<std::string> buildSubscribeArgs(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, AttributeId attribute,
        Duration min_interval, Duration max_interval) const;
    std::vector<std::string> buildInvokeArgs(
        uint64_t node_id, EndpointId endpoint,
        ClusterId cluster, CommandId command,
        const std::string& payload) const;

private:
    ChipToolConfig config_;
    ProcessManager process_;
    std::unordered_map<SubscriptionId, std::unique_ptr<SubscriptionHandle>> subscriptions_;
    SubscriptionId next_sub_id_ = 1;

    std::vector<std::string> baseArgs() const;
    ProcessConfig buildCommand(const std::vector<std::string>& args) const;
    Result<ProcessOutput> runCommand(const std::vector<std::string>& args);

    // Cluster/attribute name lookup for chip-tool CLI
    static std::string clusterName(ClusterId id);
    static std::string attributeReadName(ClusterId cluster, AttributeId attr);
    static std::string commandName(ClusterId cluster, CommandId cmd);
};

} // namespace mt::hw
