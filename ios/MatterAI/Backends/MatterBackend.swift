import Foundation
#if canImport(Matter)
import Matter
import CryptoKit
import Security
#endif

/// Backend wrapping Apple's Matter.framework (`MTRDeviceController`).
///
/// This drives the real SDK: it brings up an `MTRDeviceControllerFactory`, creates
/// a controller on a local test fabric, commissions devices over the real PASE/CASE
/// flow, and performs attribute reads/writes, command invokes and subscriptions via
/// `MTRBaseDevice`.
///
/// Requirements to actually run (not just compile):
///   - A physical iOS 16.4+ device (Matter.framework does not function in the Simulator).
///   - Entitlements: `com.apple.developer.matter.allow-setup-payload`,
///     `com.apple.developer.networking.multicast` (see MatterAI.entitlements).
///   - Reachable Matter devices on the LAN; for Thread devices, a Thread Border
///     Router in the network.
///
/// The local-fabric bring-up here uses an in-memory store and a freshly generated
/// signing key, i.e. a throwaway fabric for development. A production app would
/// persist storage and reuse a stable operational identity (or commission through
/// the system MatterSupport flow into the user's Home fabric).
final class MatterBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .appleMatter

    var isAvailable: Bool {
        #if canImport(Matter)
        if #available(iOS 16.4, *) { return true }
        #endif
        return false
    }

    private(set) var isActive: Bool = false

    #if canImport(Matter)
    private static let fabricID: UInt64 = 1
    private let workQueue = DispatchQueue(label: "MatterBackend.work")
    private let lock = NSLock()
    private var discoveredDevices: [String: UnifiedDevice] = [:]
    private var nextNodeID: UInt64 = 0x1000

    @available(iOS 16.4, *)
    private var controller: MTRDeviceController? {
        get { _controller as? MTRDeviceController }
        set { _controller = newValue }
    }
    private var _controller: AnyObject?
    private var keypair: MatterTestKeypair?
    private var commissioningDelegate: MatterCommissioningDelegate?
    #endif

    // MARK: - Lifecycle

    func startDiscovery() async throws {
        #if canImport(Matter)
        guard isAvailable else { throw BackendError.notAvailable(source) }
        if #available(iOS 16.4, *) {
            if controller == nil { try bringUpController() }
            isActive = true
            return
        }
        #endif
        throw BackendError.notAvailable(source)
    }

    func stopDiscovery() async {
        #if canImport(Matter)
        if #available(iOS 16.4, *) {
            controller?.shutdown()
            controller = nil
            MTRDeviceControllerFactory.sharedInstance().stop()
        }
        #endif
        isActive = false
    }

    #if canImport(Matter)
    /// Start the controller factory and create a controller on a fresh local fabric.
    @available(iOS 16.4, *)
    private func bringUpController() throws {
        let factory = MTRDeviceControllerFactory.sharedInstance()
        let storage = MatterInMemoryStorage()
        let factoryParams = MTRDeviceControllerFactoryParams(storage: storage)
        try factory.start(factoryParams)   // startControllerFactory:error:

        let kp = MatterTestKeypair()
        self.keypair = kp

        // 16-byte Identity Protection Key for the fabric.
        var ipkBytes = [UInt8](repeating: 0, count: 16)
        _ = SecRandomCopyBytes(kSecRandomDefault, ipkBytes.count, &ipkBytes)
        let ipk = Data(ipkBytes)

        let startupParams = MTRDeviceControllerStartupParams(
            ipk: ipk, fabricID: NSNumber(value: Self.fabricID), nocSigner: kp)
        startupParams.vendorID = NSNumber(value: 0xFFF1)   // test vendor id

        let controller = try factory.createController(onNewFabric: startupParams)
        let delegate = MatterCommissioningDelegate()
        controller.setDeviceControllerDelegate(delegate, queue: workQueue)
        self.commissioningDelegate = delegate
        self.controller = controller
    }
    #endif

    // MARK: - Commissioning

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        #if canImport(Matter)
        guard isActive else { throw BackendError.notConnected }
        if #available(iOS 16.4, *) {
            guard let controller = controller, let delegate = commissioningDelegate else {
                throw BackendError.notConnected
            }
            guard let onboarding = payload,
                  let setupPayload = MTRSetupPayload(payload: onboarding) else {
                throw BackendError.commissioningFailed("Invalid setup payload")
            }

            let nodeID = lock.withLock { () -> UInt64 in nextNodeID += 1; return nextNodeID }
            let nodeNum = NSNumber(value: nodeID)

            // Phase 1: establish the PASE commissioning session. Bounded so an
            // unreachable / wrong-code device reports clearly instead of hanging.
            try await withDeviceTimeout(30) { (finish: @escaping (Result<Void, Error>) -> Void) in
                delegate.onSessionEstablished = { error in
                    finish(error.map { .failure($0) } ?? .success(()))
                }
                do {
                    try controller.setupCommissioningSession(with: setupPayload, newNodeID: nodeNum)
                } catch {
                    finish(.failure(error))
                }
            }

            // Phase 2: commission the node onto the fabric.
            try await withDeviceTimeout(60) { (finish: @escaping (Result<Void, Error>) -> Void) in
                delegate.onCommissioningComplete = { error in
                    finish(error.map { .failure($0) } ?? .success(()))
                }
                do {
                    let params = MTRCommissioningParameters()
                    try controller.commissionNode(withID: nodeNum, commissioningParams: params)
                } catch {
                    finish(.failure(error))
                }
            }

            let device = UnifiedDevice(
                id: "\(source.rawValue):\(nodeID)",
                source: source,
                nativeId: String(nodeID),
                name: "Matter Device \(deviceId)",
                room: "Unknown",
                vendor: "",
                deviceType: 0,
                reachable: true,
                attributes: [:],
                lastUpdated: Date())
            lock.withLock { discoveredDevices[String(nodeID)] = device }
            return device
        }
        #endif
        throw BackendError.commissioningFailed("Matter.framework not available")
    }

    // MARK: - Attribute / command operations

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        #if canImport(Matter)
        if #available(iOS 16.4, *) {
            let device = try baseDevice(for: deviceId)
            let values: [[String: Any]] = try await withDeviceTimeout { finish in
                device.readAttributes(
                    withEndpointID: NSNumber(value: path.endpointId),
                    clusterID: NSNumber(value: path.clusterId),
                    attributeID: NSNumber(value: path.attributeId),
                    params: nil, queue: self.workQueue) { values, error in
                        finish(error.map { .failure($0) } ?? .success(values ?? []))
                    }
            }
            guard let dataValue = values.first?[MTRDataKey] as? [String: Any],
                  let value = Self.decode(dataValue) else {
                throw BackendError.attributeNotFound(path)
            }
            return value
        }
        #endif
        throw BackendError.notConnected
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        #if canImport(Matter)
        if #available(iOS 16.4, *) {
            let device = try baseDevice(for: deviceId)
            let encoded = Self.encode(value)
            let _: [[String: Any]] = try await withDeviceTimeout { finish in
                device.writeAttribute(
                    withEndpointID: NSNumber(value: path.endpointId),
                    clusterID: NSNumber(value: path.clusterId),
                    attributeID: NSNumber(value: path.attributeId),
                    value: encoded, timedWriteTimeout: nil, queue: self.workQueue) { values, error in
                        finish(error.map { .failure($0) } ?? .success(values ?? []))
                    }
            }
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        #if canImport(Matter)
        if #available(iOS 16.4, *) {
            let device = try baseDevice(for: deviceId)
            // Most lock/on-off style commands carry no fields. Structured field
            // encoding (PIN codes, etc.) needs per-command context-tag mapping;
            // pass an empty structure when no fields are supplied.
            let fields: [String: Any] = [MTRTypeKey: MTRStructureValueType, MTRValueKey: [[String: Any]]()]
            let _: [[String: Any]] = try await withDeviceTimeout { finish in
                device.invokeCommand(
                    withEndpointID: NSNumber(value: endpointId),
                    clusterID: NSNumber(value: clusterId),
                    commandID: NSNumber(value: commandId),
                    commandFields: fields, timedInvokeTimeout: nil, queue: self.workQueue) { values, error in
                        finish(error.map { .failure($0) } ?? .success(values ?? []))
                    }
            }
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        return AsyncStream { continuation in
            #if canImport(Matter)
            if #available(iOS 16.4, *), let device = try? baseDevice(for: deviceId) {
                let params = MTRSubscribeParams(
                    minInterval: NSNumber(value: Int(minInterval)),
                    maxInterval: NSNumber(value: Int(maxInterval)))
                // Wildcard subscribe (all attributes), then filter to requested paths.
                let wanted = Set(paths)
                device.subscribeToAttributes(
                    withEndpointID: nil, clusterID: nil, attributeID: nil,
                    params: params, queue: workQueue,
                    reportHandler: { values, error in
                        guard error == nil, let values = values else { return }
                        for entry in values {
                            guard let ap = entry[MTRAttributePathKey] as? MTRAttributePath,
                                  let data = entry[MTRDataKey] as? [String: Any],
                                  let value = Self.decode(data) else { continue }
                            let path = AttributePath(
                                endpointId: ap.endpoint.uint16Value,
                                clusterId: ap.cluster.uint32Value,
                                attributeId: ap.attribute.uint32Value)
                            if wanted.isEmpty || wanted.contains(path) {
                                continuation.yield(AttributeUpdate(
                                    deviceId: deviceId, path: path, value: value, timestamp: Date()))
                            }
                        }
                    },
                    subscriptionEstablished: nil)
            } else {
                continuation.finish()
            }
            #else
            continuation.finish()
            #endif
        }
    }

    func knownDevices() -> [UnifiedDevice] {
        #if canImport(Matter)
        return lock.withLock { Array(discoveredDevices.values) }
        #else
        return []
        #endif
    }

    // MARK: - Helpers

    #if canImport(Matter)
    @available(iOS 16.4, *)
    private func baseDevice(for deviceId: String) throws -> MTRBaseDevice {
        guard isActive, let controller = controller else { throw BackendError.notConnected }
        guard let nodeID = UInt64(deviceId) else { throw BackendError.deviceNotFound(deviceId) }
        return MTRBaseDevice(nodeID: NSNumber(value: nodeID), controller: controller)
    }

    /// One-shot resume guard so a continuation is resumed exactly once (the MTR
    /// completion may race the timeout).
    private final class ResumeGuard {
        private let lock = NSLock()
        private var claimed = false
        func claim() -> Bool { lock.lock(); defer { lock.unlock() }; if claimed { return false }; claimed = true; return true }
    }

    /// Run a Matter operation whose completion fires once, bounded by a timeout.
    /// On timeout (or if the op never calls back) it surfaces a clear, human
    /// "couldn't reach the device" error instead of hanging or leaking a raw CHIP code.
    private func withDeviceTimeout<T>(
        _ seconds: TimeInterval = 20,
        perform: @escaping (@escaping (Result<T, Error>) -> Void) -> Void
    ) async throws -> T {
        let guardOnce = ResumeGuard()
        return try await withCheckedThrowingContinuation { (cont: CheckedContinuation<T, Error>) in
            let finish: (Result<T, Error>) -> Void = { result in
                guard guardOnce.claim() else { return }
                cont.resume(with: result)
            }
            workQueue.asyncAfter(deadline: .now() + seconds) {
                finish(.failure(BackendError.deviceUnreachable(
                    "Couldn't reach the Matter device within \(Int(seconds))s — check it's powered on, in range, and on the same network.")))
            }
            perform(finish)
        }
    }

    /// Decode a Matter data-value dictionary into an SDKAttributeValue.
    private static func decode(_ dataValue: [String: Any]) -> SDKAttributeValue? {
        guard let type = dataValue[MTRTypeKey] as? String else { return nil }
        switch type {
        case MTRBooleanValueType:
            if let n = dataValue[MTRValueKey] as? NSNumber { return .bool(n.boolValue) }
        case MTRSignedIntegerValueType, MTRUnsignedIntegerValueType:
            if let n = dataValue[MTRValueKey] as? NSNumber { return .int(n.int64Value) }
        case MTRFloatValueType, MTRDoubleValueType:
            if let n = dataValue[MTRValueKey] as? NSNumber { return .float(n.floatValue) }
        case MTRUTF8StringValueType:
            if let s = dataValue[MTRValueKey] as? String { return .string(s) }
        case MTROctetStringValueType:
            if let d = dataValue[MTRValueKey] as? Data { return .bytes(d) }
        default:
            return nil
        }
        return nil
    }

    /// Encode an SDKAttributeValue into a Matter data-value dictionary.
    private static func encode(_ value: SDKAttributeValue) -> [String: Any] {
        switch value {
        case .bool(let v):
            return [MTRTypeKey: MTRBooleanValueType, MTRValueKey: NSNumber(value: v)]
        case .int(let v):
            return [MTRTypeKey: MTRSignedIntegerValueType, MTRValueKey: NSNumber(value: v)]
        case .float(let v):
            return [MTRTypeKey: MTRFloatValueType, MTRValueKey: NSNumber(value: v)]
        case .string(let v):
            return [MTRTypeKey: MTRUTF8StringValueType, MTRValueKey: v]
        case .bytes(let v):
            return [MTRTypeKey: MTROctetStringValueType, MTRValueKey: v]
        }
    }
    #endif
}

#if canImport(Matter)

// MARK: - In-memory MTRStorage

/// Throwaway, RAM-backed implementation of MTRStorage for a development fabric.
final class MatterInMemoryStorage: NSObject, MTRStorage {
    private let lock = NSLock()
    private var store: [String: Data] = [:]

    func storageData(forKey key: String) -> Data? {
        lock.withLock { store[key] }
    }
    func setStorageData(_ value: Data, forKey key: String) -> Bool {
        lock.withLock { store[key] = value }; return true
    }
    func removeStorageData(forKey key: String) -> Bool {
        lock.withLock { store.removeValue(forKey: key) != nil }
    }
}

// MARK: - Operational signing keypair (CryptoKit P-256)

/// MTRKeypair backed by a freshly generated P-256 key. Signs operational
/// certificates for the local development fabric.
final class MatterTestKeypair: NSObject, MTRKeypair {
    private let privateKey = P256.Signing.PrivateKey()

    // Requires iOS 18.4+ (the modern, non-leaking keypair API). On 16.4–18.3 the
    // framework would call the deprecated `publicKey` instead; add that variant if
    // you need to deploy below 18.4.
    @available(iOS 18.4, *)
    func copyPublicKey() -> SecKey {
        let attrs: [String: Any] = [
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecAttrKeyClass as String: kSecAttrKeyClassPublic,
            kSecAttrKeySizeInBits as String: 256,
        ]
        let data = privateKey.publicKey.x963Representation as CFData
        return SecKeyCreateWithData(data, attrs as CFDictionary, nil)!
    }

    func signMessageECDSA_RAW(_ message: Data) -> Data {
        // CryptoKit hashes with SHA-256 internally for P-256 signing; Matter
        // expects the raw r||s representation.
        guard let sig = try? privateKey.signature(for: message) else { return Data() }
        return sig.rawRepresentation
    }
}

// MARK: - Commissioning delegate

/// Bridges MTRDeviceControllerDelegate callbacks to async continuations.
final class MatterCommissioningDelegate: NSObject, MTRDeviceControllerDelegate {
    var onSessionEstablished: ((Error?) -> Void)?
    var onCommissioningComplete: ((Error?) -> Void)?

    func controller(_ controller: MTRDeviceController,
                    commissioningSessionEstablishmentDone error: Error?) {
        let cb = onSessionEstablished; onSessionEstablished = nil
        cb?(error)
    }

    func controller(_ controller: MTRDeviceController,
                    commissioningComplete error: Error?, nodeID: NSNumber?) {
        let cb = onCommissioningComplete; onCommissioningComplete = nil
        cb?(error)
    }
}

#endif
