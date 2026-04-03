import Foundation
import Combine

/// Swift-side home manager backed by the C++ engine via MatterBridge
@MainActor
class HomeManager: ObservableObject {
    // Home devices (demo)
    @Published var devices: [DeviceInfo] = []
    @Published var rooms: [String] = []
    @Published var chatMessages: [ChatMessage] = []

    // Simulation state
    @Published var simNodes: [SimNodeInfo] = []
    @Published var simVans: [SimVanInfo] = []
    @Published var dashboardConnected = false
    @Published var gatewayConnected = false
    @Published var meshHealthy = false
    @Published var autoRefresh = false {
        didSet { autoRefresh ? startPolling() : stopPolling() }
    }

    private let bridge = MatterBridge()
    private let simService = SimulationService()
    private var pollTask: Task<Void, Never>?

    init() {
        bridge.loadDemoHome()
        refresh()
    }

    // MARK: - Home Devices

    func refresh() {
        let maDevices = bridge.allDevices()
        devices = maDevices.map { DeviceInfo(from: $0) }
        rooms = (bridge.roomNames() as [String]?) ?? []
    }

    func homeSummary() -> String {
        return bridge.homeSummary()
    }

    func roomSummary(_ room: String) -> String {
        return bridge.roomSummary(room)
    }

    func processQuery(_ text: String) -> String {
        // Check if this is a simulation query
        let lower = text.lowercased()
        let simKeywords = ["mesh", "node", "fleet", "van", "topology", "link",
                           "network", "simulation", "sim", "healthy", "health",
                           "alert", "metric", "traffic"]
        let isSimQuery = simKeywords.contains(where: { lower.contains($0) })

        if isSimQuery && dashboardConnected {
            return bridge.answerSimulationQuery(text)
        }

        let result = bridge.processNaturalLanguage(text)

        // Apply actions locally
        for action in result.actions {
            if action.command == "On" {
                bridge.updateAttribute(forNode: action.nodeId, endpoint: action.endpointId,
                                       cluster: 0x0006, attribute: 0x0000, boolValue: true)
            } else if action.command == "Off" {
                bridge.updateAttribute(forNode: action.nodeId, endpoint: action.endpointId,
                                       cluster: 0x0006, attribute: 0x0000, boolValue: false)
            }
        }

        refresh()
        return result.response
    }

    func sendMessage(_ text: String) {
        chatMessages.append(ChatMessage(text: text, isUser: true))
        let response = processQuery(text)
        chatMessages.append(ChatMessage(text: response, isUser: false))
    }

    func toggleDevice(_ device: DeviceInfo) {
        let newState = !device.isOn
        bridge.updateAttribute(forNode: device.nodeId, endpoint: 1,
                               cluster: 0x0006, attribute: 0x0000, boolValue: newState)
        refresh()
    }

    // MARK: - Simulation

    func refreshSimulation() {
        Task {
            await fetchSimulationData()
        }
    }

    func linkInfoForPair(_ src: Int, _ dst: Int) -> LinkInfoData? {
        guard let info = bridge.linkInfo(from: UInt16(src), to: UInt16(dst)) as? MALinkInfo else {
            return nil
        }
        return LinkInfoData(
            lossPercent: info.lossPercent,
            latencyMs: info.latencyMs,
            up: info.up,
            lqi: info.lqi,
            rssi: info.rssi
        )
    }

    func toggleVanLock(_ van: SimVanInfo) {
        Task {
            if van.locked {
                let _ = await simService.unlockVan(van.vanId)
            } else {
                let _ = await simService.lockVan(van.vanId)
            }
            await fetchSimulationData()
        }
    }

    // MARK: - Polling

    private func startPolling() {
        stopPolling()
        pollTask = Task {
            while !Task.isCancelled {
                await fetchSimulationData()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    private func stopPolling() {
        pollTask?.cancel()
        pollTask = nil
    }

    private func fetchSimulationData() async {
        // Dashboard
        if let status = await simService.fetchStatus() {
            bridge.updateSimulationNodes(status)
            dashboardConnected = true
        } else {
            dashboardConnected = false
        }

        if let topo = await simService.fetchTopology() {
            bridge.updateSimulationTopology(topo)
        }

        if let timeline = await simService.fetchTimeline() {
            bridge.updateSimulationTimeline(timeline)
        }

        if let metrics = await simService.fetchMetrics() {
            bridge.updateSimulationMetrics(metrics)
        }

        // Gateway
        if let vans = await simService.fetchVans() {
            bridge.updateSimulationVans(vans)
            gatewayConnected = true
        } else {
            gatewayConnected = false
        }

        if let alerts = await simService.fetchAlerts() {
            bridge.updateSimulationAlerts(alerts)
        }

        // Update published state
        let maNodes = bridge.simulationNodes() as? [MANodeState] ?? []
        simNodes = maNodes.map { SimNodeInfo(from: $0) }

        let maVans = bridge.simulationVans() as? [MAVanState] ?? []
        simVans = maVans.map { SimVanInfo(from: $0) }

        meshHealthy = bridge.isMeshHealthy()
    }
}

// MARK: - Data Models

struct DeviceInfo: Identifiable {
    let id: UInt64
    let nodeId: UInt64
    let name: String
    let room: String
    let vendor: String?
    let isOn: Bool
    let reachable: Bool
    let stateDescription: String
    let temperature: Float?
    let humidity: Float?
    let brightness: Int?
    let battery: Int?
    let isLocked: Bool
    let hasToggle: Bool

    init(from maDevice: MADevice) {
        self.id = maDevice.nodeId
        self.nodeId = maDevice.nodeId
        self.name = maDevice.name
        self.room = maDevice.room
        self.vendor = maDevice.vendorName
        self.isOn = maDevice.isOn
        self.reachable = maDevice.reachable
        self.stateDescription = maDevice.stateDescription
        self.temperature = maDevice.temperature?.floatValue
        self.humidity = maDevice.humidity?.floatValue
        self.brightness = maDevice.brightness?.intValue
        self.battery = maDevice.battery?.intValue
        self.isLocked = maDevice.isLocked
        self.hasToggle = maDevice.hasToggle
    }
}

struct ChatMessage: Identifiable {
    let id = UUID()
    let text: String
    let isUser: Bool
    let timestamp = Date()
}

struct SimNodeInfo: Identifiable {
    let id: UInt16
    let nodeId: UInt16
    let role: String
    let state: String
    let pid: Int
    let reachable: Bool

    init(from ma: MANodeState) {
        self.id = ma.nodeId
        self.nodeId = ma.nodeId
        self.role = ma.role
        self.state = ma.state
        self.pid = Int(ma.pid)
        self.reachable = ma.reachable
    }
}

struct SimVanInfo: Identifiable {
    let id: String
    let vanId: String
    let name: String
    let state: String
    let locked: Bool

    init(from ma: MAVanState) {
        self.id = ma.vanId
        self.vanId = ma.vanId
        self.name = ma.name
        self.state = ma.state
        self.locked = ma.locked
    }
}

struct LinkInfoData {
    let lossPercent: Float
    let latencyMs: Float
    let up: Bool
    let lqi: UInt8
    let rssi: Int8
}
