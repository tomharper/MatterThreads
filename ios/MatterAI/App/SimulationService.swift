import Foundation

/// Talks to MatterThreads Dashboard (8080) and Gateway (8090) APIs
actor SimulationService {
    private let dashboardBase: String
    private let gatewayBase: String
    private let session: URLSession

    init(host: String = "localhost", dashboardPort: Int = 8080, gatewayPort: Int = 8090) {
        self.dashboardBase = "http://\(host):\(dashboardPort)"
        self.gatewayBase = "http://\(host):\(gatewayPort)"

        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 5
        config.timeoutIntervalForResource = 10
        self.session = URLSession(configuration: config)
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
        do {
            let (data, response) = try await session.data(from: url)
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
        request.httpBody = body.data(using: .utf8)
        do {
            let (_, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse else { return false }
            return http.statusCode >= 200 && http.statusCode < 300
        } catch {
            return false
        }
    }
}
