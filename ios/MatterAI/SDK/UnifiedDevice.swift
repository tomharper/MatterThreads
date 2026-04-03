import Foundation

/// A device from any backend, normalized to a common interface.
/// This is the type the app layer works with — it doesn't care which SDK
/// the device came from.
struct UnifiedDevice: Identifiable, Sendable {
    /// Globally unique ID: "<backend>:<native_id>"
    let id: String

    /// Which backend owns this device
    let source: BackendSource

    /// Backend-specific device identifier (nodeId, accessoryId, etc.)
    let nativeId: String

    /// Human-readable name
    var name: String

    /// Room / location
    var room: String

    /// Vendor name
    var vendor: String

    /// Matter device type (0 if not Matter-based)
    var deviceType: UInt32

    /// Whether the device is reachable
    var reachable: Bool

    /// Cached attribute state — keyed by AttributePath
    var attributes: [AttributePath: SDKAttributeValue]

    /// Last time state was updated
    var lastUpdated: Date

    // MARK: - Convenience Accessors

    var isOn: Bool {
        attributes[.onOff]?.boolValue ?? false
    }

    var brightness: UInt8? {
        guard let v = attributes[.currentLevel]?.intValue else { return nil }
        return UInt8(clamping: v)
    }

    var temperature: Float? {
        guard let v = attributes[.measuredTemp]?.intValue else { return nil }
        return Float(v) / 100.0  // Matter uses centidegrees
    }

    var humidity: Float? {
        guard let v = attributes[.measuredHumidity]?.intValue else { return nil }
        return Float(v) / 100.0
    }

    var isLocked: Bool {
        guard let v = attributes[.lockState]?.intValue else { return false }
        return v == 1  // LockStateEnum::Locked
    }

    var batteryPercent: UInt8? {
        guard let v = attributes[.batteryRemaining]?.intValue else { return nil }
        return UInt8(clamping: v / 2)  // spec uses 200 = 100%
    }

    var stateDescription: String {
        // Build a description from available attributes
        var parts: [String] = []

        if let temp = temperature {
            parts.append(String(format: "%.1f°C", temp))
        }
        if let hum = humidity {
            parts.append(String(format: "%.0f%% RH", hum))
        }
        if let lvl = brightness {
            let pct = Int(Float(lvl) / 254.0 * 100)
            parts.append(isOn ? "\(pct)%" : "Off")
        } else if attributes[.onOff] != nil {
            parts.append(isOn ? "On" : "Off")
        }
        if attributes[.lockState] != nil {
            parts.append(isLocked ? "Locked" : "Unlocked")
        }
        if let bat = batteryPercent {
            parts.append("Battery: \(bat)%")
        }

        if parts.isEmpty { return reachable ? "Ready" : "Unreachable" }
        return parts.joined(separator: " · ")
    }

    /// Whether this device supports on/off toggle
    var hasToggle: Bool {
        attributes[.onOff] != nil
    }
}

// MARK: - Well-Known Attribute Paths

extension AttributePath {
    // OnOff
    static let onOff = AttributePath(endpointId: 1, clusterId: 0x0006, attributeId: 0x0000)

    // LevelControl
    static let currentLevel = AttributePath(endpointId: 1, clusterId: 0x0008, attributeId: 0x0000)

    // TemperatureMeasurement
    static let measuredTemp = AttributePath(endpointId: 1, clusterId: 0x0402, attributeId: 0x0000)

    // HumidityMeasurement
    static let measuredHumidity = AttributePath(endpointId: 1, clusterId: 0x0405, attributeId: 0x0000)

    // DoorLock
    static let lockState = AttributePath(endpointId: 1, clusterId: 0x0101, attributeId: 0x0000)

    // PowerSource
    static let batteryRemaining = AttributePath(endpointId: 0, clusterId: 0x002F, attributeId: 0x000C)

    // Thermostat
    static let localTemperature = AttributePath(endpointId: 1, clusterId: 0x0201, attributeId: 0x0000)
    static let heatingSetpoint = AttributePath(endpointId: 1, clusterId: 0x0201, attributeId: 0x0012)
    static let coolingSetpoint = AttributePath(endpointId: 1, clusterId: 0x0201, attributeId: 0x0011)
    static let thermostatMode = AttributePath(endpointId: 1, clusterId: 0x0201, attributeId: 0x001C)

    // FanControl
    static let fanMode = AttributePath(endpointId: 1, clusterId: 0x0202, attributeId: 0x0000)
    static let fanPercent = AttributePath(endpointId: 1, clusterId: 0x0202, attributeId: 0x0002)

    // WindowCovering
    static let coverPosition = AttributePath(endpointId: 1, clusterId: 0x0102, attributeId: 0x000E)

    // OccupancySensing
    static let occupancy = AttributePath(endpointId: 1, clusterId: 0x0406, attributeId: 0x0000)

    // BooleanState (contact sensor)
    static let contactState = AttributePath(endpointId: 1, clusterId: 0x0045, attributeId: 0x0000)

    // ColorControl
    static let colorTemp = AttributePath(endpointId: 1, clusterId: 0x0300, attributeId: 0x0007)
}
