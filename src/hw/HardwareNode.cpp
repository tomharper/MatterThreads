#include "hw/HardwareNode.h"
#include "core/Log.h"

#include <sstream>

namespace mt::hw {

HardwareNode::HardwareNode(uint64_t device_id, std::string name,
                             std::shared_ptr<ChipToolDriver> driver,
                             std::shared_ptr<OTBRClient> otbr)
    : device_id_(device_id), name_(std::move(name)),
      driver_(std::move(driver)), otbr_(std::move(otbr)) {}

std::string HardwareNode::valueToString(const AttributeValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "1" : "0";
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + v + "\"";
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            std::ostringstream oss;
            oss << "hex:";
            for (auto b : v) {
                oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
            }
            return oss.str();
        } else {
            return "unknown";
        }
    }, value);
}

Result<void> HardwareNode::commission(uint32_t setup_code) {
    MT_INFO("HardwareNode", "commissioning device " + std::to_string(device_id_) +
            " with code " + std::to_string(setup_code));

    auto result = driver_->pairOnNetwork(device_id_, setup_code);
    if (!result.ok()) return Error(result.error().message);

    if (!result->success) {
        return Error("Commissioning failed: " + result->error_message);
    }

    commissioned_ = true;
    MT_INFO("HardwareNode", "commissioning succeeded for " + name_);
    return Result<void>::success();
}

Result<void> HardwareNode::openCASESession() {
    if (!commissioned_) {
        return Error("Cannot establish CASE session: device not commissioned");
    }

    auto result = driver_->establishCASE(device_id_);
    if (!result.ok()) return Error(result.error().message);

    if (!result->success) {
        return Error("CASE session failed: " + result->error_message);
    }

    return Result<void>::success();
}

Result<AttributeValue> HardwareNode::readAttribute(
    EndpointId ep, ClusterId cluster, AttributeId attr) {

    if (!commissioned_) {
        return Error("Device not commissioned");
    }

    auto result = driver_->readAttribute(device_id_, ep, cluster, attr);
    if (!result.ok()) return Error(result.error().message);

    if (!result->success) {
        return Error("Read failed: " + result->error_message);
    }

    return result->value;
}

Result<void> HardwareNode::writeAttribute(
    EndpointId ep, ClusterId cluster, AttributeId attr,
    const AttributeValue& value) {

    if (!commissioned_) {
        return Error("Device not commissioned");
    }

    auto str_val = valueToString(value);
    auto result = driver_->writeAttribute(device_id_, ep, cluster, attr, str_val);
    if (!result.ok()) return Error(result.error().message);

    if (!result->success) {
        return Error("Write failed: " + result->error_message);
    }

    return Result<void>::success();
}

Result<InvokeResponseData> HardwareNode::invokeCommand(
    EndpointId ep, ClusterId cluster, CommandId cmd,
    const std::vector<uint8_t>& payload) {

    if (!commissioned_) {
        return Error("Device not commissioned");
    }

    std::string payload_str;
    if (!payload.empty()) {
        std::ostringstream oss;
        oss << "hex:";
        for (auto b : payload) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        payload_str = oss.str();
    }

    auto result = driver_->invokeCommand(device_id_, ep, cluster, cmd, payload_str);
    if (!result.ok()) return Error(result.error().message);

    InvokeResponseData response;
    response.command = {ep, cluster, cmd};
    response.status_code = result->status_code;
    response.response_fields = result->response_data;
    return response;
}

Result<SubscriptionId> HardwareNode::subscribe(
    EndpointId ep, ClusterId cluster, AttributeId attr,
    Duration min_interval, Duration max_interval,
    std::function<void(const AttributeValue&)> on_report) {

    if (!commissioned_) {
        return Error("Device not commissioned");
    }

    return driver_->subscribe(device_id_, ep, cluster, attr,
                               min_interval, max_interval, std::move(on_report));
}

void HardwareNode::cancelSubscription(SubscriptionId id) {
    driver_->cancelSubscription(id);
}

void HardwareNode::tick() {
    driver_->tick();
}

Result<ThreadNetworkInfo> HardwareNode::getThreadNetwork() {
    if (!otbr_) return Error("No OTBR client configured");
    return otbr_->getActiveDataset();
}

Result<std::vector<ThreadDeviceInfo>> HardwareNode::getNeighbors() {
    if (!otbr_) return Error("No OTBR client configured");
    return otbr_->getNeighborTable();
}

} // namespace mt::hw
