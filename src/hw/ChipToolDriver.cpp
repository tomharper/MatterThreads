#include "hw/ChipToolDriver.h"
#include "core/Log.h"

#include <sstream>

namespace mt::hw {

// ---- Cluster/Attribute/Command name lookup tables ----

std::string ChipToolDriver::clusterName(ClusterId id) {
    switch (id) {
        case 0x0006: return "onoff";
        case 0x0008: return "levelcontrol";
        case 0x0300: return "colorcontrol";
        case 0x0101: return "doorlock";
        case 0x0201: return "thermostat";
        case 0x001D: return "descriptor";
        case 0x0028: return "basicinformation";
        case 0x003E: return "operationalcredentials";
        case 0x0031: return "networkcommissioning";
        case 0x0030: return "generalcommissioning";
        case 0x002A: return "otasoftwareupdaterequestor";
        case 0x0405: return "relativehumiditymeasurement";
        case 0x0402: return "temperaturemeasurement";
        case 0x0400: return "illuminancemeasurement";
        case 0x003F: return "groupkeymanagement";
        default: return "";
    }
}

std::string ChipToolDriver::attributeReadName(ClusterId cluster, AttributeId attr) {
    // OnOff cluster
    if (cluster == 0x0006) {
        if (attr == 0x0000) return "on-off";
        if (attr == 0x4000) return "global-scene-control";
        if (attr == 0x4001) return "on-time";
        if (attr == 0x4002) return "off-wait-time";
    }
    // LevelControl
    if (cluster == 0x0008) {
        if (attr == 0x0000) return "current-level";
        if (attr == 0x0001) return "remaining-time";
        if (attr == 0x0002) return "min-level";
        if (attr == 0x0003) return "max-level";
    }
    // DoorLock
    if (cluster == 0x0101) {
        if (attr == 0x0000) return "lock-state";
        if (attr == 0x0001) return "lock-type";
        if (attr == 0x0002) return "actuator-enabled";
    }
    // Thermostat
    if (cluster == 0x0201) {
        if (attr == 0x0000) return "local-temperature";
        if (attr == 0x0012) return "occupied-heating-setpoint";
        if (attr == 0x0013) return "occupied-cooling-setpoint";
        if (attr == 0x001C) return "system-mode";
    }
    // Temperature Measurement
    if (cluster == 0x0402) {
        if (attr == 0x0000) return "measured-value";
        if (attr == 0x0001) return "min-measured-value";
        if (attr == 0x0002) return "max-measured-value";
    }
    // BasicInformation
    if (cluster == 0x0028) {
        if (attr == 0x0000) return "data-model-revision";
        if (attr == 0x0001) return "vendor-name";
        if (attr == 0x0002) return "vendor-id";
        if (attr == 0x0003) return "product-name";
        if (attr == 0x0005) return "node-label";
    }
    return "";
}

std::string ChipToolDriver::commandName(ClusterId cluster, CommandId cmd) {
    // OnOff
    if (cluster == 0x0006) {
        if (cmd == 0x0000) return "off";
        if (cmd == 0x0001) return "on";
        if (cmd == 0x0002) return "toggle";
    }
    // DoorLock
    if (cluster == 0x0101) {
        if (cmd == 0x0000) return "lock-door";
        if (cmd == 0x0001) return "unlock-door";
    }
    // LevelControl
    if (cluster == 0x0008) {
        if (cmd == 0x0000) return "move-to-level";
        if (cmd == 0x0004) return "move-to-level-with-on-off";
    }
    return "";
}

// ---- Construction ----

ChipToolDriver::ChipToolDriver(ChipToolConfig config) : config_(std::move(config)) {}

ChipToolDriver::~ChipToolDriver() {
    for (auto& [id, handle] : subscriptions_) {
        handle->process.kill();
    }
}

std::vector<std::string> ChipToolDriver::baseArgs() const {
    std::vector<std::string> args;
    if (!config_.commissioner_name.empty()) {
        args.push_back("--commissioner-name");
        args.push_back(config_.commissioner_name);
    }
    if (!config_.storage_dir.empty()) {
        args.push_back("--storage-directory");
        args.push_back(config_.storage_dir);
    }
    return args;
}

ProcessConfig ChipToolDriver::buildCommand(const std::vector<std::string>& args) const {
    ProcessConfig pc;
    pc.binary_path = config_.binary_path;
    pc.args = args;
    pc.timeout = config_.command_timeout;
    return pc;
}

Result<ProcessOutput> ChipToolDriver::runCommand(const std::vector<std::string>& args) {
    auto cmd = buildCommand(args);
    return process_.run(cmd);
}

bool ChipToolDriver::isAvailable() const {
    ProcessManager pm;
    ProcessConfig check;
    check.binary_path = config_.binary_path;
    check.args = {"--help"};
    check.timeout = Duration(5000);
    auto result = pm.run(check);
    return result.ok();
}

// ---- Build argument vectors (exposed for testing) ----

std::vector<std::string> ChipToolDriver::buildPairingArgs(
    uint64_t node_id, uint32_t setup_code,
    const std::string& device_ip, uint16_t port) const {
    std::vector<std::string> args;
    args.push_back("pairing");

    if (device_ip.empty()) {
        args.push_back("onnetwork");
    } else {
        args.push_back("ethernet");
    }

    args.push_back(std::to_string(node_id));
    args.push_back(std::to_string(setup_code));

    if (!device_ip.empty()) {
        args.push_back(device_ip);
        args.push_back(std::to_string(port));
    }

    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());
    return args;
}

std::vector<std::string> ChipToolDriver::buildReadArgs(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute) const {

    std::vector<std::string> args;
    auto cname = clusterName(cluster);
    auto aname = attributeReadName(cluster, attribute);

    if (!cname.empty() && !aname.empty()) {
        args.push_back(cname);
        args.push_back("read");
        args.push_back(aname);
    } else {
        // Fallback to read-by-id
        args.push_back("any");
        args.push_back("read-by-id");
        std::ostringstream c_hex, a_hex;
        c_hex << "0x" << std::hex << cluster;
        a_hex << "0x" << std::hex << attribute;
        args.push_back(c_hex.str());
        args.push_back(a_hex.str());
    }

    args.push_back(std::to_string(node_id));
    args.push_back(std::to_string(endpoint));

    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());
    return args;
}

std::vector<std::string> ChipToolDriver::buildWriteArgs(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute,
    const std::string& value_str) const {

    std::vector<std::string> args;
    auto cname = clusterName(cluster);
    auto aname = attributeReadName(cluster, attribute);

    if (!cname.empty() && !aname.empty()) {
        args.push_back(cname);
        args.push_back("write");
        args.push_back(aname);
        args.push_back(value_str);
    } else {
        args.push_back("any");
        args.push_back("write-by-id");
        std::ostringstream c_hex, a_hex;
        c_hex << "0x" << std::hex << cluster;
        a_hex << "0x" << std::hex << attribute;
        args.push_back(c_hex.str());
        args.push_back(a_hex.str());
        args.push_back(value_str);
    }

    args.push_back(std::to_string(node_id));
    args.push_back(std::to_string(endpoint));

    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());
    return args;
}

std::vector<std::string> ChipToolDriver::buildSubscribeArgs(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute,
    Duration min_interval, Duration max_interval) const {

    std::vector<std::string> args;
    auto cname = clusterName(cluster);
    auto aname = attributeReadName(cluster, attribute);

    if (!cname.empty() && !aname.empty()) {
        args.push_back(cname);
        args.push_back("subscribe");
        args.push_back(aname);
    } else {
        args.push_back("any");
        args.push_back("subscribe-by-id");
        std::ostringstream c_hex, a_hex;
        c_hex << "0x" << std::hex << cluster;
        a_hex << "0x" << std::hex << attribute;
        args.push_back(c_hex.str());
        args.push_back(a_hex.str());
    }

    // chip-tool expects min/max in seconds
    auto min_sec = std::max(int64_t{1}, min_interval.count() / 1000);
    auto max_sec = std::max(min_sec, max_interval.count() / 1000);
    args.push_back(std::to_string(min_sec));
    args.push_back(std::to_string(max_sec));
    args.push_back(std::to_string(node_id));
    args.push_back(std::to_string(endpoint));

    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());
    return args;
}

std::vector<std::string> ChipToolDriver::buildInvokeArgs(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, CommandId command,
    const std::string& payload) const {

    std::vector<std::string> args;
    auto cname = clusterName(cluster);
    auto cmd_name = commandName(cluster, command);

    if (!cname.empty() && !cmd_name.empty()) {
        args.push_back(cname);
        args.push_back(cmd_name);
    } else {
        args.push_back("any");
        args.push_back("command-by-id");
        std::ostringstream c_hex, cmd_hex;
        c_hex << "0x" << std::hex << cluster;
        cmd_hex << "0x" << std::hex << command;
        args.push_back(c_hex.str());
        args.push_back(cmd_hex.str());
    }

    args.push_back(std::to_string(node_id));
    args.push_back(std::to_string(endpoint));

    if (!payload.empty()) {
        args.push_back(payload);
    }

    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());
    return args;
}

// ---- Public API ----

Result<std::string> ChipToolDriver::version() {
    auto result = runCommand({"--version"});
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    if (out.exit_code != 0 && out.stdout_data.empty()) {
        // Some chip-tool versions don't support --version, try --help
        auto help_result = runCommand({"--help"});
        if (!help_result.ok()) return Error("chip-tool not found: " + help_result.error().message);
        return std::string("unknown (--version not supported)");
    }
    std::string ver = out.stdout_data;
    // Trim trailing newline
    while (!ver.empty() && (ver.back() == '\n' || ver.back() == '\r')) ver.pop_back();
    return ver;
}

Result<PairingResult> ChipToolDriver::pairOnNetwork(
    uint64_t node_id, uint32_t setup_code,
    const std::string& device_ip, uint16_t port) {

    auto args = buildPairingArgs(node_id, setup_code, device_ip, port);
    MT_INFO("ChipToolDriver", "pairing onnetwork node " + std::to_string(node_id) +
            " code " + std::to_string(setup_code));

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    return ChipToolOutputParser::parsePairingOutput(out.stdout_data, out.stderr_data, out.exit_code);
}

Result<PairingResult> ChipToolDriver::pairBLE(
    uint64_t node_id, uint32_t setup_code, uint16_t discriminator,
    const std::string& thread_dataset) {

    std::vector<std::string> args;
    args.push_back("pairing");
    args.push_back("ble-thread");
    args.push_back(std::to_string(node_id));
    if (!thread_dataset.empty()) {
        args.push_back("hex:" + thread_dataset);
    }
    args.push_back(std::to_string(setup_code));
    args.push_back(std::to_string(discriminator));
    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());

    MT_INFO("ChipToolDriver", "pairing BLE node " + std::to_string(node_id) +
            " disc " + std::to_string(discriminator));

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    return ChipToolOutputParser::parsePairingOutput(out.stdout_data, out.stderr_data, out.exit_code);
}

Result<PairingResult> ChipToolDriver::establishCASE(uint64_t node_id) {
    // CASE session is established implicitly when performing operations
    // on an already-commissioned node. We use a read of BasicInfo to trigger it.
    auto args = buildReadArgs(node_id, 0, 0x0028, 0x0000); // BasicInfo:DataModelRevision
    MT_INFO("ChipToolDriver", "establishing CASE session to node " + std::to_string(node_id));

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);

    PairingResult pr;
    pr.node_id = node_id;
    pr.session_type = SessionType::CASE;
    pr.success = (result->exit_code == 0);
    if (!pr.success) {
        pr.error_message = "Failed to establish CASE session";
    }
    return pr;
}

Result<void> ChipToolDriver::unpair(uint64_t node_id) {
    std::vector<std::string> args;
    args.push_back("pairing");
    args.push_back("unpair");
    args.push_back(std::to_string(node_id));
    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    if (result->exit_code != 0) {
        return Error("Unpair failed with exit code " + std::to_string(result->exit_code));
    }
    return Result<void>::success();
}

Result<ReadAttributeResult> ChipToolDriver::readAttribute(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute) {

    auto args = buildReadArgs(node_id, endpoint, cluster, attribute);
    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    auto parsed = ChipToolOutputParser::parseReadOutput(out.stdout_data, out.stderr_data, out.exit_code);
    if (parsed.ok()) {
        parsed->path = {endpoint, cluster, attribute};
    }
    return parsed;
}

Result<WriteAttributeResult> ChipToolDriver::writeAttribute(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute,
    const std::string& value_str) {

    auto args = buildWriteArgs(node_id, endpoint, cluster, attribute, value_str);
    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    return ChipToolOutputParser::parseWriteOutput(out.stdout_data, out.stderr_data, out.exit_code);
}

Result<InvokeResult> ChipToolDriver::invokeCommand(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, CommandId command,
    const std::string& payload) {

    auto args = buildInvokeArgs(node_id, endpoint, cluster, command, payload);
    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    return ChipToolOutputParser::parseInvokeOutput(out.stdout_data, out.stderr_data, out.exit_code);
}

Result<InvokeResult> ChipToolDriver::timedInvoke(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, CommandId command,
    Duration timed_interaction_timeout,
    const std::string& payload) {

    auto args = buildInvokeArgs(node_id, endpoint, cluster, command, payload);
    args.push_back("--timedInteractionTimeoutMs");
    args.push_back(std::to_string(timed_interaction_timeout.count()));

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    auto& out = *result;
    return ChipToolOutputParser::parseInvokeOutput(out.stdout_data, out.stderr_data, out.exit_code);
}

Result<SubscriptionId> ChipToolDriver::subscribe(
    uint64_t node_id, EndpointId endpoint,
    ClusterId cluster, AttributeId attribute,
    Duration min_interval, Duration max_interval,
    std::function<void(const AttributeValue&)> on_report) {

    auto args = buildSubscribeArgs(node_id, endpoint, cluster, attribute, min_interval, max_interval);

    auto handle = std::make_unique<SubscriptionHandle>();
    handle->id = next_sub_id_++;
    handle->path = {endpoint, cluster, attribute};
    handle->on_report = std::move(on_report);

    auto cmd = buildCommand(args);
    cmd.timeout = Duration(0); // No timeout for long-running subscriptions
    auto start_result = handle->process.start(cmd);
    if (!start_result.ok()) {
        return Error("Failed to start subscribe process: " + start_result.error().message);
    }

    SubscriptionId id = handle->id;
    subscriptions_[id] = std::move(handle);

    MT_INFO("ChipToolDriver", "subscribed node " + std::to_string(node_id) +
            " ep " + std::to_string(endpoint) + " -> sub " + std::to_string(id));
    return id;
}

void ChipToolDriver::cancelSubscription(SubscriptionId id) {
    auto it = subscriptions_.find(id);
    if (it != subscriptions_.end()) {
        it->second->process.kill();
        subscriptions_.erase(it);
        MT_INFO("ChipToolDriver", "cancelled subscription " + std::to_string(id));
    }
}

void ChipToolDriver::tick() {
    std::vector<SubscriptionId> to_remove;

    for (auto& [id, handle] : subscriptions_) {
        auto result = handle->process.poll();
        if (result.has_value()) {
            // Process ended — subscription terminated
            if (handle->on_error) {
                handle->on_error(Error("Subscription process exited: code " +
                    std::to_string(result->exit_code)));
            }
            to_remove.push_back(id);
        }
        // TODO: For active subscriptions, read partial stdout and parse
        // new report lines. This requires a streaming read approach that
        // would need non-blocking incremental stdout reading.
    }

    for (auto id : to_remove) {
        subscriptions_.erase(id);
    }
}

size_t ChipToolDriver::activeSubscriptionCount() const {
    return subscriptions_.size();
}

Result<std::vector<uint8_t>> ChipToolDriver::readFabrics(uint64_t node_id) {
    auto args = buildReadArgs(node_id, 0, 0x003E, 0x0001); // OperationalCredentials:Fabrics
    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    // Return raw output for now — fabric parsing is complex
    return std::vector<uint8_t>(result->stdout_data.begin(), result->stdout_data.end());
}

Result<void> ChipToolDriver::removeFabric(uint64_t node_id, FabricIndex index) {
    std::vector<std::string> args;
    args.push_back("operationalcredentials");
    args.push_back("remove-fabric");
    args.push_back(std::to_string(node_id));
    args.push_back("0"); // endpoint
    args.push_back("--fabric-index");
    args.push_back(std::to_string(index));
    auto base = baseArgs();
    args.insert(args.end(), base.begin(), base.end());

    auto result = runCommand(args);
    if (!result.ok()) return Error(result.error().message);
    if (result->exit_code != 0) {
        return Error("Remove fabric failed: exit code " + std::to_string(result->exit_code));
    }
    return Result<void>::success();
}

} // namespace mt::hw
