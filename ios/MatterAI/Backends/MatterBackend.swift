import Foundation
#if canImport(Matter)
import Matter
#endif

/// Backend wrapping Apple's Matter.framework (MTRDeviceController).
/// Requires iOS 16.1+ and entitlements for Matter support.
final class MatterBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .appleMatter

    var isAvailable: Bool {
        #if canImport(Matter)
        if #available(iOS 16.1, *) { return true }
        #endif
        return false
    }

    private(set) var isActive: Bool = false

    #if canImport(Matter)
    @available(iOS 16.1, *)
    private var controller: MTRDeviceController?
    private var discoveredDevices: [String: UnifiedDevice] = [:]
    private let lock = NSLock()
    #endif

    func startDiscovery() async throws {
        #if canImport(Matter)
        guard isAvailable else { throw BackendError.notAvailable(source) }

        if #available(iOS 16.1, *) {
            // In a real app, you'd set up MTRDeviceControllerFactory,
            // create storage, and start the controller.
            // For now, we mark as active — actual pairing happens via commission().
            //
            // let factory = MTRDeviceControllerFactory.sharedInstance()
            // let params = MTRDeviceControllerStartupParams(...)
            // controller = try factory.createController(onExistingFabric: params)
            isActive = true
        }
        #else
        throw BackendError.notAvailable(source)
        #endif
    }

    func stopDiscovery() async {
        #if canImport(Matter)
        if #available(iOS 16.1, *) {
            controller?.shutdown()
            controller = nil
        }
        #endif
        isActive = false
    }

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        #if canImport(Matter)
        guard isActive else { throw BackendError.notConnected }

        if #available(iOS 16.4, *) {
            // Real commissioning would use:
            // let setupPayload = try MTRSetupPayload(onboardingPayload: payload ?? "")
            // let params = MTRCommissioningParameters()
            // try controller?.commissionNode(withID: nodeId as NSNumber, commissioningParams: params)
            //
            // For now, return a placeholder
            let device = UnifiedDevice(
                id: "\(source.rawValue):\(deviceId)",
                source: source,
                nativeId: deviceId,
                name: "Matter Device \(deviceId)",
                room: "Unknown",
                vendor: "",
                deviceType: 0x0100,
                reachable: true,
                attributes: [:],
                lastUpdated: Date()
            )
            lock.withLock { discoveredDevices[deviceId] = device }
            return device
        }
        #endif
        throw BackendError.commissioningFailed("Matter.framework not available")
    }

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        #if canImport(Matter)
        guard isActive else { throw BackendError.notConnected }

        if #available(iOS 16.1, *) {
            // Real read:
            // let device = MTRDevice(nodeID: nodeId as NSNumber, controller: controller!)
            // let result = try await device.readAttribute(
            //     withEndpointID: path.endpointId as NSNumber,
            //     clusterID: path.clusterId as NSNumber,
            //     attributeID: path.attributeId as NSNumber,
            //     params: nil
            // )
            throw BackendError.attributeNotFound(path)
        }
        #endif
        throw BackendError.notConnected
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        #if canImport(Matter)
        guard isActive else { throw BackendError.notConnected }

        if #available(iOS 16.1, *) {
            // Real write:
            // let device = MTRDevice(nodeID: nodeId as NSNumber, controller: controller!)
            // let writeParams = MTRWriteParams()
            // try await device.writeAttribute(...)
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        #if canImport(Matter)
        guard isActive else { throw BackendError.notConnected }

        if #available(iOS 16.1, *) {
            // Real invoke:
            // let device = MTRDevice(nodeID: nodeId as NSNumber, controller: controller!)
            // try await device.invokeCommand(
            //     withEndpointID: endpointId as NSNumber,
            //     clusterID: clusterId as NSNumber,
            //     commandID: commandId as NSNumber,
            //     commandFields: payload as NSDictionary
            // )
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        return AsyncStream { continuation in
            #if canImport(Matter)
            // Real subscription:
            // let device = MTRDevice(nodeID: nodeId as NSNumber, controller: controller!)
            // device.setDelegate(subscriber, queue: .main)
            // The delegate receives attributeData updates
            #endif
            continuation.finish()
        }
    }

    func knownDevices() -> [UnifiedDevice] {
        #if canImport(Matter)
        return lock.withLock { Array(discoveredDevices.values) }
        #else
        return []
        #endif
    }
}
