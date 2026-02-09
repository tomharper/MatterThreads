#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include "matter/DataModel.h"
#include "matter/Session.h"
#include "matter/InteractionModel.h"
#include "hw/ChipToolDriver.h"
#include "hw/OTBRClient.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace mt::hw {

struct IHardwareNode {
    virtual ~IHardwareNode() = default;

    virtual uint64_t deviceId() const = 0;
    virtual std::string name() const = 0;

    virtual Result<void> commission(uint32_t setup_code) = 0;
    virtual Result<void> openCASESession() = 0;
    virtual bool isCommissioned() const = 0;

    virtual Result<AttributeValue> readAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr) = 0;
    virtual Result<void> writeAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        const AttributeValue& value) = 0;
    virtual Result<InvokeResponseData> invokeCommand(
        EndpointId ep, ClusterId cluster, CommandId cmd,
        const std::vector<uint8_t>& payload = {}) = 0;

    virtual Result<SubscriptionId> subscribe(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        Duration min_interval, Duration max_interval,
        std::function<void(const AttributeValue&)> on_report) = 0;
    virtual void cancelSubscription(SubscriptionId id) = 0;

    virtual void tick() = 0;
};

class HardwareNode : public IHardwareNode {
public:
    HardwareNode(uint64_t device_id, std::string name,
                  std::shared_ptr<ChipToolDriver> driver,
                  std::shared_ptr<OTBRClient> otbr = nullptr);

    uint64_t deviceId() const override { return device_id_; }
    std::string name() const override { return name_; }

    Result<void> commission(uint32_t setup_code) override;
    Result<void> openCASESession() override;
    bool isCommissioned() const override { return commissioned_; }

    Result<AttributeValue> readAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr) override;
    Result<void> writeAttribute(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        const AttributeValue& value) override;
    Result<InvokeResponseData> invokeCommand(
        EndpointId ep, ClusterId cluster, CommandId cmd,
        const std::vector<uint8_t>& payload) override;

    Result<SubscriptionId> subscribe(
        EndpointId ep, ClusterId cluster, AttributeId attr,
        Duration min_interval, Duration max_interval,
        std::function<void(const AttributeValue&)> on_report) override;
    void cancelSubscription(SubscriptionId id) override;

    void tick() override;

    Result<ThreadNetworkInfo> getThreadNetwork();
    Result<std::vector<ThreadDeviceInfo>> getNeighbors();

    ChipToolDriver& driver() { return *driver_; }
    OTBRClient* otbr() { return otbr_.get(); }

private:
    uint64_t device_id_;
    std::string name_;
    std::shared_ptr<ChipToolDriver> driver_;
    std::shared_ptr<OTBRClient> otbr_;
    bool commissioned_ = false;

    static std::string valueToString(const AttributeValue& value);
};

} // namespace mt::hw
