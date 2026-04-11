import Foundation
import Combine

/// A single message in the assistant chat history.
struct SDKChatMessage: Identifiable, Equatable {
    let id = UUID()
    let text: String
    let isUser: Bool
    let timestamp = Date()
}

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

    /// Event stream for real-time device lifecycle events
    let eventStream = DeviceEventStream()

    /// Persistent store of devices commissioned through the SDK
    let commissionedStore = CommissionedDeviceStore()

    /// Conversation history for the assistant chat
    @Published private(set) var chatHistory: [SDKChatMessage] = []

    /// True while processing an NL query (so the UI can show a typing indicator)
    @Published private(set) var isProcessingQuery: Bool = false

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

        // Forward attribute updates from subscriptions into the event stream
        router.attributeUpdateSink = { [weak self] device, update in
            guard let self else { return }
            self.eventStream.emit(DeviceEvent(
                timestamp: update.timestamp,
                deviceId: device.id,
                deviceName: device.name,
                source: device.source,
                type: .attributeChanged,
                detail: "\(self.shortPath(update.path)) → \(update.value.displayString)"
            ))
        }
    }

    private func shortPath(_ p: AttributePath) -> String {
        if p == .onOff { return "OnOff" }
        if p == .currentLevel { return "Level" }
        if p == .measuredTemp { return "Temp" }
        if p == .measuredHumidity { return "Humidity" }
        if p == .lockState { return "Lock" }
        if p == .batteryRemaining { return "Battery" }
        if p == .colorTemp { return "ColorTemp" }
        if p == .occupancy { return "Occupancy" }
        if p == .contactState { return "Contact" }
        return String(format: "ep%d/0x%04X/0x%04X", p.endpointId, p.clusterId, p.attributeId)
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
        case .googleHome:
            // Try to restore previously persisted config from Keychain
            let backend = GoogleHomeBackend()
            backend.restoreFromKeychain()
            googleHomeBackend = backend
            router.register(backend)
        case .thread: enableThread()
        }
    }

    // MARK: - Lifecycle

    /// Start all enabled backends
    func start() async {
        await router.startAll()
        // Emit discovery events for initial devices
        for device in router.devices {
            eventStream.emit(DeviceEvent(
                timestamp: Date(), deviceId: device.id,
                deviceName: device.name, source: device.source,
                type: .discovered, detail: device.stateDescription
            ))
        }
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
        eventStream.emit(DeviceEvent(
            timestamp: Date(), deviceId: device.id,
            deviceName: device.name, source: device.source,
            type: .stateChanged, detail: device.isOn ? "Off" : "On"
        ))
    }

    /// Read an attribute from a device
    func readAttribute(device: UnifiedDevice, path: AttributePath) async throws -> SDKAttributeValue {
        try await router.readAttribute(deviceId: device.id, path: path)
    }

    /// Write an attribute to a device
    func writeAttribute(device: UnifiedDevice, path: AttributePath, value: SDKAttributeValue) async throws {
        try await router.writeAttribute(deviceId: device.id, path: path, value: value)
        eventStream.emit(DeviceEvent(
            timestamp: Date(), deviceId: device.id,
            deviceName: device.name, source: device.source,
            type: .attributeWrite,
            detail: "ep\(path.endpointId)/0x\(String(format: "%04X", path.clusterId))/0x\(String(format: "%04X", path.attributeId))"
        ))
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
        let device = try await router.commission(backend: source, deviceId: deviceId, payload: payload)
        commissionedStore.add(CommissionedDeviceRecord(
            deviceId: device.id,
            source: device.source.rawValue,
            nativeId: device.nativeId,
            name: device.name,
            commissionedAt: Date()
        ))
        eventStream.emit(DeviceEvent(
            timestamp: Date(), deviceId: device.id,
            deviceName: device.name, source: device.source,
            type: .commissioned, detail: "via \(source.rawValue)"
        ))
        return device
    }

    // MARK: - AI / NL Query

    /// Process a natural language query about the home (synchronous, C++ side only)
    func processQuery(_ text: String) -> String {
        return bridge.processNaturalLanguage(text).response
    }

    /// Send a user message and get an assistant reply.
    /// Routes simple intents across all backends (e.g. "turn off all lights")
    /// and falls back to the C++ NL processor for general queries.
    @discardableResult
    func ask(_ text: String) async -> String {
        chatHistory.append(SDKChatMessage(text: text, isUser: true))
        isProcessingQuery = true
        defer { isProcessingQuery = false }

        let lower = text.lowercased()
        var reply: String

        // Cross-backend intents
        if lower.contains("all off") || lower.contains("turn everything off") || lower.contains("turn off all") {
            let n = await applyToAllOnOff(turnOn: false)
            reply = "Turned off \(n) device(s) across \(activeBackends.count) backend(s)."
        } else if lower.contains("all on") || lower.contains("turn everything on") || lower.contains("turn on all") {
            let n = await applyToAllOnOff(turnOn: true)
            reply = "Turned on \(n) device(s)."
        } else if lower.contains("status") || lower.contains("summary") || lower.contains("what's on") || lower.contains("whats on") {
            reply = homeSummary()
        } else if lower.contains("temperature") || lower.contains("temperatures") {
            reply = temperatureSummary()
        } else if lower.contains("backend") || lower.contains("integration") {
            reply = backendSummary()
        } else if isSimulationQuery(lower) {
            // Route mesh/fleet queries to the C++ simulation engine
            reply = bridge.answerSimulationQuery(text)
            if reply.isEmpty {
                reply = "Simulation data unavailable — is the dashboard running?"
            }
        } else {
            // Fallback to C++ NL processor + augment with cross-backend stats
            let coreReply = bridge.processNaturalLanguage(text).response
            reply = coreReply
            if activeBackends.count > 1 {
                reply += "\n\n(Searched \(activeBackends.count) backends, \(devices.count) total device(s).)"
            }
        }

        chatHistory.append(SDKChatMessage(text: reply, isUser: false))
        return reply
    }

    /// Clear the chat history
    func clearChat() {
        chatHistory.removeAll()
    }

    private static let simKeywords = [
        "mesh", "node", "fleet", "van", "topology", "link",
        "network", "simulation", "sim", "healthy", "health",
        "alert", "metric", "traffic", "broker", "router",
        "leader", "thread mesh", "packet", "latency"
    ]

    private func isSimulationQuery(_ lower: String) -> Bool {
        Self.simKeywords.contains { lower.contains($0) }
    }

    // MARK: - Cross-backend intent helpers

    private func applyToAllOnOff(turnOn: Bool) async -> Int {
        var count = 0
        for d in devices where d.hasToggle && d.reachable && d.isOn != turnOn {
            do {
                try await router.writeAttribute(deviceId: d.id, path: .onOff, value: .bool(turnOn))
                count += 1
                eventStream.emit(DeviceEvent(
                    timestamp: Date(), deviceId: d.id, deviceName: d.name,
                    source: d.source, type: .stateChanged,
                    detail: turnOn ? "On (bulk)" : "Off (bulk)"
                ))
            } catch {
                // Skip failures, continue with the rest
            }
        }
        return count
    }

    private func temperatureSummary() -> String {
        let temps = devices.compactMap { d -> (String, Double)? in
            guard case .int(let centi) = d.attributes[.measuredTemp] else { return nil }
            return (d.name, Double(centi) / 100.0)
        }
        if temps.isEmpty { return "No temperature sensors found." }
        return temps.map { "\($0.0): \(String(format: "%.1f", $0.1))°C" }.joined(separator: "\n")
    }

    private func backendSummary() -> String {
        var lines = ["Active backends: \(activeBackends.count)"]
        for source in BackendSource.allCases {
            let n = devices(from: source).count
            let status = activeBackends.contains(source) ? "✓" : "·"
            lines.append("  \(status) \(source.rawValue): \(n) device(s)")
        }
        return lines.joined(separator: "\n")
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
