#include "DeviceModel.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>

namespace matter {

// ============================================================================
// AttributeValue
// ============================================================================

AttributeValue AttributeValue::fromBool(bool v) {
    AttributeValue av; av.type = Type::Bool; av.boolVal = v; return av;
}
AttributeValue AttributeValue::fromInt(int64_t v) {
    AttributeValue av; av.type = Type::Int; av.intVal = v; return av;
}
AttributeValue AttributeValue::fromFloat(float v) {
    AttributeValue av; av.type = Type::Float; av.floatVal = v; return av;
}
AttributeValue AttributeValue::fromString(const std::string& v) {
    AttributeValue av; av.type = Type::String; av.strVal = v; return av;
}

std::string AttributeValue::describe() const {
    switch (type) {
        case Type::Bool:   return boolVal ? "on" : "off";
        case Type::Int:    return std::to_string(intVal);
        case Type::Float: {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << floatVal;
            return oss.str();
        }
        case Type::String: return strVal;
        case Type::Bytes:  return "[" + std::to_string(bytesVal.size()) + " bytes]";
    }
    return "?";
}

// ============================================================================
// Endpoint
// ============================================================================

uint32_t Endpoint::key(ClusterId c, uint32_t a) {
    return (static_cast<uint32_t>(c) << 16) | (a & 0xFFFF);
}

bool Endpoint::hasCluster(ClusterId cluster) const {
    uint32_t prefix = static_cast<uint32_t>(cluster) << 16;
    for (const auto& [k, _] : attributes) {
        if ((k & 0xFFFF0000) == prefix) return true;
    }
    return false;
}

AttributeValue* Endpoint::getAttribute(ClusterId cluster, uint32_t attrId) {
    auto it = attributes.find(key(cluster, attrId));
    return (it != attributes.end()) ? &it->second : nullptr;
}

const AttributeValue* Endpoint::getAttribute(ClusterId cluster, uint32_t attrId) const {
    auto it = attributes.find(key(cluster, attrId));
    return (it != attributes.end()) ? &it->second : nullptr;
}

void Endpoint::setAttribute(ClusterId cluster, uint32_t attrId, AttributeValue val) {
    attributes[key(cluster, attrId)] = std::move(val);
}

// ============================================================================
// Device — state queries
// ============================================================================

Endpoint* Device::appEndpoint(uint16_t epId) {
    if (epId > 0) {
        for (auto& ep : endpoints) { if (ep.id == epId) return &ep; }
        return nullptr;
    }
    for (auto& ep : endpoints) { if (ep.id > 0) return &ep; }
    return nullptr;
}

const Endpoint* Device::appEndpoint(uint16_t epId) const {
    if (epId > 0) {
        for (const auto& ep : endpoints) { if (ep.id == epId) return &ep; }
        return nullptr;
    }
    for (const auto& ep : endpoints) { if (ep.id > 0) return &ep; }
    return nullptr;
}

Endpoint* Device::rootEndpoint() {
    for (auto& ep : endpoints) { if (ep.id == 0) return &ep; }
    return nullptr;
}

bool Device::isOn() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::OnOff, attr::OnOff)) {
            if (v->type == AttributeValue::Type::Bool) return v->boolVal;
        }
    }
    return false;
}

std::optional<float> Device::temperature() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::TemperatureMeas, attr::MeasuredValue)) {
            if (v->type == AttributeValue::Type::Float) return v->floatVal;
        }
        if (auto* v = ep.getAttribute(ClusterId::Thermostat, attr::LocalTemperature)) {
            if (v->type == AttributeValue::Type::Float) return v->floatVal;
        }
    }
    return std::nullopt;
}

std::optional<float> Device::humidity() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::HumidityMeas, attr::MeasuredValue)) {
            if (v->type == AttributeValue::Type::Float) return v->floatVal;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> Device::level() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::LevelControl, attr::CurrentLevel)) {
            if (v->type == AttributeValue::Type::Int) return static_cast<uint8_t>(v->intVal);
        }
    }
    return std::nullopt;
}

std::optional<uint16_t> Device::colorTempMireds() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::ColorControl, attr::ColorTemperatureMireds)) {
            if (v->type == AttributeValue::Type::Int) return static_cast<uint16_t>(v->intVal);
        }
    }
    return std::nullopt;
}

std::optional<LockStateEnum> Device::lockState() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::DoorLock, attr::LockState)) {
            if (v->type == AttributeValue::Type::Int)
                return static_cast<LockStateEnum>(v->intVal);
        }
    }
    return std::nullopt;
}

std::optional<ThermostatMode> Device::thermostatMode() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::Thermostat, attr::SystemMode)) {
            if (v->type == AttributeValue::Type::Int)
                return static_cast<ThermostatMode>(v->intVal);
        }
    }
    return std::nullopt;
}

std::optional<float> Device::thermostatSetpoint() const {
    for (const auto& ep : endpoints) {
        auto mode = thermostatMode();
        if (mode == ThermostatMode::Heat || mode == ThermostatMode::Auto) {
            if (auto* v = ep.getAttribute(ClusterId::Thermostat, attr::OccupiedHeatingSetpoint))
                if (v->type == AttributeValue::Type::Float) return v->floatVal;
        }
        if (mode == ThermostatMode::Cool) {
            if (auto* v = ep.getAttribute(ClusterId::Thermostat, attr::OccupiedCoolingSetpoint))
                if (v->type == AttributeValue::Type::Float) return v->floatVal;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> Device::fanSpeed() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::FanControl, attr::PercentCurrent))
            if (v->type == AttributeValue::Type::Int) return static_cast<uint8_t>(v->intVal);
    }
    return std::nullopt;
}

std::optional<uint16_t> Device::coverPosition() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::WindowCovering, attr::CurrentPositionLiftPercent100ths))
            if (v->type == AttributeValue::Type::Int) return static_cast<uint16_t>(v->intVal);
    }
    return std::nullopt;
}

std::optional<uint8_t> Device::batteryPercent() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::PowerSource, attr::BatPercentRemaining))
            if (v->type == AttributeValue::Type::Int) return static_cast<uint8_t>(v->intVal);
    }
    return std::nullopt;
}

std::optional<bool> Device::contactOpen() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::BooleanState, attr::StateValue))
            if (v->type == AttributeValue::Type::Bool) return v->boolVal;
    }
    return std::nullopt;
}

std::optional<bool> Device::occupied() const {
    for (const auto& ep : endpoints) {
        if (auto* v = ep.getAttribute(ClusterId::OccupancySensing, attr::Occupancy))
            if (v->type == AttributeValue::Type::Bool) return v->boolVal;
    }
    return std::nullopt;
}

std::vector<ClusterId> Device::applicationClusters() const {
    std::set<uint32_t> seen;
    std::vector<ClusterId> result;
    for (const auto& ep : endpoints) {
        if (ep.id == 0) continue;
        for (const auto& [k, _] : ep.attributes) {
            uint32_t cid = k >> 16;
            if (seen.insert(cid).second) {
                result.push_back(static_cast<ClusterId>(cid));
            }
        }
    }
    return result;
}

std::string Device::stateDescription() const {
    std::ostringstream oss;
    oss << name;
    if (!reachable) { oss << " (unreachable)"; return oss.str(); }

    const auto* ep = appEndpoint();
    if (!ep) return oss.str();

    oss << ": ";

    // OnOff devices
    if (ep->hasCluster(ClusterId::OnOff)) {
        oss << (isOn() ? "on" : "off");
        if (auto l = level()) {
            oss << ", " << static_cast<int>(*l * 100 / 254) << "%";
        }
        if (auto ct = colorTempMireds()) {
            int kelvin = *ct > 0 ? 1000000 / *ct : 0;
            oss << ", " << kelvin << "K";
        }
    }

    // Thermostat
    if (auto mode = thermostatMode()) {
        const char* modeStr = "off";
        switch (*mode) {
            case ThermostatMode::Heat: modeStr = "heating"; break;
            case ThermostatMode::Cool: modeStr = "cooling"; break;
            case ThermostatMode::Auto: modeStr = "auto"; break;
            case ThermostatMode::FanOnly: modeStr = "fan only"; break;
            default: break;
        }
        oss << modeStr;
        if (auto t = temperature()) oss << ", " << std::fixed << std::setprecision(1) << *t << "\u00B0C";
        if (auto sp = thermostatSetpoint()) oss << " \u2192 " << std::fixed << std::setprecision(1) << *sp << "\u00B0C";
    }

    // Temperature sensor
    if (ep->hasCluster(ClusterId::TemperatureMeas) && !ep->hasCluster(ClusterId::Thermostat)) {
        if (auto t = temperature()) oss << std::fixed << std::setprecision(1) << *t << "\u00B0C";
        if (auto h = humidity()) oss << ", " << std::fixed << std::setprecision(0) << *h << "% RH";
    }

    // Door lock
    if (auto ls = lockState()) {
        switch (*ls) {
            case LockStateEnum::Locked: oss << "locked"; break;
            case LockStateEnum::Unlocked: oss << "UNLOCKED"; break;
            case LockStateEnum::Unlatched: oss << "unlatched"; break;
            default: oss << "unknown"; break;
        }
    }

    // Fan
    if (auto fs = fanSpeed()) {
        oss << static_cast<int>(*fs) << "% speed";
    }

    // Window covering
    if (auto pos = coverPosition()) {
        oss << static_cast<int>(*pos / 100) << "% open";
    }

    // Contact sensor
    if (auto open = contactOpen()) {
        oss << (*open ? "open" : "closed");
    }

    // Occupancy
    if (auto occ = occupied()) {
        oss << (*occ ? "occupied" : "unoccupied");
    }

    // Battery
    if (auto bat = batteryPercent()) {
        oss << " [" << static_cast<int>(*bat / 2) << "% battery]";
    }

    return oss.str();
}

// ============================================================================
// DeviceManager
// ============================================================================

void DeviceManager::addDevice(Device device) {
    device.lastSeen = std::chrono::system_clock::now();
    devices_.push_back(std::move(device));
}

void DeviceManager::removeDevice(uint64_t nodeId) {
    devices_.erase(
        std::remove_if(devices_.begin(), devices_.end(),
                       [nodeId](const Device& d) { return d.nodeId == nodeId; }),
        devices_.end());
}

Device* DeviceManager::findDevice(uint64_t nodeId) {
    for (auto& d : devices_) { if (d.nodeId == nodeId) return &d; }
    return nullptr;
}

const Device* DeviceManager::findDevice(uint64_t nodeId) const {
    for (const auto& d : devices_) { if (d.nodeId == nodeId) return &d; }
    return nullptr;
}

std::vector<Device*> DeviceManager::devicesInRoom(const std::string& room) {
    std::vector<Device*> result;
    for (auto& d : devices_) { if (d.room == room) result.push_back(&d); }
    return result;
}

std::vector<Device*> DeviceManager::allDevices() {
    std::vector<Device*> result;
    for (auto& d : devices_) result.push_back(&d);
    return result;
}

bool DeviceManager::updateAttribute(uint64_t nodeId, uint16_t endpointId,
                                     ClusterId cluster, uint32_t attrId,
                                     AttributeValue value) {
    Device* dev = findDevice(nodeId);
    if (!dev) return false;
    for (auto& ep : dev->endpoints) {
        if (ep.id == endpointId) {
            ep.setAttribute(cluster, attrId, std::move(value));
            dev->lastSeen = std::chrono::system_clock::now();
            return true;
        }
    }
    return false;
}

std::vector<std::string> DeviceManager::roomNames() const {
    std::set<std::string> rooms;
    for (const auto& d : devices_) { if (!d.room.empty()) rooms.insert(d.room); }
    return {rooms.begin(), rooms.end()};
}

std::string DeviceManager::roomSummary(const std::string& room) const {
    std::ostringstream oss;
    oss << room << ":\n";
    for (const auto& d : devices_) {
        if (d.room == room) oss << "  " << d.stateDescription() << "\n";
    }
    return oss.str();
}

std::string DeviceManager::homeSummary() const {
    std::ostringstream oss;
    for (const auto& room : roomNames()) oss << roomSummary(room);

    bool hasUnreachable = false;
    for (const auto& d : devices_) {
        if (!d.reachable) {
            if (!hasUnreachable) { oss << "\nUnreachable:\n"; hasUnreachable = true; }
            oss << "  " << d.name << " (" << d.room << ")\n";
        }
    }
    return oss.str();
}

// ============================================================================
// Factory helpers
// ============================================================================

void DeviceManager::addRootEndpoint(Device& dev) {
    Endpoint ep0;
    ep0.id = 0;
    ep0.deviceType = DeviceType::RootNode;

    // BasicInformation cluster
    ep0.setAttribute(ClusterId::BasicInformation, attr::VendorName, AttributeValue::fromString(dev.vendorName));
    ep0.setAttribute(ClusterId::BasicInformation, attr::VendorID, AttributeValue::fromInt(dev.vendorId));
    ep0.setAttribute(ClusterId::BasicInformation, attr::ProductName, AttributeValue::fromString(dev.name));
    ep0.setAttribute(ClusterId::BasicInformation, attr::ProductID, AttributeValue::fromInt(dev.productId));
    ep0.setAttribute(ClusterId::BasicInformation, attr::NodeLabel, AttributeValue::fromString(dev.name));
    ep0.setAttribute(ClusterId::BasicInformation, attr::SoftwareVersion, AttributeValue::fromInt(1));
    ep0.setAttribute(ClusterId::BasicInformation, attr::SoftwareVersionString, AttributeValue::fromString(dev.firmwareVersion));
    ep0.setAttribute(ClusterId::BasicInformation, attr::SerialNumber, AttributeValue::fromString(dev.serialNumber));
    ep0.setAttribute(ClusterId::BasicInformation, attr::HardwareVersion, AttributeValue::fromInt(1));
    ep0.setAttribute(ClusterId::BasicInformation, attr::DataModelRevision, AttributeValue::fromInt(17));

    // Descriptor — PartsList lists the app endpoints
    std::string parts;
    for (const auto& e : dev.endpoints) { if (e.id > 0) parts += std::to_string(e.id) + ","; }
    ep0.setAttribute(ClusterId::Descriptor, attr::PartsList, AttributeValue::fromString(parts));

    // NetworkCommissioning
    ep0.setAttribute(ClusterId::NetworkCommissioning, 0x0000, AttributeValue::fromInt(1)); // MaxNetworks
    ep0.setAttribute(ClusterId::GeneralCommissioning, 0x0000, AttributeValue::fromInt(0)); // Breadcrumb

    dev.endpoints.insert(dev.endpoints.begin(), std::move(ep0));
}

Device DeviceManager::makeLight(uint64_t nodeId, const std::string& name,
                                 const std::string& room, DeviceType type,
                                 const std::string& vendor, bool isOn, uint8_t level,
                                 uint16_t colorTemp) {
    Device dev;
    dev.nodeId = nodeId;
    dev.name = name;
    dev.room = room;
    dev.vendorName = vendor;
    dev.vendorId = 0x1234;
    dev.productId = static_cast<uint16_t>(type);
    dev.serialNumber = "SN-" + std::to_string(nodeId);
    dev.firmwareVersion = "2.1.0";

    Endpoint ep;
    ep.id = 1;
    ep.deviceType = type;

    // OnOff cluster — full attribute set
    ep.setAttribute(ClusterId::OnOff, attr::OnOff, AttributeValue::fromBool(isOn));
    ep.setAttribute(ClusterId::OnOff, attr::GlobalSceneControl, AttributeValue::fromBool(true));
    ep.setAttribute(ClusterId::OnOff, attr::OnTime, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::OnOff, attr::OffWaitTime, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::OnOff, attr::StartUpOnOff, AttributeValue::fromInt(0xFF)); // Previous

    if (type != DeviceType::OnOffLight) {
        // LevelControl
        ep.setAttribute(ClusterId::LevelControl, attr::CurrentLevel, AttributeValue::fromInt(level));
        ep.setAttribute(ClusterId::LevelControl, attr::MinLevel, AttributeValue::fromInt(1));
        ep.setAttribute(ClusterId::LevelControl, attr::MaxLevel, AttributeValue::fromInt(254));
        ep.setAttribute(ClusterId::LevelControl, attr::RemainingTime, AttributeValue::fromInt(0));
        ep.setAttribute(ClusterId::LevelControl, attr::OnOffTransitionTime, AttributeValue::fromInt(5));
        ep.setAttribute(ClusterId::LevelControl, attr::OnLevel, AttributeValue::fromInt(254));
        ep.setAttribute(ClusterId::LevelControl, attr::StartUpCurrentLevel, AttributeValue::fromInt(254));
    }

    if (type == DeviceType::ColorTempLight || type == DeviceType::ExtColorLight) {
        // ColorControl
        ep.setAttribute(ClusterId::ColorControl, attr::ColorMode, AttributeValue::fromInt(2)); // ColorTemp
        ep.setAttribute(ClusterId::ColorControl, attr::ColorTemperatureMireds, AttributeValue::fromInt(colorTemp > 0 ? colorTemp : 370));
        ep.setAttribute(ClusterId::ColorControl, attr::ColorTempPhysicalMin, AttributeValue::fromInt(153));   // 6535K
        ep.setAttribute(ClusterId::ColorControl, attr::ColorTempPhysicalMax, AttributeValue::fromInt(500));   // 2000K
        ep.setAttribute(ClusterId::ColorControl, attr::ColorCapabilities, AttributeValue::fromInt(0x10));     // CT only

        if (type == DeviceType::ExtColorLight) {
            ep.setAttribute(ClusterId::ColorControl, attr::CurrentHue, AttributeValue::fromInt(0));
            ep.setAttribute(ClusterId::ColorControl, attr::CurrentSaturation, AttributeValue::fromInt(0));
            ep.setAttribute(ClusterId::ColorControl, attr::CurrentX, AttributeValue::fromInt(24939));
            ep.setAttribute(ClusterId::ColorControl, attr::CurrentY, AttributeValue::fromInt(24701));
            ep.setAttribute(ClusterId::ColorControl, attr::EnhancedCurrentHue, AttributeValue::fromInt(0));
            ep.setAttribute(ClusterId::ColorControl, attr::ColorCapabilities, AttributeValue::fromInt(0x1F)); // All
        }
    }

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeTempHumiditySensor(uint64_t nodeId, const std::string& name,
                                              const std::string& room, const std::string& vendor,
                                              float temp, float humidity, uint8_t battery) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x1235; dev.productId = 0x0302;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "1.3.2";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::TempSensor;
    ep.setAttribute(ClusterId::TemperatureMeas, attr::MeasuredValue, AttributeValue::fromFloat(temp));
    ep.setAttribute(ClusterId::TemperatureMeas, attr::MinMeasuredValue, AttributeValue::fromFloat(-40.0f));
    ep.setAttribute(ClusterId::TemperatureMeas, attr::MaxMeasuredValue, AttributeValue::fromFloat(85.0f));
    ep.setAttribute(ClusterId::TemperatureMeas, attr::Tolerance, AttributeValue::fromFloat(0.5f));

    Endpoint ep2;
    ep2.id = 2; ep2.deviceType = DeviceType::HumiditySensor;
    ep2.setAttribute(ClusterId::HumidityMeas, attr::MeasuredValue, AttributeValue::fromFloat(humidity));
    ep2.setAttribute(ClusterId::HumidityMeas, attr::MinMeasuredValue, AttributeValue::fromFloat(0.0f));
    ep2.setAttribute(ClusterId::HumidityMeas, attr::MaxMeasuredValue, AttributeValue::fromFloat(100.0f));

    // Battery on both endpoints via PowerSource
    ep.setAttribute(ClusterId::PowerSource, attr::BatPercentRemaining, AttributeValue::fromInt(battery * 2));
    ep.setAttribute(ClusterId::PowerSource, attr::PSStatus, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::PowerSource, attr::PSDescription, AttributeValue::fromString("Battery"));

    dev.endpoints.push_back(std::move(ep));
    dev.endpoints.push_back(std::move(ep2));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeDoorLock(uint64_t nodeId, const std::string& name,
                                    const std::string& room, const std::string& vendor,
                                    LockStateEnum state, uint8_t battery) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x1236; dev.productId = 0x000A;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "3.0.1";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::DoorLock;
    ep.setAttribute(ClusterId::DoorLock, attr::LockState, AttributeValue::fromInt(static_cast<int>(state)));
    ep.setAttribute(ClusterId::DoorLock, attr::LockType, AttributeValue::fromInt(0)); // Deadbolt
    ep.setAttribute(ClusterId::DoorLock, attr::ActuatorEnabled, AttributeValue::fromBool(true));
    ep.setAttribute(ClusterId::DoorLock, attr::DoorState, AttributeValue::fromInt(0)); // Closed
    ep.setAttribute(ClusterId::DoorLock, attr::AutoRelockTime, AttributeValue::fromInt(30));
    ep.setAttribute(ClusterId::DoorLock, attr::OperatingMode, AttributeValue::fromInt(0)); // Normal
    ep.setAttribute(ClusterId::DoorLock, attr::NumberOfTotalUsers, AttributeValue::fromInt(10));
    ep.setAttribute(ClusterId::PowerSource, attr::BatPercentRemaining, AttributeValue::fromInt(battery * 2));
    ep.setAttribute(ClusterId::PowerSource, attr::PSDescription, AttributeValue::fromString("Battery"));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeThermostat(uint64_t nodeId, const std::string& name,
                                      const std::string& room, const std::string& vendor,
                                      float localTemp, float heatSetpoint, float coolSetpoint,
                                      ThermostatMode mode) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x1237; dev.productId = 0x0301;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "4.2.0";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::Thermostat;
    ep.setAttribute(ClusterId::Thermostat, attr::LocalTemperature, AttributeValue::fromFloat(localTemp));
    ep.setAttribute(ClusterId::Thermostat, attr::OccupiedHeatingSetpoint, AttributeValue::fromFloat(heatSetpoint));
    ep.setAttribute(ClusterId::Thermostat, attr::OccupiedCoolingSetpoint, AttributeValue::fromFloat(coolSetpoint));
    ep.setAttribute(ClusterId::Thermostat, attr::SystemMode, AttributeValue::fromInt(static_cast<int>(mode)));
    ep.setAttribute(ClusterId::Thermostat, attr::ThermostatRunningState, AttributeValue::fromInt(mode == ThermostatMode::Off ? 0 : 1));
    ep.setAttribute(ClusterId::Thermostat, attr::ControlSequenceOfOperation, AttributeValue::fromInt(4)); // CoolingAndHeating

    // Embedded temp sensor
    ep.setAttribute(ClusterId::TemperatureMeas, attr::MeasuredValue, AttributeValue::fromFloat(localTemp));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeContactSensor(uint64_t nodeId, const std::string& name,
                                         const std::string& room, const std::string& vendor,
                                         bool open, uint8_t battery) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x1238; dev.productId = 0x0015;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "1.1.0";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::ContactSensor;
    ep.setAttribute(ClusterId::BooleanState, attr::StateValue, AttributeValue::fromBool(open));
    ep.setAttribute(ClusterId::PowerSource, attr::BatPercentRemaining, AttributeValue::fromInt(battery * 2));
    ep.setAttribute(ClusterId::PowerSource, attr::PSDescription, AttributeValue::fromString("CR2032"));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeWindowCovering(uint64_t nodeId, const std::string& name,
                                          const std::string& room, const std::string& vendor,
                                          uint16_t positionPercent100ths) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x1239; dev.productId = 0x0202;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "2.0.0";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::WindowCovering;
    ep.setAttribute(ClusterId::WindowCovering, attr::WCType, AttributeValue::fromInt(0)); // Rollershade
    ep.setAttribute(ClusterId::WindowCovering, attr::CurrentPositionLiftPercent100ths, AttributeValue::fromInt(positionPercent100ths));
    ep.setAttribute(ClusterId::WindowCovering, attr::TargetPositionLiftPercent100ths, AttributeValue::fromInt(positionPercent100ths));
    ep.setAttribute(ClusterId::WindowCovering, attr::OperationalStatus, AttributeValue::fromInt(0)); // Stopped

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeFan(uint64_t nodeId, const std::string& name,
                               const std::string& room, const std::string& vendor,
                               FanModeEnum mode, uint8_t speedPercent) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x123A; dev.productId = 0x002B;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "1.5.0";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::Fan;
    ep.setAttribute(ClusterId::FanControl, attr::FanMode, AttributeValue::fromInt(static_cast<int>(mode)));
    ep.setAttribute(ClusterId::FanControl, attr::FanModeSequence, AttributeValue::fromInt(2)); // OffLowMedHigh
    ep.setAttribute(ClusterId::FanControl, attr::PercentSetting, AttributeValue::fromInt(speedPercent));
    ep.setAttribute(ClusterId::FanControl, attr::PercentCurrent, AttributeValue::fromInt(speedPercent));
    ep.setAttribute(ClusterId::FanControl, attr::SpeedMax, AttributeValue::fromInt(100));
    ep.setAttribute(ClusterId::FanControl, attr::SpeedSetting, AttributeValue::fromInt(speedPercent));
    ep.setAttribute(ClusterId::FanControl, attr::SpeedCurrent, AttributeValue::fromInt(speedPercent));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeOccupancySensor(uint64_t nodeId, const std::string& name,
                                           const std::string& room, const std::string& vendor,
                                           bool occupied, uint8_t battery) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x123B; dev.productId = 0x0107;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "1.2.1";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::OccupancySensor;
    ep.setAttribute(ClusterId::OccupancySensing, attr::Occupancy, AttributeValue::fromBool(occupied));
    ep.setAttribute(ClusterId::OccupancySensing, attr::OccupancySensorType, AttributeValue::fromInt(0)); // PIR
    ep.setAttribute(ClusterId::PowerSource, attr::BatPercentRemaining, AttributeValue::fromInt(battery * 2));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makePlug(uint64_t nodeId, const std::string& name,
                                const std::string& room, const std::string& vendor,
                                bool isOn) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x123C; dev.productId = 0x010A;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "1.0.3";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::OnOffPlug;
    ep.setAttribute(ClusterId::OnOff, attr::OnOff, AttributeValue::fromBool(isOn));
    ep.setAttribute(ClusterId::OnOff, attr::StartUpOnOff, AttributeValue::fromInt(0xFF));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

Device DeviceManager::makeSmokeCOAlarm(uint64_t nodeId, const std::string& name,
                                        const std::string& room, const std::string& vendor,
                                        uint8_t battery) {
    Device dev;
    dev.nodeId = nodeId; dev.name = name; dev.room = room;
    dev.vendorName = vendor; dev.vendorId = 0x123D; dev.productId = 0x0076;
    dev.serialNumber = "SN-" + std::to_string(nodeId); dev.firmwareVersion = "2.3.0";

    Endpoint ep;
    ep.id = 1; ep.deviceType = DeviceType::SmokeCOAlarm;
    ep.setAttribute(ClusterId::SmokeCOAlarm, attr::ExpressedState, AttributeValue::fromInt(0)); // Normal
    ep.setAttribute(ClusterId::SmokeCOAlarm, attr::SmokeState, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::SmokeCOAlarm, attr::COState, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::SmokeCOAlarm, attr::BatteryAlert, AttributeValue::fromInt(0));
    ep.setAttribute(ClusterId::SmokeCOAlarm, attr::TestInProgress, AttributeValue::fromBool(false));
    ep.setAttribute(ClusterId::PowerSource, attr::BatPercentRemaining, AttributeValue::fromInt(battery * 2));

    dev.endpoints.push_back(std::move(ep));
    addRootEndpoint(dev);
    return dev;
}

// ============================================================================
// Realistic home with ~24 devices
// ============================================================================

void DeviceManager::loadRealisticHome() {
    devices_.clear();

    // --- Living Room ---
    addDevice(makeLight(1, "Ceiling Light", "Living Room", DeviceType::ColorTempLight,
                        "Philips", true, 200, 370));
    addDevice(makeLight(2, "Floor Lamp", "Living Room", DeviceType::DimmableLight,
                        "IKEA", true, 127, 0));
    addDevice(makeLight(3, "TV Bias Light", "Living Room", DeviceType::ExtColorLight,
                        "Nanoleaf", false, 0, 250));
    addDevice(makeTempHumiditySensor(4, "Climate Sensor", "Living Room", "Eve", 21.5f, 45.0f, 85));
    addDevice(makeOccupancySensor(5, "Motion Sensor", "Living Room", "Aqara", true, 72));
    addDevice(makeWindowCovering(6, "Roller Blinds", "Living Room", "IKEA", 7500)); // 75% open
    addDevice(makePlug(7, "TV Plug", "Living Room", "TP-Link", true));

    // --- Kitchen ---
    addDevice(makeLight(8, "Kitchen Downlights", "Kitchen", DeviceType::ColorTempLight,
                        "Philips", true, 254, 320));
    addDevice(makeLight(9, "Under Cabinet LEDs", "Kitchen", DeviceType::DimmableLight,
                        "IKEA", true, 180, 0));
    addDevice(makePlug(10, "Coffee Machine", "Kitchen", "Eve", true));
    addDevice(makeTempHumiditySensor(11, "Kitchen Sensor", "Kitchen", "Aqara", 23.1f, 52.0f, 90));
    addDevice(makeContactSensor(12, "Fridge Door", "Kitchen", "Aqara", false, 68));
    addDevice(makeSmokeCOAlarm(13, "Smoke Alarm", "Kitchen", "Eve", 92));

    // --- Bedroom ---
    addDevice(makeLight(14, "Bedside Lamp L", "Bedroom", DeviceType::ColorTempLight,
                        "Philips", false, 0, 454));      // warm 2200K
    addDevice(makeLight(15, "Bedside Lamp R", "Bedroom", DeviceType::ColorTempLight,
                        "Philips", false, 0, 454));
    addDevice(makeFan(16, "Ceiling Fan", "Bedroom", "Big Ass Fans", FanModeEnum::Low, 30));
    addDevice(makeTempHumiditySensor(17, "Bedroom Sensor", "Bedroom", "Eve", 19.8f, 41.0f, 78));
    addDevice(makeWindowCovering(18, "Blackout Blinds", "Bedroom", "Lutron", 0)); // closed

    // --- Hallway / Entry ---
    addDevice(makeLight(19, "Porch Light", "Hallway", DeviceType::OnOffLight,
                        "Ring", true, 254, 0));
    addDevice(makeDoorLock(20, "Front Door Lock", "Hallway", "Yale",
                           LockStateEnum::Locked, 65));
    addDevice(makeContactSensor(21, "Front Door Sensor", "Hallway", "Aqara", false, 55));
    addDevice(makeSmokeCOAlarm(22, "Hallway Smoke/CO", "Hallway", "Google", 88));

    // --- Office ---
    addDevice(makeLight(23, "Desk Lamp", "Office", DeviceType::ColorTempLight,
                        "BenQ", true, 230, 280));
    addDevice(makePlug(24, "Monitor Plug", "Office", "TP-Link", true));

    // --- Whole Home ---
    addDevice(makeThermostat(25, "Thermostat", "Hallway", "Ecobee",
                             20.5f, 21.0f, 24.0f, ThermostatMode::Heat));

    // Mark one device as unreachable for realism
    if (auto* dev = findDevice(5)) dev->reachable = false;
}

} // namespace matter
