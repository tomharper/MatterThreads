#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>

namespace matter {

// Matter cluster IDs (from the spec)
enum class ClusterId : uint32_t {
    OnOff           = 0x0006,
    LevelControl    = 0x0008,
    ColorControl    = 0x0300,
    TemperatureMeas = 0x0402,
    OccupancySensor = 0x0406,
    DoorLock        = 0x0101,
    Thermostat      = 0x0201,
};

// Matter attribute IDs
enum class AttributeId : uint32_t {
    OnOff               = 0x0000,
    CurrentLevel        = 0x0000,
    MeasuredValue       = 0x0000,
    Occupancy           = 0x0000,
    LockState           = 0x0000,
    LocalTemperature    = 0x0000,
    OccupiedHeatingSetpoint = 0x0012,
    OccupiedCoolingSetpoint = 0x0011,
};

// Device types
enum class DeviceType : uint32_t {
    OnOffLight      = 0x0100,
    DimmableLight   = 0x0101,
    ColorLight      = 0x010D,
    TempSensor      = 0x0302,
    OccupancySensor = 0x0107,
    DoorLock        = 0x000A,
    Thermostat      = 0x0301,
    ContactSensor   = 0x0015,
};

// Attribute value — type-erased container
struct AttributeValue {
    enum class Type { Bool, Int, Float, String };
    Type type;
    bool   boolVal = false;
    int64_t intVal = 0;
    float  floatVal = 0.0f;
    std::string strVal;

    static AttributeValue fromBool(bool v);
    static AttributeValue fromInt(int64_t v);
    static AttributeValue fromFloat(float v);
    static AttributeValue fromString(const std::string& v);

    std::string describe() const;
};

// An endpoint on a device (Matter devices can have multiple endpoints)
struct Endpoint {
    uint16_t id;
    DeviceType deviceType;
    std::unordered_map<uint32_t, AttributeValue> attributes; // keyed by clusterId<<16 | attrId
};

// Represents a commissioned Matter device
struct Device {
    uint64_t nodeId;
    std::string name;
    std::string room;
    std::string vendor;
    std::vector<Endpoint> endpoints;
    bool reachable = true;
    std::chrono::system_clock::time_point lastSeen;

    // Convenience accessors
    bool isOn() const;
    std::optional<float> temperature() const;
    std::optional<uint8_t> level() const;
    std::string stateDescription() const;
};

// Manages all known devices — the in-memory home model
class DeviceManager {
public:
    void addDevice(Device device);
    void removeDevice(uint64_t nodeId);
    Device* findDevice(uint64_t nodeId);
    const Device* findDevice(uint64_t nodeId) const;
    std::vector<Device*> devicesInRoom(const std::string& room);
    std::vector<Device*> allDevices();
    const std::vector<Device>& devices() const { return devices_; }

    // Attribute updates (from Matter subscriptions)
    bool updateAttribute(uint64_t nodeId, uint16_t endpointId,
                         ClusterId cluster, AttributeId attr,
                         AttributeValue value);

    // State queries for AI
    std::string homeSummary() const;
    std::string roomSummary(const std::string& room) const;
    std::vector<std::string> roomNames() const;

private:
    std::vector<Device> devices_;
    static uint32_t attrKey(ClusterId c, AttributeId a);
};

} // namespace matter
