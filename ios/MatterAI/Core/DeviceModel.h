#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <variant>

namespace matter {

// ============================================================================
// Cluster IDs — Matter Application Cluster Specification
// ============================================================================
enum class ClusterId : uint32_t {
    // Utility clusters
    Descriptor              = 0x001D,
    Binding                 = 0x001E,
    AccessControl           = 0x001F,
    BasicInformation        = 0x0028,
    OTASoftwareUpdate       = 0x002A,
    LocalizationConfig      = 0x002B,
    PowerSourceConfig       = 0x002E,
    PowerSource             = 0x002F,
    NetworkCommissioning    = 0x0031,
    GeneralCommissioning    = 0x0030,
    GeneralDiagnostics      = 0x0033,
    WiFiDiagnostics         = 0x0036,
    ThreadDiagnostics       = 0x0035,
    AdminCommissioning      = 0x003C,
    OperationalCredentials  = 0x003E,
    GroupKeyManagement      = 0x003F,
    // Application clusters
    Identify                = 0x0003,
    Groups                  = 0x0004,
    OnOff                   = 0x0006,
    LevelControl            = 0x0008,
    BooleanState            = 0x0045,
    ModeSelect              = 0x0050,
    DoorLock                = 0x0101,
    WindowCovering          = 0x0102,
    PumpConfig              = 0x0200,
    Thermostat              = 0x0201,
    FanControl              = 0x0202,
    ThermostatUI            = 0x0204,
    ColorControl            = 0x0300,
    IlluminanceMeas         = 0x0400,
    TemperatureMeas         = 0x0402,
    PressureMeas            = 0x0403,
    FlowMeas                = 0x0404,
    HumidityMeas            = 0x0405,
    OccupancySensing        = 0x0406,
    ElectricalMeasurement   = 0x0B04,
    SmokeCOAlarm            = 0x005C,
    AirQuality              = 0x005B,
};

// ============================================================================
// Attribute IDs — per-cluster (the spec defines these per cluster)
// ============================================================================
namespace attr {
    // Descriptor (0x001D)
    constexpr uint32_t DeviceTypeList       = 0x0000;
    constexpr uint32_t ServerList            = 0x0001;
    constexpr uint32_t ClientList            = 0x0002;
    constexpr uint32_t PartsList             = 0x0003;

    // BasicInformation (0x0028)
    constexpr uint32_t DataModelRevision     = 0x0000;
    constexpr uint32_t VendorName            = 0x0001;
    constexpr uint32_t VendorID              = 0x0002;
    constexpr uint32_t ProductName           = 0x0003;
    constexpr uint32_t ProductID             = 0x0004;
    constexpr uint32_t NodeLabel             = 0x0005;
    constexpr uint32_t Location              = 0x0006;
    constexpr uint32_t HardwareVersion       = 0x0007;
    constexpr uint32_t HardwareVersionString = 0x0008;
    constexpr uint32_t SoftwareVersion       = 0x0009;
    constexpr uint32_t SoftwareVersionString = 0x000A;
    constexpr uint32_t SerialNumber          = 0x000F;
    constexpr uint32_t UniqueID              = 0x0012;

    // OnOff (0x0006)
    constexpr uint32_t OnOff                 = 0x0000;
    constexpr uint32_t GlobalSceneControl    = 0x4000;
    constexpr uint32_t OnTime                = 0x4001;
    constexpr uint32_t OffWaitTime           = 0x4002;
    constexpr uint32_t StartUpOnOff          = 0x4003;

    // LevelControl (0x0008)
    constexpr uint32_t CurrentLevel          = 0x0000;
    constexpr uint32_t RemainingTime         = 0x0001;
    constexpr uint32_t MinLevel              = 0x0002;
    constexpr uint32_t MaxLevel              = 0x0003;
    constexpr uint32_t OnOffTransitionTime   = 0x0010;
    constexpr uint32_t OnLevel               = 0x0011;
    constexpr uint32_t Options               = 0x000F;
    constexpr uint32_t StartUpCurrentLevel   = 0x4000;

    // ColorControl (0x0300)
    constexpr uint32_t CurrentHue            = 0x0000;
    constexpr uint32_t CurrentSaturation     = 0x0001;
    constexpr uint32_t CurrentX              = 0x0003;
    constexpr uint32_t CurrentY              = 0x0004;
    constexpr uint32_t ColorTemperatureMireds = 0x0007;
    constexpr uint32_t ColorMode             = 0x0008;
    constexpr uint32_t EnhancedCurrentHue    = 0x4000;
    constexpr uint32_t ColorCapabilities     = 0x400A;
    constexpr uint32_t ColorTempPhysicalMin  = 0x400B;
    constexpr uint32_t ColorTempPhysicalMax  = 0x400C;

    // TemperatureMeasurement (0x0402)
    constexpr uint32_t MeasuredValue         = 0x0000;
    constexpr uint32_t MinMeasuredValue      = 0x0001;
    constexpr uint32_t MaxMeasuredValue      = 0x0002;
    constexpr uint32_t Tolerance             = 0x0003;

    // HumidityMeasurement (0x0405)
    // same attr IDs as temperature: 0x0000-0x0003

    // OccupancySensing (0x0406)
    constexpr uint32_t Occupancy             = 0x0000;
    constexpr uint32_t OccupancySensorType   = 0x0001;

    // DoorLock (0x0101)
    constexpr uint32_t LockState             = 0x0000;
    constexpr uint32_t LockType              = 0x0001;
    constexpr uint32_t ActuatorEnabled       = 0x0002;
    constexpr uint32_t DoorState             = 0x0003;
    constexpr uint32_t NumberOfTotalUsers    = 0x0011;
    constexpr uint32_t AutoRelockTime        = 0x0023;
    constexpr uint32_t OperatingMode         = 0x0025;

    // Thermostat (0x0201)
    constexpr uint32_t LocalTemperature          = 0x0000;
    constexpr uint32_t OutdoorTemperature        = 0x0001;
    constexpr uint32_t OccupiedCoolingSetpoint   = 0x0011;
    constexpr uint32_t OccupiedHeatingSetpoint   = 0x0012;
    constexpr uint32_t SystemMode                = 0x001C;
    constexpr uint32_t ThermostatRunningState    = 0x0029;
    constexpr uint32_t ControlSequenceOfOperation = 0x001B;

    // FanControl (0x0202)
    constexpr uint32_t FanMode              = 0x0000;
    constexpr uint32_t FanModeSequence      = 0x0001;
    constexpr uint32_t PercentSetting       = 0x0002;
    constexpr uint32_t PercentCurrent       = 0x0003;
    constexpr uint32_t SpeedMax             = 0x0004;
    constexpr uint32_t SpeedSetting         = 0x0005;
    constexpr uint32_t SpeedCurrent         = 0x0006;

    // WindowCovering (0x0102)
    constexpr uint32_t WCType                        = 0x0000;
    constexpr uint32_t CurrentPositionLiftPercent100ths = 0x000E;
    constexpr uint32_t CurrentPositionTiltPercent100ths = 0x000F;
    constexpr uint32_t OperationalStatus             = 0x000A;
    constexpr uint32_t TargetPositionLiftPercent100ths  = 0x000B;
    constexpr uint32_t WCEndProductType              = 0x000D;

    // PowerSource (0x002F)
    constexpr uint32_t PSStatus             = 0x0000;
    constexpr uint32_t PSOrder              = 0x0001;
    constexpr uint32_t PSDescription        = 0x0002;
    constexpr uint32_t BatChargeLevel       = 0x000E;
    constexpr uint32_t BatPercentRemaining  = 0x000C;
    constexpr uint32_t BatVoltage           = 0x000B;

    // BooleanState (0x0045) — contact sensors
    constexpr uint32_t StateValue           = 0x0000;

    // SmokeCOAlarm (0x005C)
    constexpr uint32_t ExpressedState       = 0x0000;
    constexpr uint32_t SmokeState           = 0x0001;
    constexpr uint32_t COState              = 0x0002;
    constexpr uint32_t BatteryAlert         = 0x0003;
    constexpr uint32_t TestInProgress       = 0x0005;

    // IlluminanceMeasurement (0x0400)
    // same attr IDs as temperature: 0x0000-0x0003

    // AirQuality (0x005B)
    constexpr uint32_t AirQualityValue      = 0x0000;
}

// ============================================================================
// Device Types — Matter Device Library Specification
// ============================================================================
enum class DeviceType : uint32_t {
    RootNode            = 0x0016,
    PowerSource         = 0x0011,
    OnOffLight          = 0x0100,
    DimmableLight       = 0x0101,
    ColorTempLight      = 0x010C,
    ExtColorLight       = 0x010D,
    OnOffPlug           = 0x010A,
    DimmablePlug        = 0x010B,
    OnOffSwitch         = 0x0103,
    DimmerSwitch        = 0x0104,
    ColorDimmerSwitch   = 0x0105,
    TempSensor          = 0x0302,
    HumiditySensor      = 0x0307,
    OccupancySensor     = 0x0107,
    LightSensor         = 0x0106,
    ContactSensor       = 0x0015,
    DoorLock            = 0x000A,
    DoorLockController  = 0x000B,
    Thermostat          = 0x0301,
    Fan                 = 0x002B,
    WindowCovering      = 0x0202,
    SmokeCOAlarm        = 0x0076,
    AirQualitySensor    = 0x002C,
    AirPurifier         = 0x002D,
    RobotVacuum         = 0x0074,
    Pump                = 0x0303,
    WaterLeakDetector   = 0x0043,
    Speaker             = 0x0022,
};

// ============================================================================
// Lock states, thermostat modes, etc. — spec enums
// ============================================================================
enum class LockStateEnum : uint8_t {
    NotFullyLocked = 0, Locked = 1, Unlocked = 2, Unlatched = 3
};

enum class ThermostatMode : uint8_t {
    Off = 0, Auto = 1, Cool = 3, Heat = 4, EmergencyHeat = 5, FanOnly = 7
};

enum class FanModeEnum : uint8_t {
    Off = 0, Low = 1, Medium = 2, High = 3, On = 4, Auto = 5
};

enum class AirQualityEnum : uint8_t {
    Unknown = 0, Good = 1, Fair = 2, Moderate = 3, Poor = 4, VeryPoor = 5, ExtremelyPoor = 6
};

enum class SmokeStateEnum : uint8_t {
    Normal = 0, Warning = 1, Critical = 2
};

// ============================================================================
// Attribute value — type-erased container
// ============================================================================
struct AttributeValue {
    enum class Type { Bool, Int, Float, String, Bytes };
    Type type;
    bool boolVal = false;
    int64_t intVal = 0;
    float floatVal = 0.0f;
    std::string strVal;
    std::vector<uint8_t> bytesVal;

    static AttributeValue fromBool(bool v);
    static AttributeValue fromInt(int64_t v);
    static AttributeValue fromFloat(float v);
    static AttributeValue fromString(const std::string& v);

    std::string describe() const;
};

// ============================================================================
// Endpoint — a functional unit on a device
// ============================================================================
struct Endpoint {
    uint16_t id;
    DeviceType deviceType;
    // Attributes keyed by (clusterId << 16 | attrId)
    std::unordered_map<uint32_t, AttributeValue> attributes;

    // Helpers
    bool hasCluster(ClusterId cluster) const;
    AttributeValue* getAttribute(ClusterId cluster, uint32_t attrId);
    const AttributeValue* getAttribute(ClusterId cluster, uint32_t attrId) const;
    void setAttribute(ClusterId cluster, uint32_t attrId, AttributeValue val);
    static uint32_t key(ClusterId c, uint32_t a);
};

// ============================================================================
// Device — a commissioned Matter node
// ============================================================================
struct Device {
    uint64_t nodeId;
    std::string name;
    std::string room;
    std::string vendorName;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::string serialNumber;
    std::string firmwareVersion;
    std::vector<Endpoint> endpoints;
    bool reachable = true;
    uint64_t fabricIndex = 1;
    std::chrono::system_clock::time_point lastSeen;

    // Convenience: find the first application endpoint (id > 0)
    Endpoint* appEndpoint(uint16_t epId = 0);
    const Endpoint* appEndpoint(uint16_t epId = 0) const;
    Endpoint* rootEndpoint();

    // High-level state queries
    bool isOn() const;
    std::optional<float> temperature() const;
    std::optional<float> humidity() const;
    std::optional<uint8_t> level() const;
    std::optional<uint16_t> colorTempMireds() const;
    std::optional<LockStateEnum> lockState() const;
    std::optional<ThermostatMode> thermostatMode() const;
    std::optional<float> thermostatSetpoint() const;
    std::optional<uint8_t> fanSpeed() const;
    std::optional<uint16_t> coverPosition() const;
    std::optional<uint8_t> batteryPercent() const;
    std::optional<bool> contactOpen() const;
    std::optional<bool> occupied() const;

    std::string stateDescription() const;
    std::vector<ClusterId> applicationClusters() const;
};

// ============================================================================
// DeviceManager — manages all commissioned devices
// ============================================================================
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
                         ClusterId cluster, uint32_t attrId,
                         AttributeValue value);

    // State queries for AI
    std::string homeSummary() const;
    std::string roomSummary(const std::string& room) const;
    std::vector<std::string> roomNames() const;

    // Load a realistic demo home
    void loadRealisticHome();

private:
    std::vector<Device> devices_;

    // Helpers to create fully-populated devices
    static Device makeLight(uint64_t nodeId, const std::string& name,
                            const std::string& room, DeviceType type,
                            const std::string& vendor, bool isOn, uint8_t level,
                            uint16_t colorTemp = 0);
    static Device makeTempHumiditySensor(uint64_t nodeId, const std::string& name,
                                          const std::string& room, const std::string& vendor,
                                          float temp, float humidity, uint8_t battery);
    static Device makeDoorLock(uint64_t nodeId, const std::string& name,
                                const std::string& room, const std::string& vendor,
                                LockStateEnum state, uint8_t battery);
    static Device makeThermostat(uint64_t nodeId, const std::string& name,
                                  const std::string& room, const std::string& vendor,
                                  float localTemp, float heatSetpoint, float coolSetpoint,
                                  ThermostatMode mode);
    static Device makeContactSensor(uint64_t nodeId, const std::string& name,
                                     const std::string& room, const std::string& vendor,
                                     bool open, uint8_t battery);
    static Device makeWindowCovering(uint64_t nodeId, const std::string& name,
                                      const std::string& room, const std::string& vendor,
                                      uint16_t positionPercent100ths);
    static Device makeFan(uint64_t nodeId, const std::string& name,
                           const std::string& room, const std::string& vendor,
                           FanModeEnum mode, uint8_t speedPercent);
    static Device makeOccupancySensor(uint64_t nodeId, const std::string& name,
                                       const std::string& room, const std::string& vendor,
                                       bool occupied, uint8_t battery);
    static Device makePlug(uint64_t nodeId, const std::string& name,
                            const std::string& room, const std::string& vendor,
                            bool isOn);
    static Device makeSmokeCOAlarm(uint64_t nodeId, const std::string& name,
                                    const std::string& room, const std::string& vendor,
                                    uint8_t battery);

    // Populate endpoint 0 (root node) with standard clusters
    static void addRootEndpoint(Device& dev);
};

} // namespace matter
