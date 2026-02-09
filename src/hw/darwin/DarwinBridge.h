#pragma once

#include "hw/HardwareNode.h"

#include <string>
#include <memory>

#ifdef __APPLE__

namespace mt::hw::darwin {

struct DarwinControllerConfig {
    std::string storage_path;
    std::string vendor_id = "0xFFF1";  // Test vendor ID
    uint16_t fabric_id = 1;
};

// Factory: returns IHardwareNode backed by Darwin Framework (MTRDeviceController)
std::unique_ptr<IHardwareNode> createDarwinController(
    uint64_t device_id, const std::string& name,
    const DarwinControllerConfig& config = {});

// Check if Darwin Matter framework is available at runtime
bool isDarwinMatterAvailable();

} // namespace mt::hw::darwin

#endif // __APPLE__
