#pragma once

#include <cstdint>

namespace matter {

/// Identifies which platform SDK a device was discovered/commissioned through
enum class BackendSource : uint8_t {
    Local = 0,       // In-memory demo / simulation
    AppleMatter,     // Apple Matter.framework (MTRDeviceController)
    HomeKit,         // Apple HomeKit (HMHomeManager)
    GoogleHome,      // Google Home Device Access API
    Thread,          // OpenThread network diagnostics
};

/// String label for each backend
inline const char* backendSourceName(BackendSource src) {
    switch (src) {
        case BackendSource::Local:       return "Local";
        case BackendSource::AppleMatter: return "Apple Matter";
        case BackendSource::HomeKit:     return "HomeKit";
        case BackendSource::GoogleHome:  return "Google Home";
        case BackendSource::Thread:      return "Thread";
    }
    return "Unknown";
}

} // namespace matter
