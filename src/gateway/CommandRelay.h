#pragma once

#include "gateway/GatewayTypes.h"
#include "gateway/SessionPool.h"
#include "core/Result.h"
#include "hw/ChipToolDriver.h"
#include "hw/ChipToolOutputParser.h"

#include <memory>

namespace mt::gateway {

class CommandRelay {
public:
    CommandRelay(std::shared_ptr<hw::ChipToolDriver> driver,
                  CASESessionPool& session_pool);

    Result<hw::InvokeResult> invoke(const VanId& van_id, uint64_t device_id,
                                     EndpointId ep, ClusterId cluster,
                                     CommandId cmd,
                                     const std::string& payload = "");

    Result<hw::InvokeResult> timedInvoke(const VanId& van_id, uint64_t device_id,
                                          EndpointId ep, ClusterId cluster,
                                          CommandId cmd, Duration timeout,
                                          const std::string& payload = "");

    Result<hw::ReadAttributeResult> readAttribute(const VanId& van_id, uint64_t device_id,
                                                    EndpointId ep, ClusterId cluster,
                                                    AttributeId attr);

    Result<hw::WriteAttributeResult> writeAttribute(const VanId& van_id, uint64_t device_id,
                                                      EndpointId ep, ClusterId cluster,
                                                      AttributeId attr,
                                                      const std::string& value);

private:
    std::shared_ptr<hw::ChipToolDriver> driver_;
    CASESessionPool& session_pool_;

    Result<void> checkConnected(const VanId& van_id) const;
};

} // namespace mt::gateway
