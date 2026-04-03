import Foundation
#if canImport(HomeKit)
import HomeKit
#endif

/// Backend wrapping Apple HomeKit (HMHomeManager).
/// Bridges HMAccessory / HMService / HMCharacteristic to unified interface.
final class HomeKitBackend: NSObject, DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .homeKit

    var isAvailable: Bool {
        #if canImport(HomeKit)
        return true
        #else
        return false
        #endif
    }

    private(set) var isActive: Bool = false

    #if canImport(HomeKit)
    private var homeManager: HMHomeManager?
    private var discoveredDevices: [String: UnifiedDevice] = [:]
    private var discoveryCompletion: (() -> Void)?
    private let lock = NSLock()
    #endif

    func startDiscovery() async throws {
        #if canImport(HomeKit)
        return try await withCheckedThrowingContinuation { continuation in
            homeManager = HMHomeManager()
            discoveryCompletion = { [weak self] in
                self?.syncAccessories()
                self?.isActive = true
                continuation.resume()
            }
            homeManager?.delegate = self
        }
        #else
        throw BackendError.notAvailable(source)
        #endif
    }

    func stopDiscovery() async {
        #if canImport(HomeKit)
        homeManager?.delegate = nil
        homeManager = nil
        #endif
        isActive = false
    }

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        // HomeKit pairing happens through the Home app or HMAccessoryBrowser
        throw BackendError.commissioningFailed("Use Apple Home app to add HomeKit accessories")
    }

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        #if canImport(HomeKit)
        guard isActive else { throw BackendError.notConnected }

        guard let characteristic = findCharacteristic(deviceId: deviceId, path: path) else {
            throw BackendError.attributeNotFound(path)
        }

        return try await withCheckedThrowingContinuation { continuation in
            characteristic.readValue { error in
                if let error = error {
                    continuation.resume(throwing: error)
                    return
                }
                if let value = characteristic.value {
                    continuation.resume(returning: self.convertHKValue(value, type: characteristic.characteristicType))
                } else {
                    continuation.resume(throwing: BackendError.attributeNotFound(path))
                }
            }
        }
        #else
        throw BackendError.notConnected
        #endif
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        #if canImport(HomeKit)
        guard isActive else { throw BackendError.notConnected }

        guard let characteristic = findCharacteristic(deviceId: deviceId, path: path) else {
            throw BackendError.attributeNotFound(path)
        }

        let hkValue = convertToHKValue(value)
        return try await withCheckedThrowingContinuation { continuation in
            characteristic.writeValue(hkValue) { error in
                if let error = error {
                    continuation.resume(throwing: error)
                } else {
                    continuation.resume()
                }
            }
        }
        #else
        throw BackendError.notConnected
        #endif
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        // HomeKit doesn't have a direct command model — commands are done via
        // characteristic writes (e.g., writing LockTargetState to lock a door)
        // Map common Matter commands to HK characteristic writes
        if clusterId == 0x0006 { // OnOff
            let on = commandId == 0x0001
            try await writeAttribute(deviceId: deviceId, path: .onOff, value: .bool(on))
        }
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        return AsyncStream { continuation in
            #if canImport(HomeKit)
            // HMCharacteristic.enableNotification(true) triggers delegate callbacks
            // Real implementation would set up notification handlers
            #endif
            continuation.finish()
        }
    }

    func knownDevices() -> [UnifiedDevice] {
        #if canImport(HomeKit)
        lock.lock()
        let devices = Array(discoveredDevices.values)
        lock.unlock()
        return devices
        #else
        return []
        #endif
    }

    // MARK: - Internal

    #if canImport(HomeKit)
    private func syncAccessories() {
        guard let homes = homeManager?.homes else { return }
        var devices: [String: UnifiedDevice] = [:]

        for home in homes {
            for accessory in home.accessories {
                let deviceId = accessory.uniqueIdentifier.uuidString
                var attrs: [AttributePath: SDKAttributeValue] = [:]

                // Map HMServices to Matter-style attributes
                for service in accessory.services {
                    for char in service.characteristics {
                        if let path = mapHKCharacteristicToPath(char, service: service),
                           let value = char.value {
                            attrs[path] = convertHKValue(value, type: char.characteristicType)
                        }
                    }
                }

                let room = accessory.room?.name ?? home.name
                devices[deviceId] = UnifiedDevice(
                    id: "\(source.rawValue):\(deviceId)",
                    source: source,
                    nativeId: deviceId,
                    name: accessory.name,
                    room: room,
                    vendor: accessory.manufacturer ?? "",
                    deviceType: mapHKCategoryToDeviceType(accessory.category),
                    reachable: accessory.isReachable,
                    attributes: attrs,
                    lastUpdated: Date()
                )
            }
        }

        lock.lock()
        discoveredDevices = devices
        lock.unlock()
    }

    private func findCharacteristic(deviceId: String, path: AttributePath) -> HMCharacteristic? {
        guard let homes = homeManager?.homes else { return nil }
        for home in homes {
            for accessory in home.accessories where accessory.uniqueIdentifier.uuidString == deviceId {
                for service in accessory.services {
                    for char in service.characteristics {
                        if let charPath = mapHKCharacteristicToPath(char, service: service),
                           charPath == path {
                            return char
                        }
                    }
                }
            }
        }
        return nil
    }

    private func mapHKCharacteristicToPath(_ char: HMCharacteristic, service: HMService) -> AttributePath? {
        // Map common HomeKit characteristic types to Matter attribute paths
        switch char.characteristicType {
        case HMCharacteristicTypePowerState:
            return .onOff
        case HMCharacteristicTypeBrightness:
            return .currentLevel
        case HMCharacteristicTypeCurrentTemperature:
            return .measuredTemp
        case HMCharacteristicTypeCurrentRelativeHumidity:
            return .measuredHumidity
        case HMCharacteristicTypeLockMechanismCurrentState:
            return .lockState
        case HMCharacteristicTypeBatteryLevel:
            return .batteryRemaining
        case HMCharacteristicTypeColorTemperature:
            return .colorTemp
        case HMCharacteristicTypeOccupancyDetected:
            return .occupancy
        case HMCharacteristicTypeContactState:
            return .contactState
        default:
            return nil
        }
    }

    private func convertHKValue(_ value: Any, type: String) -> SDKAttributeValue {
        switch value {
        case let b as Bool: return .bool(b)
        case let n as NSNumber:
            switch type {
            case HMCharacteristicTypeCurrentTemperature:
                return .int(Int64(n.floatValue * 100))  // Convert to centidegrees
            case HMCharacteristicTypeBrightness:
                return .int(Int64(Float(n.intValue) / 100.0 * 254))  // HK uses 0-100, Matter 0-254
            case HMCharacteristicTypeBatteryLevel:
                return .int(Int64(n.intValue * 2))  // Matter uses 200=100%
            default:
                return .int(n.int64Value)
            }
        case let s as String: return .string(s)
        default: return .int(0)
        }
    }

    private func convertToHKValue(_ value: SDKAttributeValue) -> Any {
        switch value {
        case .bool(let v): return v
        case .int(let v): return NSNumber(value: v)
        case .float(let v): return NSNumber(value: v)
        case .string(let v): return v
        case .bytes: return NSNull()
        }
    }

    private func mapHKCategoryToDeviceType(_ category: HMAccessoryCategory) -> UInt32 {
        switch category.categoryType {
        case HMAccessoryCategoryTypeLightbulb: return 0x0100
        case HMAccessoryCategoryTypeDoorLock: return 0x000A
        case HMAccessoryCategoryTypeThermostat: return 0x0301
        case HMAccessoryCategoryTypeFan: return 0x002B
        case HMAccessoryCategoryTypeWindowCovering: return 0x0202
        case HMAccessoryCategoryTypeSwitch, HMAccessoryCategoryTypeOutlet: return 0x010A
        case HMAccessoryCategoryTypeSensor: return 0x0302
        default: return 0
        }
    }
    #endif
}

#if canImport(HomeKit)
extension HomeKitBackend: HMHomeManagerDelegate {
    func homeManagerDidUpdateHomes(_ manager: HMHomeManager) {
        discoveryCompletion?()
        discoveryCompletion = nil
        syncAccessories()
    }
}
#endif
