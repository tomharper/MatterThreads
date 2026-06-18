import Foundation

/// Backend wrapping Google Home Device Access API.
/// Uses OAuth2 + REST to access Google Home devices via Smart Device Management API.
/// Reference: https://developers.google.com/home/matter
final class GoogleHomeBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .googleHome

    var isAvailable: Bool { true }  // REST-based, always available
    private(set) var isActive: Bool = false

    private var accessToken: String?
    private var projectId: String?
    private var discoveredDevices: [String: UnifiedDevice] = [:]
    private let lock = NSLock()
    private let session: URLSession = {
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 15
        cfg.timeoutIntervalForResource = 30
        cfg.waitsForConnectivity = false
        return URLSession(configuration: cfg)
    }()

    /// Send a request, mapping transport failures and HTTP status codes to clear,
    /// user-facing errors instead of leaking raw URLErrors or silent nil-parses.
    private func send(_ request: URLRequest, context: String) async throws -> Data {
        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await session.data(for: request)
        } catch let urlError as URLError {
            switch urlError.code {
            case .notConnectedToInternet, .networkConnectionLost, .cannotConnectToHost, .dataNotAllowed:
                throw BackendError.deviceUnreachable("No internet connection — couldn't reach Google Home.")
            case .timedOut:
                throw BackendError.deviceUnreachable("Google Home request timed out (\(context)). Check your connection and try again.")
            case .cannotFindHost, .dnsLookupFailed:
                throw BackendError.deviceUnreachable("Couldn't reach Google's servers (\(context)).")
            default:
                throw BackendError.deviceUnreachable("Network error talking to Google Home: \(urlError.localizedDescription)")
            }
        }
        guard let http = response as? HTTPURLResponse else {
            throw BackendError.deviceUnreachable("Unexpected response from Google Home (\(context)).")
        }
        switch http.statusCode {
        case 200...299:
            return data
        case 401, 403:
            accessToken = nil
            throw BackendError.notConfigured("Google authorization expired or is invalid — reconnect Google Home in Settings.")
        case 404:
            throw BackendError.deviceNotFound(context)
        case 429:
            throw BackendError.deviceUnreachable("Google Home is rate-limiting requests — try again shortly.")
        default:
            let body = String(data: data, encoding: .utf8)?.prefix(140) ?? ""
            throw BackendError.commandFailure("Google Home \(context) failed (HTTP \(http.statusCode)) \(body)")
        }
    }

    // Google SDM API base URL
    private var baseURL: String {
        "https://smartdevicemanagement.googleapis.com/v1/enterprises/\(projectId ?? "")"
    }

    struct GoogleHomeConfig {
        let projectId: String
        let clientId: String
        let clientSecret: String
        let refreshToken: String?
    }

    private var config: GoogleHomeConfig?

    func configure(_ config: GoogleHomeConfig) {
        self.config = config
        self.projectId = config.projectId
        // Persist refresh token to Keychain so it survives restart
        if let rt = config.refreshToken {
            SDKKeychain.set(rt, for: "google.refreshToken")
        }
        SDKKeychain.set(config.clientId, for: "google.clientId")
        SDKKeychain.set(config.clientSecret, for: "google.clientSecret")
        SDKKeychain.set(config.projectId, for: "google.projectId")
    }

    /// Restore configuration from Keychain (call on startup before startDiscovery)
    func restoreFromKeychain() {
        guard let projectId = SDKKeychain.get("google.projectId"),
              let clientId = SDKKeychain.get("google.clientId"),
              let clientSecret = SDKKeychain.get("google.clientSecret") else {
            return
        }
        self.config = GoogleHomeConfig(
            projectId: projectId,
            clientId: clientId,
            clientSecret: clientSecret,
            refreshToken: SDKKeychain.get("google.refreshToken")
        )
        self.projectId = projectId
    }

    func setAccessToken(_ token: String) {
        self.accessToken = token
        SDKKeychain.set(token, for: "google.accessToken")
    }

    // MARK: - DeviceBackend

    func startDiscovery() async throws {
        guard let config = config else {
            throw BackendError.notConfigured("Google Home isn't set up — add your Device Access project and OAuth credentials in Settings → Google Home OAuth.")
        }
        projectId = config.projectId

        // Refresh token if needed
        if accessToken == nil {
            guard let refreshToken = config.refreshToken else {
                throw BackendError.notConfigured("Google Home is missing a refresh token — reconnect in Settings → Google Home OAuth.")
            }
            try await refreshAccessToken(refreshToken: refreshToken)
        }

        guard accessToken != nil else {
            throw BackendError.notConfigured("Couldn't obtain a Google access token — reconnect in Settings.")
        }

        // Fetch device list
        try await fetchDevices()
        isActive = true
    }

    func stopDiscovery() async {
        isActive = false
        lock.withLock { discoveredDevices.removeAll() }
    }

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        // Google Home commissioning happens through the Google Home app
        throw BackendError.commissioningFailed("Use Google Home app to add devices")
    }

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        guard isActive, let token = accessToken else { throw BackendError.notConnected }

        // SDM API: GET /devices/{deviceId}
        let url = URL(string: "\(baseURL)/devices/\(deviceId)")!
        var request = URLRequest(url: url)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")

        let data = try await send(request, context: "read \(deviceId)")
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let traits = json["traits"] as? [String: Any] else {
            throw BackendError.attributeNotFound(path)
        }

        return try mapGoogleTraitToAttribute(traits: traits, path: path)
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        guard isActive, let token = accessToken else { throw BackendError.notConnected }

        // Map attribute write to SDM command
        let (command, params) = try mapAttributeToGoogleCommand(path: path, value: value)

        let url = URL(string: "\(baseURL)/devices/\(deviceId):executeCommand")!
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let body: [String: Any] = [
            "command": command,
            "params": params
        ]
        request.httpBody = try JSONSerialization.data(withJSONObject: body)

        _ = try await send(request, context: "command \(command)")
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        // Map Matter commands to Google SDM commands
        if clusterId == 0x0006 { // OnOff
            let on = commandId == 0x0001
            try await writeAttribute(deviceId: deviceId, path: .onOff, value: .bool(on))
        }
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        // Google SDM uses pub/sub for events — would need Google Cloud Pub/Sub setup
        return AsyncStream { continuation in
            let task = Task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(maxInterval))
                    guard !Task.isCancelled else { break }
                    // Poll device state
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
        lock.withLock { Array(discoveredDevices.values) }
    }

    // MARK: - Internal

    private func refreshAccessToken(refreshToken: String) async throws {
        guard let config = config else { return }

        let url = URL(string: "https://oauth2.googleapis.com/token")!
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")

        let body = "client_id=\(config.clientId)&client_secret=\(config.clientSecret)&refresh_token=\(refreshToken)&grant_type=refresh_token"
        request.httpBody = body.data(using: .utf8)

        let data = try await send(request, context: "OAuth token refresh")
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let token = json["access_token"] as? String else {
            throw BackendError.notConfigured("Google rejected the refresh token — reconnect Google Home in Settings.")
        }
        accessToken = token
    }

    private func fetchDevices() async throws {
        guard let token = accessToken else { return }

        let url = URL(string: "\(baseURL)/devices")!
        var request = URLRequest(url: url)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")

        let data = try await send(request, context: "list devices")
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let deviceList = json["devices"] as? [[String: Any]] else {
            return
        }

        var devices: [String: UnifiedDevice] = [:]
        for deviceJson in deviceList {
            guard let name = deviceJson["name"] as? String else { continue }
            let deviceId = name.components(separatedBy: "/").last ?? name
            let traits = deviceJson["traits"] as? [String: Any] ?? [:]
            let parentRelations = deviceJson["parentRelations"] as? [[String: Any]] ?? []
            let room = parentRelations.first?["displayName"] as? String ?? "Unknown"
            let displayName = (traits["sdm.devices.traits.Info"] as? [String: Any])?["customName"] as? String ?? deviceId

            let type = deviceJson["type"] as? String ?? ""
            let deviceType = mapGoogleTypeToMatterDeviceType(type)

            var attrs: [AttributePath: SDKAttributeValue] = [:]
            mapGoogleTraitsToAttributes(traits: traits, into: &attrs)

            devices[deviceId] = UnifiedDevice(
                id: "\(source.rawValue):\(deviceId)",
                source: source,
                nativeId: deviceId,
                name: displayName,
                room: room,
                vendor: "Google",
                deviceType: deviceType,
                reachable: isOnline(traits: traits),
                attributes: attrs,
                lastUpdated: Date()
            )
        }

        lock.withLock { discoveredDevices = devices }
    }

    private func mapGoogleTypeToMatterDeviceType(_ type: String) -> UInt32 {
        if type.contains("THERMOSTAT") { return 0x0301 }
        if type.contains("CAMERA") { return 0 }
        if type.contains("DOORBELL") { return 0 }
        if type.contains("DISPLAY") { return 0x0022 }
        if type.contains("LIGHT") { return 0x0100 }
        if type.contains("LOCK") { return 0x000A }
        return 0
    }

    private func mapGoogleTraitsToAttributes(traits: [String: Any], into attrs: inout [AttributePath: SDKAttributeValue]) {
        // Thermostat operating state (HVAC) — what it's actively doing now.
        if let hvac = traits["sdm.devices.traits.ThermostatHvac"] as? [String: Any],
           let status = hvac["status"] as? String {
            attrs[.thermostatMode] = .int(status == "HEATING" ? 4 : status == "COOLING" ? 3 : 0)
        }
        // Configured thermostat mode (HEAT/COOL/HEATCOOL/OFF) — overrides HVAC if set.
        if let modeTrait = traits["sdm.devices.traits.ThermostatMode"] as? [String: Any],
           let mode = modeTrait["mode"] as? String {
            attrs[.thermostatMode] = .int(Self.matterMode(forSDM: mode))
        }
        // Temperature setpoints.
        if let setpoint = traits["sdm.devices.traits.ThermostatTemperatureSetpoint"] as? [String: Any] {
            if let heat = setpoint["heatCelsius"] as? Double {
                attrs[.heatingSetpoint] = .int(Int64(heat * 100))
            }
            if let cool = setpoint["coolCelsius"] as? Double {
                attrs[.coolingSetpoint] = .int(Int64(cool * 100))
            }
        }
        // Ambient temperature.
        if let temp = traits["sdm.devices.traits.Temperature"] as? [String: Any],
           let ambient = temp["ambientTemperatureCelsius"] as? Double {
            attrs[.localTemperature] = .int(Int64(ambient * 100))
            attrs[.measuredTemp] = .int(Int64(ambient * 100))
        }
        // Ambient humidity.
        if let humidity = traits["sdm.devices.traits.Humidity"] as? [String: Any],
           let pct = humidity["ambientHumidityPercent"] as? Double {
            attrs[.measuredHumidity] = .int(Int64(pct * 100))
        }
    }

    /// True if the device's Connectivity trait reports ONLINE (defaults to true
    /// when the trait is absent, e.g. wired devices that don't report it).
    private func isOnline(traits: [String: Any]) -> Bool {
        guard let conn = traits["sdm.devices.traits.Connectivity"] as? [String: Any],
              let status = conn["status"] as? String else { return true }
        return status == "ONLINE"
    }

    /// Map an SDM thermostat mode string to the Matter ThermostatMode enum value.
    private static func matterMode(forSDM mode: String) -> Int64 {
        switch mode {
        case "HEAT": return 4
        case "COOL": return 3
        case "HEATCOOL": return 1
        default: return 0   // OFF
        }
    }

    private func mapGoogleTraitToAttribute(traits: [String: Any], path: AttributePath) throws -> SDKAttributeValue {
        var attrs: [AttributePath: SDKAttributeValue] = [:]
        mapGoogleTraitsToAttributes(traits: traits, into: &attrs)
        guard let value = attrs[path] else {
            throw BackendError.attributeNotFound(path)
        }
        return value
    }

    private func mapAttributeToGoogleCommand(path: AttributePath, value: SDKAttributeValue) throws -> (String, [String: Any]) {
        // Map Matter attribute writes to Google SDM commands
        if path == .thermostatMode {
            guard let mode = value.intValue else {
                throw BackendError.writeFailure("Invalid thermostat mode")
            }
            let sdmMode: String
            switch mode {
            case 0: sdmMode = "OFF"
            case 1: sdmMode = "HEATCOOL"
            case 3: sdmMode = "COOL"
            case 4: sdmMode = "HEAT"
            default: sdmMode = "OFF"
            }
            return ("sdm.devices.commands.ThermostatMode.SetMode", ["mode": sdmMode])
        }

        if path == .heatingSetpoint || path == .coolingSetpoint {
            guard let temp = value.intValue else {
                throw BackendError.writeFailure("Invalid temperature")
            }
            let celsius = Double(temp) / 100.0
            if path == .heatingSetpoint {
                return ("sdm.devices.commands.ThermostatTemperatureSetpoint.SetHeat", ["heatCelsius": celsius])
            } else {
                return ("sdm.devices.commands.ThermostatTemperatureSetpoint.SetCool", ["coolCelsius": celsius])
            }
        }

        throw BackendError.writeFailure("Attribute not mappable to Google command")
    }
}
