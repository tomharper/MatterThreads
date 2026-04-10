import Foundation
#if canImport(ThreadNetwork)
import ThreadNetwork
#endif

/// Backend for Thread network diagnostics.
/// Uses Apple's ThreadNetwork.framework (iOS 15+) for credential sharing,
/// and our MatterThreads simulation for detailed mesh diagnostics.
final class ThreadBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .thread

    var isAvailable: Bool {
        #if canImport(ThreadNetwork)
        if #available(iOS 15.0, *) { return true }
        #endif
        return false
    }

    private(set) var isActive: Bool = false
    private var discoveredDevices: [String: UnifiedDevice] = [:]
    private let lock = NSLock()

    // Thread network info from Apple's framework
    private var preferredNetworkName: String?
    private var preferredPanId: UInt16?

    // Connection to MatterThreads Dashboard API for mesh diagnostics
    private var dashboardURL: URL?
    private let session = URLSession.shared

    /// Configure with optional MatterThreads Dashboard URL for detailed mesh data.
    /// If nil, reads from SimulationConfig.shared.dashboardURL at runtime.
    func configure(dashboardURL: URL? = nil) {
        if let url = dashboardURL {
            self.dashboardURL = url
        }
        // If still nil, will resolve lazily from SimulationConfig
    }

    /// Resolves dashboard URL — explicit override or falls back to SimulationConfig
    private func resolvedDashboardURL() -> URL? {
        if let url = dashboardURL { return url }
        // Read from shared config (MainActor-isolated, but we only need the string)
        let urlString = UserDefaults.standard.string(forKey: "sim.dashboardURL")
            ?? "http://localhost:8080"
        return URL(string: urlString)
    }

    // MARK: - DeviceBackend

    func startDiscovery() async throws {
        #if canImport(ThreadNetwork)
        if #available(iOS 15.0, *) {
            // Query preferred Thread network credentials
            let client = THClient()
            do {
                let credential = try await client.preferredCredentials()
                preferredNetworkName = credential.networkName
                // THCredentials doesn't expose panId directly in all iOS versions,
                // but networkName is always available
            } catch {
                // No Thread network configured — that's OK, we still start
                print("[ThreadBackend] No preferred Thread credentials: \(error)")
            }
        }
        #endif

        // Also fetch from MatterThreads Dashboard if configured
        if resolvedDashboardURL() != nil {
            await fetchMeshNodes()
        }

        isActive = true
    }

    func stopDiscovery() async {
        isActive = false
        lock.withLock { discoveredDevices.removeAll() }
    }

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        throw BackendError.commissioningFailed("Thread backend is for diagnostics — commission via Matter backend")
    }

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        guard isActive else { throw BackendError.notConnected }

        // Thread diagnostics: expose mesh health data as attributes
        if path.clusterId == UInt32(0x0035) { // ThreadDiagnostics cluster
            return try await readThreadDiagnostic(deviceId: deviceId, attributeId: path.attributeId)
        }

        throw BackendError.attributeNotFound(path)
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        throw BackendError.writeFailure("Thread diagnostics are read-only")
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        throw BackendError.commandFailure("Thread diagnostics don't support commands")
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        return AsyncStream { continuation in
            let task = Task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(maxInterval))
                    guard !Task.isCancelled else { break }
                    await self.fetchMeshNodes()
                    // Emit updates for requested paths
                    for path in paths {
                        if let value = try? await self.readAttribute(deviceId: deviceId, path: path) {
                            continuation.yield(AttributeUpdate(
                                deviceId: "\(self.source.rawValue):\(deviceId)",
                                path: path,
                                value: value,
                                timestamp: Date()
                            ))
                        }
                    }
                }
                continuation.finish()
            }
            continuation.onTermination = { _ in task.cancel() }
        }
    }

    func knownDevices() -> [UnifiedDevice] {
        lock.withLock { Array(discoveredDevices.values) }
    }

    // MARK: - Thread Diagnostics

    /// Thread diagnostic attribute IDs (from Matter Thread Network Diagnostics cluster 0x0035)
    enum ThreadDiagAttr: UInt32 {
        case channel = 0x0000
        case routingRole = 0x0001
        case networkName = 0x0002
        case panId = 0x0003
        case extendedPanId = 0x0004
        case meshLocalPrefix = 0x0005
        case neighborTableCount = 0x0007
        case routeTableCount = 0x0008
        case partitionId = 0x0009
        case leaderRouterId = 0x000B
        case detachedRoleCount = 0x000C
        case childRoleCount = 0x000D
        case routerRoleCount = 0x000E
        case leaderRoleCount = 0x000F
    }

    private func readThreadDiagnostic(deviceId: String, attributeId: UInt32) async throws -> SDKAttributeValue {
        guard let dashURL = resolvedDashboardURL() else {
            throw BackendError.notConnected
        }

        // Fetch node-specific data from Dashboard API
        let url = dashURL.appendingPathComponent("api/status")
        let (data, _) = try await session.data(from: url)
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let nodes = json["nodes"] as? [[String: Any]] else {
            throw BackendError.attributeNotFound(AttributePath(endpointId: 0, clusterId: 0x0035, attributeId: attributeId))
        }

        // Find matching node
        guard let node = nodes.first(where: { "\($0["node_id"] ?? "")" == deviceId }) else {
            throw BackendError.deviceNotFound(deviceId)
        }

        // Map attribute ID to JSON field
        guard let attr = ThreadDiagAttr(rawValue: attributeId) else {
            throw BackendError.attributeNotFound(AttributePath(endpointId: 0, clusterId: 0x0035, attributeId: attributeId))
        }

        switch attr {
        case .routingRole:
            let role = node["role"] as? String ?? "disabled"
            let roleValue: Int64
            switch role {
            case "leader": roleValue = 6
            case "router": roleValue = 5
            case "child", "end-device": roleValue = 4
            case "reed": roleValue = 3
            case "sed": roleValue = 2
            default: roleValue = 0
            }
            return .int(roleValue)
        case .networkName:
            return .string(preferredNetworkName ?? (node["network"] as? String ?? "Thread"))
        case .channel:
            return .int(Int64(node["channel"] as? Int ?? 15))
        default:
            return .int(0)
        }
    }

    // MARK: - Dashboard Integration

    private func fetchMeshNodes() async {
        guard let dashURL = resolvedDashboardURL() else { return }

        let url = dashURL.appendingPathComponent("api/status")
        guard let (data, _) = try? await session.data(from: url),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let nodes = json["nodes"] as? [[String: Any]] else {
            return
        }

        var devices: [String: UnifiedDevice] = [:]
        for node in nodes {
            guard let nodeId = node["node_id"] as? Int else { continue }
            let deviceId = String(nodeId)
            let role = node["role"] as? String ?? "unknown"
            let state = node["state"] as? String ?? "unknown"
            let reachable = state != "offline" && state != "unreachable"

            // Thread devices expose diagnostics on endpoint 0
            var attrs: [AttributePath: SDKAttributeValue] = [:]
            let diagPath = { (attrId: UInt32) in
                AttributePath(endpointId: 0, clusterId: 0x0035, attributeId: attrId)
            }

            // Role
            let roleValue: Int64
            switch role {
            case "leader": roleValue = 6
            case "router": roleValue = 5
            case "end-device", "child": roleValue = 4
            default: roleValue = 0
            }
            attrs[diagPath(0x0001)] = .int(roleValue)

            // Network name
            attrs[diagPath(0x0002)] = .string(preferredNetworkName ?? "MatterThreads")

            devices[deviceId] = UnifiedDevice(
                id: "\(source.rawValue):\(deviceId)",
                source: source,
                nativeId: deviceId,
                name: "Thread Node \(nodeId) (\(role))",
                room: "Network",
                vendor: "Thread",
                deviceType: 0,
                reachable: reachable,
                attributes: attrs,
                lastUpdated: Date()
            )
        }

        lock.withLock { discoveredDevices = devices }
    }
}
