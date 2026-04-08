import Foundation
import Security

/// Minimal Keychain wrapper for SDK secrets (OAuth tokens, refresh tokens, fabric keys).
/// Items are stored as generic passwords scoped to the app's bundle.
enum SDKKeychain {
    private static let service = "com.matterthreads.matterai.sdk"

    static func set(_ value: String, for key: String) {
        guard let data = value.data(using: .utf8) else { return }
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key
        ]
        SecItemDelete(query as CFDictionary)

        var insert = query
        insert[kSecValueData as String] = data
        insert[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        SecItemAdd(insert as CFDictionary, nil)
    }

    static func get(_ key: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        guard status == errSecSuccess, let data = result as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    static func delete(_ key: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key
        ]
        SecItemDelete(query as CFDictionary)
    }
}

/// Persistent record of devices that have been commissioned through the SDK.
/// Backed by UserDefaults — survives app restart so we can re-attach to known devices.
struct CommissionedDeviceRecord: Codable, Equatable {
    let deviceId: String          // unified id, e.g. "Apple Matter:0xABCD"
    let source: String            // BackendSource.rawValue
    let nativeId: String          // backend-specific id
    let name: String
    let commissionedAt: Date
}

@MainActor
final class CommissionedDeviceStore: ObservableObject {
    @Published private(set) var records: [CommissionedDeviceRecord] = []

    private let key = "matterai.sdk.commissioned"
    private let defaults: UserDefaults

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        load()
    }

    func add(_ record: CommissionedDeviceRecord) {
        records.removeAll { $0.deviceId == record.deviceId }
        records.append(record)
        save()
    }

    func remove(deviceId: String) {
        records.removeAll { $0.deviceId == deviceId }
        save()
    }

    func contains(deviceId: String) -> Bool {
        records.contains { $0.deviceId == deviceId }
    }

    private func load() {
        guard let data = defaults.data(forKey: key),
              let decoded = try? JSONDecoder().decode([CommissionedDeviceRecord].self, from: data) else {
            return
        }
        records = decoded
    }

    private func save() {
        guard let data = try? JSONEncoder().encode(records) else { return }
        defaults.set(data, forKey: key)
    }
}
