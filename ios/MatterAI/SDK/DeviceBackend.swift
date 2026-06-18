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

    var displayString: String {
        switch self {
        case .bool(let v): return v ? "on" : "off"
        case .int(let v): return "\(v)"
        case .float(let v): return String(format: "%.2f", v)
        case .string(let v): return v
        case .bytes(let v): return "\(v.count) bytes"
        }
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

    /// Open a Matter commissioning window on an already-commissioned device so it
    /// can be shared with additional fabrics (Apple Home, Google Home, …). Returns
    /// the pairing codes the user enters in the other ecosystem's app. This is the
    /// Matter "multi-admin" mechanism — only Matter-based backends support it.
    func openCommissioningWindow(deviceId: String, duration: TimeInterval) async throws -> PairingHandoff
}

// Default: backends that aren't Matter fabrics can't hand a device to another
// ecosystem this way. Surface a clear, specific message rather than a crash.
extension DeviceBackend {
    func openCommissioningWindow(deviceId: String, duration: TimeInterval) async throws -> PairingHandoff {
        throw BackendError.commissioningFailed(
            "Multi-admin sharing isn't supported for \(source.rawValue) devices — only Matter-commissioned devices can be shared to other ecosystems.")
    }
}

// MARK: - Pairing Handoff

/// Codes produced by opening a commissioning window. The user enters the manual
/// code (or scans the QR) in Apple Home and Google Home to add the device there.
struct PairingHandoff: Sendable {
    /// 11-digit Matter manual pairing code, e.g. "3497-011-2332".
    let manualCode: String
    /// QR-code payload string ("MT:..."), if available (iOS 17.6+).
    let qrCode: String?
    /// Seconds the window stays open.
    let durationSeconds: Int
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
    case notConfigured(String)
    case deviceUnreachable(String)
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
        case .notConfigured(let msg): return msg
        case .deviceUnreachable(let msg): return msg
        case .deviceNotFound(let id): return "Device \(id) not found"
        case .attributeNotFound(let path): return "Attribute not found: ep\(path.endpointId)/0x\(String(path.clusterId, radix: 16))/0x\(String(path.attributeId, radix: 16))"
        case .commissioningFailed(let msg): return "Commissioning failed: \(msg)"
        case .writeFailure(let msg): return "Write failed: \(msg)"
        case .commandFailure(let msg): return "Command failed: \(msg)"
        case .timeout: return "Operation timed out"
        }
    }
}
