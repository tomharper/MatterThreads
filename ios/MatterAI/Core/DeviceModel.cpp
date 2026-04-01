#include "DeviceModel.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>

namespace matter {

// --- AttributeValue ---

AttributeValue AttributeValue::fromBool(bool v) {
    AttributeValue av;
    av.type = Type::Bool;
    av.boolVal = v;
    return av;
}

AttributeValue AttributeValue::fromInt(int64_t v) {
    AttributeValue av;
    av.type = Type::Int;
    av.intVal = v;
    return av;
}

AttributeValue AttributeValue::fromFloat(float v) {
    AttributeValue av;
    av.type = Type::Float;
    av.floatVal = v;
    return av;
}

AttributeValue AttributeValue::fromString(const std::string& v) {
    AttributeValue av;
    av.type = Type::String;
    av.strVal = v;
    return av;
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
    }
    return "unknown";
}

// --- Device ---

bool Device::isOn() const {
    for (const auto& ep : endpoints) {
        uint32_t key = (static_cast<uint32_t>(ClusterId::OnOff) << 16) |
                       static_cast<uint32_t>(AttributeId::OnOff);
        auto it = ep.attributes.find(key);
        if (it != ep.attributes.end() && it->second.type == AttributeValue::Type::Bool) {
            return it->second.boolVal;
        }
    }
    return false;
}

std::optional<float> Device::temperature() const {
    for (const auto& ep : endpoints) {
        uint32_t key = (static_cast<uint32_t>(ClusterId::TemperatureMeas) << 16) |
                       static_cast<uint32_t>(AttributeId::MeasuredValue);
        auto it = ep.attributes.find(key);
        if (it != ep.attributes.end() && it->second.type == AttributeValue::Type::Float) {
            return it->second.floatVal;
        }
    }
    return std::nullopt;
}

std::optional<uint8_t> Device::level() const {
    for (const auto& ep : endpoints) {
        uint32_t key = (static_cast<uint32_t>(ClusterId::LevelControl) << 16) |
                       static_cast<uint32_t>(AttributeId::CurrentLevel);
        auto it = ep.attributes.find(key);
        if (it != ep.attributes.end() && it->second.type == AttributeValue::Type::Int) {
            return static_cast<uint8_t>(it->second.intVal);
        }
    }
    return std::nullopt;
}

std::string Device::stateDescription() const {
    std::ostringstream oss;
    oss << name;
    if (!reachable) {
        oss << " (unreachable)";
        return oss.str();
    }

    bool first = true;
    for (const auto& ep : endpoints) {
        for (const auto& [key, val] : ep.attributes) {
            if (first) { oss << ": "; first = false; }
            else { oss << ", "; }
            oss << val.describe();
        }
    }
    return oss.str();
}

// --- DeviceManager ---

uint32_t DeviceManager::attrKey(ClusterId c, AttributeId a) {
    return (static_cast<uint32_t>(c) << 16) | static_cast<uint32_t>(a);
}

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
    for (auto& d : devices_) {
        if (d.nodeId == nodeId) return &d;
    }
    return nullptr;
}

const Device* DeviceManager::findDevice(uint64_t nodeId) const {
    for (const auto& d : devices_) {
        if (d.nodeId == nodeId) return &d;
    }
    return nullptr;
}

std::vector<Device*> DeviceManager::devicesInRoom(const std::string& room) {
    std::vector<Device*> result;
    for (auto& d : devices_) {
        if (d.room == room) result.push_back(&d);
    }
    return result;
}

std::vector<Device*> DeviceManager::allDevices() {
    std::vector<Device*> result;
    for (auto& d : devices_) {
        result.push_back(&d);
    }
    return result;
}

bool DeviceManager::updateAttribute(uint64_t nodeId, uint16_t endpointId,
                                     ClusterId cluster, AttributeId attr,
                                     AttributeValue value) {
    Device* dev = findDevice(nodeId);
    if (!dev) return false;

    for (auto& ep : dev->endpoints) {
        if (ep.id == endpointId) {
            ep.attributes[attrKey(cluster, attr)] = std::move(value);
            dev->lastSeen = std::chrono::system_clock::now();
            return true;
        }
    }
    return false;
}

std::vector<std::string> DeviceManager::roomNames() const {
    std::set<std::string> rooms;
    for (const auto& d : devices_) {
        if (!d.room.empty()) rooms.insert(d.room);
    }
    return {rooms.begin(), rooms.end()};
}

std::string DeviceManager::roomSummary(const std::string& room) const {
    std::ostringstream oss;
    oss << room << ":\n";
    for (const auto& d : devices_) {
        if (d.room == room) {
            oss << "  " << d.stateDescription() << "\n";
        }
    }
    return oss.str();
}

std::string DeviceManager::homeSummary() const {
    std::ostringstream oss;
    auto rooms = roomNames();
    for (const auto& room : rooms) {
        oss << roomSummary(room);
    }

    // List unreachable devices
    bool hasUnreachable = false;
    for (const auto& d : devices_) {
        if (!d.reachable) {
            if (!hasUnreachable) {
                oss << "\nUnreachable devices:\n";
                hasUnreachable = true;
            }
            oss << "  " << d.name << " (" << d.room << ")\n";
        }
    }
    return oss.str();
}

} // namespace matter
