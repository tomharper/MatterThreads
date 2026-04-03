import Foundation
import Combine

/// Routes device operations to the correct backend based on device source.
/// This is the internal coordinator — app code uses MatterHomeSDK instead.
@MainActor
class BackendRouter: ObservableObject {
    @Published private(set) var devices: [UnifiedDevice] = []
    @Published private(set) var activeBackends: [BackendSource] = []

    private var backends: [BackendSource: DeviceBackend] = [:]
    private var subscriptionTasks: [String: Task<Void, Never>] = [:]

    // MARK: - Backend Registration

    func register(_ backend: DeviceBackend) {
        backends[backend.source] = backend
    }

    func backend(for source: BackendSource) -> DeviceBackend? {
        backends[source]
    }

    // MARK: - Discovery

    func startAll() async {
        for (source, backend) in backends {
            guard backend.isAvailable else { continue }
            do {
                try await backend.startDiscovery()
                activeBackends.append(source)
            } catch {
                print("[BackendRouter] Failed to start \(source.rawValue): \(error)")
            }
        }
        refreshDeviceList()
    }

    func stopAll() async {
        for backend in backends.values {
            await backend.stopDiscovery()
        }
        activeBackends.removeAll()
        cancelAllSubscriptions()
    }

    // MARK: - Device Operations

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        let backend = try resolveBackend(for: deviceId)
        let nativeId = extractNativeId(from: deviceId)
        return try await backend.readAttribute(deviceId: nativeId, path: path)
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        let backend = try resolveBackend(for: deviceId)
        let nativeId = extractNativeId(from: deviceId)
        try await backend.writeAttribute(deviceId: nativeId, path: path, value: value)
        refreshDeviceList()
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any] = [:]) async throws {
        let backend = try resolveBackend(for: deviceId)
        let nativeId = extractNativeId(from: deviceId)
        try await backend.invokeCommand(deviceId: nativeId, endpointId: endpointId,
                                        clusterId: clusterId, commandId: commandId, payload: payload)
        refreshDeviceList()
    }

    func toggleDevice(_ device: UnifiedDevice) async throws {
        let newState: SDKAttributeValue = .bool(!device.isOn)
        try await writeAttribute(deviceId: device.id, path: .onOff, value: newState)
    }

    func commission(backend source: BackendSource, deviceId: String, payload: String? = nil) async throws -> UnifiedDevice {
        guard let backend = backends[source] else {
            throw BackendError.notAvailable(source)
        }
        let device = try await backend.commission(deviceId: deviceId, payload: payload)
        refreshDeviceList()
        return device
    }

    // MARK: - Subscriptions

    func subscribeToDevice(_ deviceId: String, paths: [AttributePath],
                           minInterval: TimeInterval = 1, maxInterval: TimeInterval = 60) {
        cancelSubscription(for: deviceId)

        let task = Task { [weak self] in
            guard let backend = try? self?.resolveBackend(for: deviceId) else { return }
            let nativeId = self?.extractNativeId(from: deviceId) ?? deviceId
            let stream = backend.subscribe(deviceId: nativeId, paths: paths,
                                           minInterval: minInterval, maxInterval: maxInterval)
            for await update in stream {
                guard !Task.isCancelled else { break }
                await self?.handleAttributeUpdate(deviceId: deviceId, update: update)
            }
        }
        subscriptionTasks[deviceId] = task
    }

    // MARK: - Internal

    func refreshDeviceList() {
        var all: [UnifiedDevice] = []
        for backend in backends.values {
            all.append(contentsOf: backend.knownDevices())
        }
        // Sort: reachable first, then by room, then by name
        all.sort { lhs, rhs in
            if lhs.reachable != rhs.reachable { return lhs.reachable }
            if lhs.room != rhs.room { return lhs.room < rhs.room }
            return lhs.name < rhs.name
        }
        devices = all
    }

    private func handleAttributeUpdate(deviceId: String, update: AttributeUpdate) {
        if let idx = devices.firstIndex(where: { $0.id == deviceId }) {
            devices[idx].attributes[update.path] = update.value
            devices[idx].lastUpdated = update.timestamp
        }
    }

    private func resolveBackend(for deviceId: String) throws -> DeviceBackend {
        let source = extractSource(from: deviceId)
        guard let backend = backends[source] else {
            throw BackendError.notAvailable(source)
        }
        guard backend.isActive else {
            throw BackendError.notConnected
        }
        return backend
    }

    private func extractSource(from deviceId: String) -> BackendSource {
        let prefix = deviceId.components(separatedBy: ":").first ?? ""
        return BackendSource(rawValue: prefix) ?? .local
    }

    private func extractNativeId(from deviceId: String) -> String {
        let parts = deviceId.components(separatedBy: ":")
        return parts.count > 1 ? parts.dropFirst().joined(separator: ":") : deviceId
    }

    private func cancelSubscription(for deviceId: String) {
        subscriptionTasks[deviceId]?.cancel()
        subscriptionTasks.removeValue(forKey: deviceId)
    }

    private func cancelAllSubscriptions() {
        for task in subscriptionTasks.values { task.cancel() }
        subscriptionTasks.removeAll()
    }
}
