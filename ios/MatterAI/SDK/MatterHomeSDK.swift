import Foundation
import Combine

/// MatterHomeSDK — unified smart home integration SDK.
///
/// Wraps Apple Matter.framework, HomeKit, Google Home Device Access API,
/// and Thread diagnostics behind a single interface. The app layer never
/// talks to individual backends directly.
///
/// Usage:
/// ```swift
/// let sdk = MatterHomeSDK()
/// sdk.enableBackend(.homeKit)
/// sdk.enableBackend(.appleMatter)
/// await sdk.start()
///
/// for device in sdk.devices {
///     print("\(device.source): \(device.name) — \(device.stateDescription)")
/// }
/// ```
@MainActor
class MatterHomeSDK: ObservableObject {
    // MARK: - Published State

    /// All devices across all backends, merged and deduplicated
    @Published private(set) var devices: [UnifiedDevice] = []

    /// Which backends are currently active
    @Published private(set) var activeBackends: Set<BackendSource> = []

    /// Overall connectivity status
    @Published private(set) var isConnected: Bool = false

    // MARK: - Internal

    private let router = BackendRouter()
    private let bridge = MatterBridge()
    private var cancellables = Set<AnyCancellable>()

    // Backend instances (lazy, created when enabled)
    private var localBackend: LocalBackend?
    private var matterBackend: MatterBackend?
    private var homeKitBackend: HomeKitBackend?
    private var googleHomeBackend: GoogleHomeBackend?
    private var threadBackend: ThreadBackend?

    init() {
        // Observe router's device list
        router.$devices
            .receive(on: RunLoop.main)
            .assign(to: &$devices)

        router.$activeBackends
            .receive(on: RunLoop.main)
            .map { !$0.isEmpty }
            .assign(to: &$isConnected)

        router.$activeBackends
            .receive(on: RunLoop.main)
            .map { Set($0) }
            .assign(to: &$activeBackends)
    }

    // MARK: - Backend Configuration

    /// Enable the local (C++ demo) backend — always available
    func enableLocal() {
        let backend = LocalBackend(bridge: bridge)
        localBackend = backend
        router.register(backend)
    }

    /// Enable Apple Matter.framework backend
    func enableMatter() {
        let backend = MatterBackend()
        matterBackend = backend
        router.register(backend)
    }

    /// Enable Apple HomeKit backend
    func enableHomeKit() {
        let backend = HomeKitBackend()
        homeKitBackend = backend
        router.register(backend)
    }

    /// Enable Google Home backend with OAuth config
    func enableGoogleHome(projectId: String, clientId: String,
                          clientSecret: String, refreshToken: String? = nil) {
        let backend = GoogleHomeBackend()
        backend.configure(GoogleHomeBackend.GoogleHomeConfig(
            projectId: projectId,
            clientId: clientId,
            clientSecret: clientSecret,
            refreshToken: refreshToken
        ))
        googleHomeBackend = backend
        router.register(backend)
    }

    /// Enable Thread diagnostics backend
    func enableThread(dashboardURL: URL? = URL(string: "http://localhost:8080")) {
        let backend = ThreadBackend()
        backend.configure(dashboardURL: dashboardURL)
        threadBackend = backend
        router.register(backend)
    }

    /// Convenience: enable a backend by source type
    func enableBackend(_ source: BackendSource) {
        switch source {
        case .local: enableLocal()
        case .appleMatter: enableMatter()
        case .homeKit: enableHomeKit()
        case .googleHome: break  // Needs config, use enableGoogleHome()
        case .thread: enableThread()
        }
    }

    // MARK: - Lifecycle

    /// Start all enabled backends
    func start() async {
        await router.startAll()
    }

    /// Stop all backends
    func stop() async {
        await router.stopAll()
    }

    // MARK: - Device Operations

    /// Get all devices in a specific room
    func devicesInRoom(_ room: String) -> [UnifiedDevice] {
        devices.filter { $0.room == room }
    }

    /// Get all unique room names
    var rooms: [String] {
        Array(Set(devices.map(\.room))).sorted()
    }

    /// Get devices from a specific backend
    func devices(from source: BackendSource) -> [UnifiedDevice] {
        devices.filter { $0.source == source }
    }

    /// Toggle a device on/off
    func toggleDevice(_ device: UnifiedDevice) async throws {
        try await router.toggleDevice(device)
    }

    /// Read an attribute from a device
    func readAttribute(device: UnifiedDevice, path: AttributePath) async throws -> SDKAttributeValue {
        try await router.readAttribute(deviceId: device.id, path: path)
    }

    /// Write an attribute to a device
    func writeAttribute(device: UnifiedDevice, path: AttributePath, value: SDKAttributeValue) async throws {
        try await router.writeAttribute(deviceId: device.id, path: path, value: value)
    }

    /// Invoke a command on a device
    func invokeCommand(device: UnifiedDevice, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any] = [:]) async throws {
        try await router.invokeCommand(deviceId: device.id, endpointId: endpointId,
                                       clusterId: clusterId, commandId: commandId, payload: payload)
    }

    /// Subscribe to attribute changes on a device
    func subscribe(device: UnifiedDevice, paths: [AttributePath],
                   minInterval: TimeInterval = 1, maxInterval: TimeInterval = 60) {
        router.subscribeToDevice(device.id, paths: paths,
                                 minInterval: minInterval, maxInterval: maxInterval)
    }

    /// Commission a new device through a specific backend
    func commission(via source: BackendSource, deviceId: String, payload: String? = nil) async throws -> UnifiedDevice {
        try await router.commission(backend: source, deviceId: deviceId, payload: payload)
    }

    // MARK: - AI / NL Query

    /// Process a natural language query about the home
    func processQuery(_ text: String) -> String {
        return bridge.processNaturalLanguage(text).response
    }

    /// Get a summary of all devices
    func homeSummary() -> String {
        var lines: [String] = []
        for source in BackendSource.allCases {
            let sourceDevices = devices(from: source)
            if !sourceDevices.isEmpty {
                lines.append("[\(source.rawValue)] \(sourceDevices.count) device(s)")
                for d in sourceDevices {
                    lines.append("  \(d.name) (\(d.room)): \(d.stateDescription)")
                }
            }
        }
        if lines.isEmpty { return "No devices connected" }
        return lines.joined(separator: "\n")
    }

    // MARK: - Access to Bridge (for simulation features)

    /// Direct access to MatterBridge for simulation-specific features
    var matterBridge: MatterBridge { bridge }

    /// Refresh device list from all backends
    func refresh() {
        router.refreshDeviceList()
    }
}
