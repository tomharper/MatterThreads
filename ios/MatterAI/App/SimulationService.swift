import Foundation

/// Talks to MatterThreads Dashboard and Gateway APIs.
///
/// URLs are read from `SimulationConfig` (UserDefaults-backed) so they can
/// be changed at runtime in SettingsView — pointing at localhost for simulator
/// dev or at a Railway relay for physical-device testing.
actor SimulationService {
    private var dashboardBase: String
    private var gatewayBase: String
    private var authToken: String?
    private let session: URLSession

    init() {
        self.dashboardBase = Self.readDashboardURL()
        self.gatewayBase = Self.readGatewayURL()
        let token = SDKKeychain.get("sim.relayToken") ?? ""
        self.authToken = token.isEmpty ? nil : token

        let urlConfig = URLSessionConfiguration.default
        urlConfig.timeoutIntervalForRequest = 5
        urlConfig.timeoutIntervalForResource = 10
        self.session = URLSession(configuration: urlConfig)
    }

    /// Reload URLs from persisted config (call after user changes settings)
    func reloadConfig() {
        dashboardBase = Self.readDashboardURL()
        gatewayBase = Self.readGatewayURL()
        let token = SDKKeychain.get("sim.relayToken") ?? ""
        authToken = token.isEmpty ? nil : token
    }

    private static func readDashboardURL() -> String {
        UserDefaults.standard.string(forKey: "sim.dashboardURL") ?? "http://localhost:8080"
    }

    private static func readGatewayURL() -> String {
        UserDefaults.standard.string(forKey: "sim.gatewayURL") ?? "http://localhost:8090"
    }

    // MARK: - Dashboard API

    func fetchStatus() async -> String? {
        return await get("\(dashboardBase)/api/status")
    }

    func fetchTopology() async -> String? {
        return await get("\(dashboardBase)/api/topology")
    }

    func fetchTimeline() async -> String? {
        return await get("\(dashboardBase)/api/timeline")
    }

    func fetchMetrics() async -> String? {
        return await get("\(dashboardBase)/api/metrics")
    }

    // MARK: - Gateway API

    func fetchVans() async -> String? {
        return await get("\(gatewayBase)/api/vans")
    }

    func fetchFleetStatus() async -> String? {
        return await get("\(gatewayBase)/api/fleet/status")
    }

    func fetchAlerts() async -> String? {
        return await get("\(gatewayBase)/api/fleet/alerts")
    }

    func fetchHealth() async -> Bool {
        guard let _ = await get("\(gatewayBase)/api/health") else { return false }
        return true
    }

    // MARK: - Commands

    func lockVan(_ vanId: String) async -> Bool {
        return await post("\(gatewayBase)/api/vans/\(vanId)/lock")
    }

    func unlockVan(_ vanId: String) async -> Bool {
        return await post("\(gatewayBase)/api/vans/\(vanId)/unlock")
    }

    func invokeCommand(vanId: String, endpoint: Int, cluster: Int, command: String, payload: String = "{}") async -> Bool {
        let url = "\(gatewayBase)/api/vans/\(vanId)/endpoints/\(endpoint)/clusters/\(cluster)/commands/\(command)"
        return await post(url, body: payload)
    }

    // MARK: - Connection Test

    func testDashboardConnection() async -> Bool {
        guard let _ = await get("\(dashboardBase)/api/status") else { return false }
        return true
    }

    func testGatewayConnection() async -> Bool {
        return await fetchHealth()
    }

    // MARK: - Private

    private func get(_ urlString: String) async -> String? {
        guard let url = URL(string: urlString) else { return nil }
        var request = URLRequest(url: url)
        if let token = authToken {
            request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
                return nil
            }
            return String(data: data, encoding: .utf8)
        } catch {
            return nil
        }
    }

    private func post(_ urlString: String, body: String = "{}") async -> Bool {
        guard let url = URL(string: urlString) else { return false }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        if let token = authToken {
            request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        do {
            let (_, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse else { return false }
            return http.statusCode >= 200 && http.statusCode < 300
        } catch {
            return false
        }
    }
}

// MARK: - Persisted Config

/// UserDefaults-backed config for dashboard/gateway URLs and relay auth token.
/// Survives app restart. Edited from SettingsView.
@MainActor
class SimulationConfig: ObservableObject {
    static let shared = SimulationConfig()

    @Published var dashboardURL: String {
        didSet { UserDefaults.standard.set(dashboardURL, forKey: "sim.dashboardURL") }
    }
    @Published var gatewayURL: String {
        didSet { UserDefaults.standard.set(gatewayURL, forKey: "sim.gatewayURL") }
    }
    @Published var relayToken: String {
        didSet { SDKKeychain.set(relayToken, for: "sim.relayToken") }
    }

    private init() {
        self.dashboardURL = UserDefaults.standard.string(forKey: "sim.dashboardURL")
            ?? "http://localhost:8080"
        self.gatewayURL = UserDefaults.standard.string(forKey: "sim.gatewayURL")
            ?? "http://localhost:8090"
        self.relayToken = SDKKeychain.get("sim.relayToken") ?? ""
    }

    /// Reset to localhost defaults
    func resetToLocal() {
        dashboardURL = "http://localhost:8080"
        gatewayURL = "http://localhost:8090"
        relayToken = ""
    }
}
