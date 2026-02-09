#include "gateway/CommandRelay.h"
#include "core/Log.h"

namespace mt::gateway {

CommandRelay::CommandRelay(std::shared_ptr<hw::ChipToolDriver> driver,
                            CASESessionPool& session_pool)
    : driver_(std::move(driver))
    , session_pool_(session_pool) {}

Result<hw::InvokeResult> CommandRelay::invoke(const VanId& van_id, uint64_t device_id,
                                               EndpointId ep, ClusterId cluster,
                                               CommandId cmd, const std::string& payload) {
    auto check = checkConnected(van_id);
    if (!check.ok()) {
        hw::InvokeResult fail;
        fail.success = false;
        fail.error_message = check.error().message;
        return fail;
    }

    MT_INFO("gateway", "Invoking command on van " + van_id +
            " ep=" + std::to_string(ep) + " cluster=" + std::to_string(cluster) +
            " cmd=" + std::to_string(cmd));

    auto result = driver_->invokeCommand(device_id, ep, cluster, cmd, payload);
    session_pool_.touchActivity(van_id, SteadyClock::now());
    return result;
}

Result<hw::InvokeResult> CommandRelay::timedInvoke(const VanId& van_id, uint64_t device_id,
                                                    EndpointId ep, ClusterId cluster,
                                                    CommandId cmd, Duration timeout,
                                                    const std::string& payload) {
    auto check = checkConnected(van_id);
    if (!check.ok()) {
        hw::InvokeResult fail;
        fail.success = false;
        fail.error_message = check.error().message;
        return fail;
    }

    MT_INFO("gateway", "Timed invoke on van " + van_id +
            " timeout=" + std::to_string(timeout.count()) + "ms");

    auto result = driver_->timedInvoke(device_id, ep, cluster, cmd, timeout, payload);
    session_pool_.touchActivity(van_id, SteadyClock::now());
    return result;
}

Result<hw::ReadAttributeResult> CommandRelay::readAttribute(const VanId& van_id,
                                                              uint64_t device_id,
                                                              EndpointId ep, ClusterId cluster,
                                                              AttributeId attr) {
    auto check = checkConnected(van_id);
    if (!check.ok()) {
        hw::ReadAttributeResult fail;
        fail.success = false;
        fail.error_message = check.error().message;
        return fail;
    }

    auto result = driver_->readAttribute(device_id, ep, cluster, attr);
    session_pool_.touchActivity(van_id, SteadyClock::now());
    return result;
}

Result<hw::WriteAttributeResult> CommandRelay::writeAttribute(const VanId& van_id,
                                                                uint64_t device_id,
                                                                EndpointId ep, ClusterId cluster,
                                                                AttributeId attr,
                                                                const std::string& value) {
    auto check = checkConnected(van_id);
    if (!check.ok()) {
        hw::WriteAttributeResult fail;
        fail.success = false;
        fail.error_message = check.error().message;
        return fail;
    }

    auto result = driver_->writeAttribute(device_id, ep, cluster, attr, value);
    session_pool_.touchActivity(van_id, SteadyClock::now());
    return result;
}

Result<void> CommandRelay::checkConnected(const VanId& van_id) const {
    if (!session_pool_.isConnected(van_id)) {
        auto state = session_pool_.sessionState(van_id);
        return Error{-1, "van not connected (" + sessionStateToString(state) + "): " + van_id};
    }
    return Result<void>::success();
}

} // namespace mt::gateway
