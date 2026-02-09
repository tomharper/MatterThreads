#pragma once

#include "core/Types.h"
#include <string>
#include <vector>
#include <chrono>

namespace mt::gateway {

using VanId = std::string;
using TenantId = uint32_t;

enum class VanState : uint8_t {
    Registered,     // Known but not yet commissioned
    Commissioning,  // PASE/commissioning in progress
    Online,         // CASE session active, subscriptions running
    Offline,        // Session lost, reconnecting
    Unreachable,    // Max reconnect attempts exceeded
    Decommissioned  // Removed from fleet
};

inline std::string vanStateToString(VanState s) {
    switch (s) {
        case VanState::Registered:     return "registered";
        case VanState::Commissioning:  return "commissioning";
        case VanState::Online:         return "online";
        case VanState::Offline:        return "offline";
        case VanState::Unreachable:    return "unreachable";
        case VanState::Decommissioned: return "decommissioned";
    }
    return "unknown";
}

struct VanRegistration {
    VanId van_id;
    uint64_t device_id = 0;
    std::string name;
    TenantId tenant_id = 0;
    FabricIndex fabric_index = 0;
    std::vector<EndpointId> endpoints;
    uint32_t setup_code = 20202021;
    std::string ip_address;
    VanState state = VanState::Registered;
    TimePoint registered_at{};
    TimePoint last_seen{};
};

struct GatewayConfig {
    uint16_t api_port = 8090;
    Duration keepalive_interval{60000};       // 60s
    Duration session_timeout{300000};         // 5min
    Duration reconnect_base{5000};            // 5s
    Duration reconnect_max{300000};           // 5min
    uint32_t max_reconnect_attempts = 10;
    size_t max_events_per_van = 10000;
    size_t max_total_events = 100000;
    size_t default_max_vans_per_tenant = 100;
};

} // namespace mt::gateway
