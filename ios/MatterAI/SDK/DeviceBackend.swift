import Foundation

// MARK: - Backend Source

/// Mirrors matter::BackendSource from C++
enum BackendSource: String, CaseIterable, Sendable {
    case local       = "Local"
    case appleMatter = "Apple Matter"
    case homeKit     = "HomeKit"
    case googleHome  = "Google Home"
    case thread      = "Thread"
}

// MARK: - Attribute Path

/// A fully-qualified path to a Matter attribute
struct AttributePath: Hashable, Sendable {
    let endpointId: UInt16
    let clusterId: UInt32
    let attributeId: UInt32
}

// MARK: - Attribute Value

/// A type-erased attribute value that can cross backend boundaries
enum SDKAttributeValue: Sendable {
    case bool(Bool)
    case int(Int64)
    case float(Float)
    case string(String)
    case bytes(Data)

    var boolValue: Bool? {
        if case .bool(let v) = self { return v }
        return nil
    }
    var intValue: Int64? {
        if case .int(let v) = self { return v }
        return nil
    }
    var floatValue: Float? {
        if case .float(let v) = self { return v }
        return nil
    }
}

// MARK: - Device Backend Protocol

/// Protocol that all platform backends implement.
/// Each backend wraps a specific SDK (Apple Matter, HomeKit, Google Home, Thread)
/// and exposes a uniform interface for device discovery and control.
protocol DeviceBackend: AnyObject, Sendable {
    /// Which platform this backend wraps
    var source: BackendSource { get }

    /// Whether this backend is available on the current device/OS
    var isAvailable: Bool { get }

    /// Whether this backend is currently connected and discovering
    var isActive: Bool { get }

    /// Start device discovery / listening for state changes
    func startDiscovery() async throws

    /// Stop discovery
    func stopDiscovery() async

    /// Commission / pair a new device (backend-specific flow)
    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice

    /// Read a single attribute
    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue

    /// Write a single attribute
    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws

    /// Invoke a command on a cluster
    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws

    /// Subscribe to attribute changes. Returns an AsyncStream of updates.
    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate>

    /// Get all currently known devices from this backend
    func knownDevices() -> [UnifiedDevice]
}

// MARK: - Attribute Update

/// An attribute change notification from a subscription
struct AttributeUpdate: Sendable {
    let deviceId: String
    let path: AttributePath
    let value: SDKAttributeValue
    let timestamp: Date
}

// MARK: - Backend Error

enum BackendError: Error, LocalizedError {
    case notAvailable(BackendSource)
    case notConnected
    case deviceNotFound(String)
    case attributeNotFound(AttributePath)
    case commissioningFailed(String)
    case writeFailure(String)
    case commandFailure(String)
    case timeout

    var errorDescription: String? {
        switch self {
        case .notAvailable(let src): return "\(src.rawValue) backend not available on this device"
        case .notConnected: return "Backend not connected"
        case .deviceNotFound(let id): return "Device \(id) not found"
        case .attributeNotFound(let path): return "Attribute not found: ep\(path.endpointId)/0x\(String(path.clusterId, radix: 16))/0x\(String(path.attributeId, radix: 16))"
        case .commissioningFailed(let msg): return "Commissioning failed: \(msg)"
        case .writeFailure(let msg): return "Write failed: \(msg)"
        case .commandFailure(let msg): return "Command failed: \(msg)"
        case .timeout: return "Operation timed out"
        }
    }
}
