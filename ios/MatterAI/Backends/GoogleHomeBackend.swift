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
    private let session = URLSession.shared

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
    }

    func setAccessToken(_ token: String) {
        self.accessToken = token
    }

    // MARK: - DeviceBackend

    func startDiscovery() async throws {
        guard let config = config else {
            throw BackendError.notConnected
        }
        projectId = config.projectId

        // Refresh token if needed
        if accessToken == nil, let refreshToken = config.refreshToken {
            try await refreshAccessToken(refreshToken: refreshToken)
        }

        guard accessToken != nil else {
            throw BackendError.notConnected
        }

        // Fetch device list
        try await fetchDevices()
        isActive = true
    }

    func stopDiscovery() async {
        isActive = false
        lock.lock()
        discoveredDevices.removeAll()
        lock.unlock()
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

        let (data, response) = try await session.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, httpResponse.statusCode == 200 else {
            throw BackendError.deviceNotFound(deviceId)
        }

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

        let (_, response) = try await session.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, httpResponse.statusCode == 200 else {
            throw BackendError.writeFailure("Google API command failed")
        }
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
        lock.lock()
        let devices = Array(discoveredDevices.values)
        lock.unlock()
        return devices
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

        let (data, _) = try await session.data(for: request)
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let token = json["access_token"] as? String else {
            throw BackendError.notConnected
        }
        accessToken = token
    }

    private func fetchDevices() async throws {
        guard let token = accessToken else { return }

        let url = URL(string: "\(baseURL)/devices")!
        var request = URLRequest(url: url)
        request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")

        let (data, response) = try await session.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, httpResponse.statusCode == 200 else {
            throw BackendError.notConnected
        }

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
                reachable: true,
                attributes: attrs,
                lastUpdated: Date()
            )
        }

        lock.lock()
        discoveredDevices = devices
        lock.unlock()
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
        // Thermostat traits
        if let hvac = traits["sdm.devices.traits.ThermostatHvac"] as? [String: Any],
           let status = hvac["status"] as? String {
            attrs[.thermostatMode] = .int(status == "HEATING" ? 4 : status == "COOLING" ? 3 : 0)
        }
        if let temp = traits["sdm.devices.traits.Temperature"] as? [String: Any],
           let ambient = temp["ambientTemperatureCelsius"] as? Double {
            attrs[.localTemperature] = .int(Int64(ambient * 100))
        }
        if let humidity = traits["sdm.devices.traits.Humidity"] as? [String: Any],
           let pct = humidity["ambientHumidityPercent"] as? Double {
            attrs[.measuredHumidity] = .int(Int64(pct * 100))
        }
        // Connectivity
        if let conn = traits["sdm.devices.traits.Connectivity"] as? [String: Any],
           let status = conn["status"] as? String {
            // Map to reachable (handled at device level)
            _ = status == "ONLINE"
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
