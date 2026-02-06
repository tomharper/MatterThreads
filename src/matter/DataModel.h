#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include <map>
#include <variant>
#include <string>
#include <vector>
#include <functional>

namespace mt {

// Well-known cluster IDs
namespace Clusters {
    static constexpr ClusterId OnOff           = 0x0006;
    static constexpr ClusterId LevelControl    = 0x0008;
    static constexpr ClusterId ColorControl    = 0x0300;
    static constexpr ClusterId DoorLock        = 0x0101;
    static constexpr ClusterId Thermostat      = 0x0201;
    static constexpr ClusterId Descriptor       = 0x001D;
    static constexpr ClusterId BasicInfo       = 0x0028;
}

// Well-known attribute IDs
namespace Attributes {
    // OnOff
    static constexpr AttributeId OnOff_OnOff      = 0x0000;
    // LevelControl
    static constexpr AttributeId Level_CurrentLevel = 0x0000;
    // Thermostat
    static constexpr AttributeId Thermo_LocalTemp  = 0x0000;
    static constexpr AttributeId Thermo_SetpointHeat = 0x0012;
    static constexpr AttributeId Thermo_SetpointCool = 0x0013;
}

struct AttributePath {
    EndpointId endpoint_id;
    ClusterId  cluster_id;
    AttributeId attribute_id;

    bool operator==(const AttributePath& other) const = default;
    bool operator<(const AttributePath& other) const {
        if (endpoint_id != other.endpoint_id) return endpoint_id < other.endpoint_id;
        if (cluster_id != other.cluster_id) return cluster_id < other.cluster_id;
        return attribute_id < other.attribute_id;
    }
};

struct CommandPath {
    EndpointId endpoint_id;
    ClusterId  cluster_id;
    CommandId  command_id;
};

using AttributeValue = std::variant<bool, int32_t, uint32_t, uint64_t, double, std::string, std::vector<uint8_t>>;

using AttributeChangeCallback = std::function<void(const AttributePath&, const AttributeValue&)>;

class DataModel {
    // Endpoint -> Cluster -> Attribute -> Value
    std::map<EndpointId, std::map<ClusterId, std::map<AttributeId, AttributeValue>>> store_;
    std::vector<AttributeChangeCallback> change_callbacks_;

public:
    void registerEndpoint(EndpointId ep, const std::vector<ClusterId>& clusters);
    void setAttribute(const AttributePath& path, AttributeValue value);

    Result<AttributeValue> readAttribute(const AttributePath& path) const;
    Result<void> writeAttribute(const AttributePath& path, const AttributeValue& value);

    bool hasEndpoint(EndpointId ep) const;
    bool hasCluster(EndpointId ep, ClusterId cluster) const;
    bool hasAttribute(const AttributePath& path) const;

    void onAttributeChange(AttributeChangeCallback cb) { change_callbacks_.push_back(std::move(cb)); }

    // Device type presets — populate typical clusters and initial attribute values
    static DataModel lightBulb();
    static DataModel doorLock();
    static DataModel thermostat();
};

} // namespace mt
