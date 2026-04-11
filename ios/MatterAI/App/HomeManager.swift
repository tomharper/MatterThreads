import Foundation
import Combine

/// Swift-side simulation state manager backed by the C++ engine via MatterBridge.
///
/// Previously also handled home devices and chat; those responsibilities moved
/// to `MatterHomeSDK`. This class now exists solely to drive the mesh/fleet
/// simulation views (`SimulationView`) by polling the dashboard/gateway HTTP
/// endpoints and publishing the C++ engine's view of the mesh.
@MainActor
class HomeManager: ObservableObject {
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
    private var simService = SimulationService()
    private var pollTask: Task<Void, Never>?

    // MARK: - Simulation

    /// Reload simulation service URLs from persisted config
    /// (call after user edits dashboard/gateway URLs in Settings)
    func reloadSimConfig() {
        Task { await simService.reloadConfig() }
    }

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
