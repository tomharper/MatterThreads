import Foundation

/// Backend that wraps our C++ DeviceManager via MatterBridge.
/// This provides the in-memory demo home as a backend source.
final class LocalBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .local
    var isAvailable: Bool { true }
    private(set) var isActive: Bool = false

    private let bridge: MatterBridge

    init(bridge: MatterBridge) {
        self.bridge = bridge
    }

    func startDiscovery() async throws {
        bridge.loadDemoHome()
        isActive = true
    }

    func stopDiscovery() async {
        isActive = false
    }

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        throw BackendError.commissioningFailed("Local backend does not support commissioning")
    }

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        // Read from C++ DeviceManager via bridge
        let devices = bridge.allDevices() as [MADevice]
        guard let device = devices.first(where: { String($0.nodeId) == deviceId }) else {
            throw BackendError.deviceNotFound(deviceId)
        }

        // Map common paths to MADevice properties
        if path == .onOff { return .bool(device.isOn) }
        if path == .lockState { return .int(device.isLocked ? 1 : 0) }
        if path == .measuredTemp, let t = device.temperature { return .int(Int64(t.floatValue * 100)) }
        if path == .measuredHumidity, let h = device.humidity { return .int(Int64(h.floatValue * 100)) }
        if path == .currentLevel, let b = device.brightness { return .int(Int64(b.intValue)) }
        if path == .batteryRemaining, let bat = device.battery { return .int(Int64(bat.intValue * 2)) }

        throw BackendError.attributeNotFound(path)
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        guard let nodeId = UInt64(deviceId) else {
            throw BackendError.deviceNotFound(deviceId)
        }

        switch value {
        case .bool(let v):
            bridge.updateAttribute(forNode: nodeId, endpoint: path.endpointId,
                                   cluster: path.clusterId, attribute: path.attributeId, boolValue: v)
        case .int(let v):
            bridge.updateAttribute(forNode: nodeId, endpoint: path.endpointId,
                                   cluster: path.clusterId, attribute: path.attributeId, intValue: v)
        case .float(let v):
            bridge.updateAttribute(forNode: nodeId, endpoint: path.endpointId,
                                   cluster: path.clusterId, attribute: path.attributeId, floatValue: v)
        default:
            throw BackendError.writeFailure("Unsupported value type for local backend")
        }
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        guard let nodeId = UInt64(deviceId) else {
            throw BackendError.deviceNotFound(deviceId)
        }
        // OnOff cluster commands
        if clusterId == 0x0006 {
            let on = commandId == 0x0001  // On=1, Off=0, Toggle=2
            if commandId == 0x0002 {
                // Toggle: read current, invert
                let devices = bridge.allDevices() as [MADevice]
                let isOn = devices.first(where: { $0.nodeId == nodeId })?.isOn ?? false
                bridge.updateAttribute(forNode: nodeId, endpoint: endpointId,
                                       cluster: clusterId, attribute: 0x0000, boolValue: !isOn)
            } else {
                bridge.updateAttribute(forNode: nodeId, endpoint: endpointId,
                                       cluster: clusterId, attribute: 0x0000, boolValue: on)
            }
        }
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        // Local backend: poll C++ state
        return AsyncStream { continuation in
            let task = Task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(maxInterval))
                    guard !Task.isCancelled else { break }
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
        let maDevices = bridge.allDevices() as [MADevice]
        return maDevices.map { maDevice in
            var attrs: [AttributePath: SDKAttributeValue] = [:]

            // Populate cached attributes
            attrs[.onOff] = .bool(maDevice.isOn)

            if let temp = maDevice.temperature {
                attrs[.measuredTemp] = .int(Int64(temp.floatValue * 100))
            }
            if let hum = maDevice.humidity {
                attrs[.measuredHumidity] = .int(Int64(hum.floatValue * 100))
            }
            if let lvl = maDevice.brightness {
                attrs[.currentLevel] = .int(Int64(lvl.intValue))
            }
            if let bat = maDevice.battery {
                attrs[.batteryRemaining] = .int(Int64(bat.intValue * 2))
            }
            if maDevice.isLocked {
                attrs[.lockState] = .int(1)
            }

            return UnifiedDevice(
                id: "\(BackendSource.local.rawValue):\(maDevice.nodeId)",
                source: .local,
                nativeId: String(maDevice.nodeId),
                name: maDevice.name,
                room: maDevice.room,
                vendor: maDevice.vendorName,
                deviceType: 0,
                reachable: maDevice.reachable,
                attributes: attrs,
                lastUpdated: Date()
            )
        }
    }
}
