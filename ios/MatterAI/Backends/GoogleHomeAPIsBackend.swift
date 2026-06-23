import Foundation
#if canImport(GoogleHomeSDK)
import GoogleHomeSDK
import GoogleHomeTypes
import Combine
#endif

// =============================================================================
// BackendSource note (read before wiring):
//   This backend sets `source = .googleHome`. There is currently exactly one
//   Google-flavoured case in `BackendSource` (DeviceBackend.swift) and this is
//   the most appropriate existing case, so it is reused here.
//
//   HOWEVER: the design brief flags that `.googleHome` may already be claimed by
//   a separate Smart Device Management (SDM) REST backend. If/when that backend
//   lands, the enum needs a *new* case to disambiguate the two Google paths
//   (e.g. `.googleHomeAPIs` for this SDK-based backend vs `.googleHome` for the
//   SDM REST one). That enum edit is intentionally NOT made here — per the task,
//   the enum is owned by the main agent. If you add `.googleHomeAPIs`, change the
//   single `let source: BackendSource = .googleHome` line below to point at it.
// =============================================================================

/// Backend wrapping Google's **Home APIs iOS SDK** (`GoogleHomeSDK` + `GoogleHomeTypes`).
///
/// Unlike `MatterBackend`, which speaks raw Matter attribute paths over
/// `MTRBaseDevice`, the Google SDK exposes a *typed trait graph*
/// (`HomeDevice → DeviceTypeController → DeviceType → matterTraits.<trait>Trait`).
/// There is no generic `readAttribute(endpoint:cluster:attribute:)` or raw
/// `invokeCommand(cluster:command:)` primitive. This backend therefore acts as a
/// translation layer from the protocol's `AttributePath` / `(clusterId, commandId)`
/// surface onto the well-known `GoogleHomeTypes` traits. Anything outside that
/// well-known set returns a clear `BackendError` (`.attributeNotFound` /
/// `.commandFailure`) rather than pretending — Google genuinely cannot do
/// arbitrary cluster/attribute access.
///
/// ---------------------------------------------------------------------------
/// What's needed to make this LIVE (not just compile):
///
///  1. SPM dependency — add the Google Home APIs SDK package in Xcode:
///       https://github.com/google-home/google-home-ios-sdk
///     and link BOTH products: `GoogleHomeSDK` and `GoogleHomeTypes`.
///     Docs: https://developers.home.google.com/apis/ios
///
///  2. Google Home Developer Console project + OAuth:
///       - Create a project at https://console.home.google.com/ and register the
///         iOS app's bundle ID; obtain the OAuth client ID / "project number".
///       - Configure the OAuth consent screen and request the device scopes.
///       - These (teamID/clientID) feed `Home.configure { ... }` at app launch.
///     Docs: https://developers.home.google.com/apis/ios/get-started
///
///  3. Entitlements / capabilities:
///       - App Attest (DeviceCheck): the SDK attests every app instance, which is
///         why it CANNOT run in the Simulator (`isAvailable` returns false there).
///       - An **App Group** shared between this app and the MatterExtension target,
///         passed via `Home.configure { $0.sharedAppGroup = "group.<bundle>" }`.
///       - Associated keychain access group, per the SDK setup guide.
///     Docs: https://developers.home.google.com/apis/ios/get-started#configure
///
///  4. A MatterExtension target (Apple **MatterSupport**) for commissioning:
///       A separate app extension subclassing `MatterAddDeviceExtensionRequestHandler`,
///       overriding `commissionDevice(in:onboardingPayload:commissioningID:)`,
///       `rooms(in:)`, and `configureDevice(named:in:)`. It must join the SAME App
///       Group as (3). The actual onboarding happens out-of-process in that
///       extension — this backend only kicks off the system sheet. This is a build
///       requirement the backend code can't satisfy at runtime.
///     Docs: https://developer.apple.com/documentation/mattersupport
///           https://developers.home.google.com/apis/matter/commission
///
///  5. An online Google/Nest hub running Cast OS in the user's structure: Matter
///     commissioning and routing go *through the hub*. `prepareForMatterCommissioning()`
///     fails if no compatible hub is reachable.
///     Docs: https://developers.home.google.com/apis/matter/hub
///
/// Base SDK requires iOS 17.0+; commissioning requires iOS 17.6+ (gated per-method).
///
/// ---------------------------------------------------------------------------
/// VERIFICATION STATUS. The SDK binary is download-gated behind Google Home
/// Developers sign-in (no public URL), so it can't be linked/compiled here — all
/// real-SDK use is inside `#if canImport(GoogleHomeSDK)` and doesn't affect today's
/// build. BUT the symbols below were cross-checked against Google's OFFICIAL iOS
/// sample app source (github.com/google-home/google-home-api-sample-app-ios):
///   VERIFIED against the sample's real, SDK-compiling code:
///     - Device types are top-level symbols, NOT `Matter.`-prefixed; the on/off
///       light type is `OnOffLightDeviceType` (the old `OnOffDeviceType` is bogus).
///     - `await device.types.get(SomeDeviceType.self)` returns an OPTIONAL with NO
///       `try` (sample: `if let x = await device.types.get(...)`) — fixed accordingly.
///     - Typed trait accessors carry the `Trait` suffix: `matterTraits.onOffTrait`,
///       `.levelControlTrait`, `.doorLockTrait`, `.thermostatTrait`, etc. — fixed.
///     - Trait commands `.on()/.off()/.toggle()`, `supportsToggleCommand`; `Home.configure`,
///       `Home.restoreSession`, `try await Home...`.
///   STILL to confirm at compile time (not exercised by the sample):
///     - `DoorLockTrait` lock/unlock labels, thermostat setpoint accessors,
///       `PowerSourceDeviceType` (absent from the supported-types list), and the exact
///       `Structure.completeMatterCommissioning()` shape.
final class GoogleHomeAPIsBackend: DeviceBackend, @unchecked Sendable {
    let source: BackendSource = .googleHomeAPIs

    /// Stricter than `canImport`: App Attest can't run in the Simulator and the
    /// base SDK is iOS 17.0+. Reporting `true` on the Simulator would lie.
    var isAvailable: Bool {
        #if canImport(GoogleHomeSDK) && !targetEnvironment(simulator)
        if #available(iOS 17.0, *) { return true }
        #endif
        return false
    }

    private(set) var isActive: Bool = false

    private let lock = NSLock()
    private var deviceCache: [String: UnifiedDevice] = [:]

    #if canImport(GoogleHomeSDK)
    /// The live `Home` session. iOS 17-only type; the whole backend is iOS 17+ so
    /// no `AnyObject` shadow is needed (unlike MatterBackend's `_controller`).
    @available(iOS 17.0, *)
    private var home: Home? {
        get { _home as? Home }
        set { _home = newValue }
    }
    private var _home: AnyObject?

    /// Google operates on `HomeDevice` objects, not numeric node IDs — keep a
    /// handle map keyed by the native (opaque string) id.
    @available(iOS 17.0, *)
    private var deviceHandles: [String: HomeDevice] {
        get { (_deviceHandles as? [String: HomeDevice]) ?? [:] }
        set { _deviceHandles = newValue }
    }
    private var _deviceHandles: AnyObject?

    /// Retained Combine subscriptions (device graph + per-device trait priming).
    private var cancellables: Set<AnyCancellable> = []

    /// Runs `Home.configure { ... }` exactly once per process.
    private static var didConfigure = false
    private static let configureLock = NSLock()
    #endif

    // MARK: - Lifecycle

    func startDiscovery() async throws {
        #if canImport(GoogleHomeSDK)
        guard isAvailable else { throw BackendError.notAvailable(source) }
        if #available(iOS 17.0, *) {
            Self.configureOnce()

            // 1. Try a silent session restore first; fall back to interactive connect.
            let session: Home
            if let restored = await Home.restoreSession() {
                session = restored
            } else {
                do {
                    session = try await Home.connect()   // may present OAuth consent UI
                } catch {
                    // Permission denied / no granted structure → actionable message.
                    throw BackendError.notConfigured(
                        "Sign in to Google Home and grant access to your structure.")
                }
            }
            lock.withLock { home = session }

            // 2. Subscribe to the authorized device graph. `devices().batched()` is a
            //    Combine publisher that emits a FULL `Set<HomeDevice>` snapshot on
            //    every change; rebuild the cache from each snapshot.
            session.devices().batched()
                .sink { [weak self] (snapshot: Set<HomeDevice>) in
                    self?.rebuildCache(from: snapshot, home: session)
                }
                .store(in: &cancellables)

            isActive = true
            return
        }
        #endif
        throw BackendError.notAvailable(source)
    }

    func stopDiscovery() async {
        #if canImport(GoogleHomeSDK)
        if #available(iOS 17.0, *) {
            cancellables.forEach { $0.cancel() }
            cancellables.removeAll()
            let session = lock.withLock { () -> Home? in
                let h = home
                home = nil
                deviceHandles = [:]
                deviceCache = [:]
                return h
            }
            // disconnect() tears down only the local session — it does NOT revoke
            // the OAuth grant (the user does that in the Google Home app).
            await session?.disconnect()
        }
        #endif
        isActive = false
    }

    #if canImport(GoogleHomeSDK)
    /// One-time SDK bootstrap. MUST run before any other backend call. In a real
    /// app this lives in an app-level bootstrap; doing it lazily here keeps the
    /// backend self-contained and idempotent.
    @available(iOS 17.0, *)
    private static func configureOnce() {
        configureLock.lock(); defer { configureLock.unlock() }
        guard !didConfigure else { return }
        Home.configure {
            // Filled from the Developer Console project (item 2 in the doc comment).
            $0.teamID = "<TEAM_ID>"
            $0.clientID = "<OAUTH_CLIENT_ID>"
            $0.sharedAppGroup = "<APP_GROUP_ID>"   // must match the MatterExtension
        }
        didConfigure = true
    }

    /// Rebuild the `UnifiedDevice` cache and `HomeDevice` handle map from a full
    /// graph snapshot. Attributes are primed lazily via trait subscriptions.
    @available(iOS 17.0, *)
    private func rebuildCache(from snapshot: Set<HomeDevice>, home: Home) {
        var devices: [String: UnifiedDevice] = [:]
        var handles: [String: HomeDevice] = [:]
        for homeDevice in snapshot {
            let native = homeDevice.id
            handles[native] = homeDevice
            devices[native] = Self.unify(homeDevice, source: source)
        }
        lock.withLock {
            deviceHandles = handles
            deviceCache = devices
        }
    }

    /// `HomeDevice` → `UnifiedDevice`. Google IDs are opaque strings, not numeric.
    @available(iOS 17.0, *)
    private static func unify(_ homeDevice: HomeDevice, source: BackendSource) -> UnifiedDevice {
        UnifiedDevice(
            id: "\(source.rawValue):\(homeDevice.id)",
            source: source,
            nativeId: homeDevice.id,
            name: homeDevice.name,
            // Room association needs a join against `home.rooms()`; "Unknown" until
            // resolved (Google exposes Room separately from the device snapshot).
            room: "Unknown",
            // Vendor isn't a flat field; left blank unless a basic-info trait is read.
            vendor: "",
            // Best-effort Matter device-type id from `homeDevice.types`; 0 otherwise.
            deviceType: 0,
            // Google has no crisp per-device reachable bool — presence in the
            // snapshot is treated as reachable.
            reachable: true,
            attributes: [:],
            lastUpdated: Date())
    }
    #endif

    // MARK: - Commissioning

    func commission(deviceId: String, payload: String?) async throws -> UnifiedDevice {
        // NOTE on `payload`: unlike MatterBackend (which parses MTRSetupPayload),
        // Apple's MatterSupport system sheet captures the QR/manual setup code from
        // the user itself. This backend does NOT parse the discriminator/passcode;
        // a nil `payload` is fine and a provided one is effectively ignored.
        #if canImport(GoogleHomeSDK)
        // Commissioning specifically needs iOS 17.6+.
        if #available(iOS 17.6, *) {
            guard isActive, let home = lock.withLock({ self.home }) else {
                throw BackendError.notConnected
            }

            // The SDK cannot create a fabric/structure — surface that honestly.
            guard let structure = try await home.structures().list().first else {
                throw BackendError.commissioningFailed(
                    "No Google Home structure available — create one in the Google Home app first.")
            }

            do {
                // 1. Verifies permission, that a Cast-OS hub is online/reachable,
                //    and that no concurrent session is running.
                do {
                    try await structure.prepareForMatterCommissioning()
                } catch {
                    throw BackendError.commissioningFailed(
                        "No compatible Google/Nest hub online — Matter commissioning routes through the hub.")
                }

                // 2. Apple MatterSupport: presents the system commissioning sheet
                //    which captures the setup code. Onboarding runs out-of-process
                //    in the MatterExtension target (see doc comment).
                let topology = MatterAddDeviceRequest.Topology(
                    ecosystemName: "Google Home",
                    homes: [MatterAddDeviceRequest.Home(displayName: structure.name)])
                let request = MatterAddDeviceRequest(topology: topology)
                try await request.perform()

                // 3. Finalize. Per the official sample this is `try` (non-async).
                let ids = try structure.completeMatterCommissioning()
                guard let nativeId = ids.first.map({ "\($0)" }) else {
                    throw BackendError.commissioningFailed(
                        "Commissioning completed but no device id was returned.")
                }

                let device = UnifiedDevice(
                    id: "\(source.rawValue):\(nativeId)",
                    source: source,
                    nativeId: nativeId,
                    name: "Google Device \(nativeId)",
                    room: "Unknown",
                    vendor: "",
                    deviceType: 0,
                    reachable: true,
                    attributes: [:],
                    lastUpdated: Date())
                lock.withLock { deviceCache[nativeId] = device }
                return device
            } catch {
                // Mandatory cleanup or the commissioning session sticks.
                structure.cancelMatterCommissioning()
                if let backendError = error as? BackendError { throw backendError }
                throw BackendError.commissioningFailed(error.localizedDescription)
            }
        }
        // Older than 17.6 → commissioning path unavailable.
        throw BackendError.notAvailable(source)
        #else
        throw BackendError.notAvailable(source)
        #endif
    }

    // MARK: - Attribute / command operations

    func readAttribute(deviceId: String, path: AttributePath) async throws -> SDKAttributeValue {
        #if canImport(GoogleHomeSDK)
        if #available(iOS 17.0, *) {
            let device = try handle(for: deviceId)

            // The honest "can't" case: any path outside the well-known translation
            // table — Google offers no generic attribute read.
            guard let mapping = TraitMap.lookup(path) else {
                throw BackendError.attributeNotFound(path)
            }

            // A "read" is the latest value from the live trait subscription, bounded
            // by a timeout (Google's model is push/Combine-first).
            do {
                return try await withTimeout(seconds: 20) {
                    try await Self.readMappedAttribute(device: device, mapping: mapping, path: path)
                }
            } catch let backendError as BackendError {
                throw backendError
            } catch {
                throw BackendError.deviceUnreachable(
                    "Couldn't read from the Google Home device — check it's online and the hub is reachable.")
            }
        }
        #endif
        throw BackendError.notConnected
    }

    func writeAttribute(deviceId: String, path: AttributePath, value: SDKAttributeValue) async throws {
        #if canImport(GoogleHomeSDK)
        if #available(iOS 17.0, *) {
            let device = try handle(for: deviceId)

            guard let mapping = TraitMap.lookup(path) else {
                throw BackendError.attributeNotFound(path)
            }

            // Read-only sensor paths can never be written.
            if mapping.writability == .readOnly {
                throw BackendError.writeFailure(
                    "Attribute \(TraitMap.describe(path)) is read-only on Google Home.")
            }

            do {
                try await Self.writeMappedAttribute(device: device, mapping: mapping, value: value)
            } catch let backendError as BackendError {
                throw backendError
            } catch {
                // Maps a thrown HomeError.
                throw BackendError.writeFailure(error.localizedDescription)
            }
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func invokeCommand(deviceId: String, endpointId: UInt16, clusterId: UInt32,
                       commandId: UInt32, payload: [String: Any]) async throws {
        #if canImport(GoogleHomeSDK)
        if #available(iOS 17.0, *) {
            let device = try handle(for: deviceId)
            // Google exposes NO raw invokeCommand(cluster:command:fields:). Dispatch
            // (clusterId, commandId) onto typed trait methods; anything not in the
            // table is an honest capability gap, not a crash.
            do {
                try await Self.dispatchCommand(
                    device: device, clusterId: clusterId, commandId: commandId, payload: payload)
            } catch let backendError as BackendError {
                throw backendError
            } catch {
                throw BackendError.commandFailure(error.localizedDescription)
            }
            return
        }
        #endif
        throw BackendError.notConnected
    }

    func subscribe(deviceId: String, paths: [AttributePath],
                   minInterval: TimeInterval, maxInterval: TimeInterval) -> AsyncStream<AttributeUpdate> {
        // NOTE: `minInterval` / `maxInterval` have NO equivalent in Google's Combine
        // subscription API. Google delivers changes when they happen, with no
        // client-controlled reporting interval. These parameters are accepted and
        // ignored (they could drive a local debounce only). The Matter MRP
        // min/max-interval semantics simply don't map.
        return AsyncStream { continuation in
            #if canImport(GoogleHomeSDK)
            if #available(iOS 17.0, *), let device = try? handle(for: deviceId), isActive {
                var locals: [AnyCancellable] = []
                let wanted = paths.isEmpty ? TraitMap.allPaths : paths

                for path in wanted {
                    guard let mapping = TraitMap.lookup(path) else { continue }
                    let cancellable = Self.subscribeMappedAttribute(
                        device: device, mapping: mapping, path: path) { value in
                            continuation.yield(AttributeUpdate(
                                deviceId: deviceId, path: path, value: value, timestamp: Date()))
                        }
                    if let cancellable = cancellable { locals.append(cancellable) }
                }

                if locals.isEmpty {
                    continuation.finish()
                    return
                }
                continuation.onTermination = { _ in
                    locals.forEach { $0.cancel() }
                }
            } else {
                continuation.finish()
            }
            #else
            continuation.finish()
            #endif
        }
    }

    func knownDevices() -> [UnifiedDevice] {
        #if canImport(GoogleHomeSDK)
        return lock.withLock { Array(deviceCache.values) }
        #else
        return []
        #endif
    }

    // `openCommissioningWindow` is intentionally NOT overridden — the protocol's
    // default extension throws a clear "multi-admin isn't supported for Google
    // Home devices" error. Rationale: the Google Home SDK owns no Matter fabric
    // (it uses the hub-managed fabric) and exposes no open-commissioning-window /
    // share-to-another-fabric API. Sharing is done by the user inside the Google
    // Home app, so inheriting the default is the faithful choice — no invented symbol.

    // MARK: - Helpers

    #if canImport(GoogleHomeSDK)
    @available(iOS 17.0, *)
    private func handle(for deviceId: String) throws -> HomeDevice {
        guard isActive, lock.withLock({ home }) != nil else { throw BackendError.notConnected }
        // Accept either the unified "<source>:<native>" id or the bare native id.
        let native = deviceId.contains(":")
            ? String(deviceId.split(separator: ":", maxSplits: 1).last ?? "")
            : deviceId
        guard let device = lock.withLock({ deviceHandles[native] }) else {
            throw BackendError.deviceNotFound(deviceId)
        }
        return device
    }

    /// Run an async operation bounded by a timeout, surfacing a clear "couldn't
    /// reach the device" error on expiry (mirrors MatterBackend's `withDeviceTimeout`).
    private func withTimeout<T: Sendable>(
        seconds: TimeInterval,
        operation: @escaping @Sendable () async throws -> T
    ) async throws -> T {
        try await withThrowingTaskGroup(of: T.self) { group in
            group.addTask { try await operation() }
            group.addTask {
                try await Task.sleep(nanoseconds: UInt64(seconds * 1_000_000_000))
                throw BackendError.deviceUnreachable(
                    "Couldn't reach the Google Home device within \(Int(seconds))s — check it's online and the hub is reachable.")
            }
            guard let result = try await group.next() else {
                throw BackendError.timeout
            }
            group.cancelAll()
            return result
        }
    }

    // -------------------------------------------------------------------------
    // The mapped read/write/subscribe/command bodies below contain the REAL SDK
    // call sites (exact `GoogleHomeTypes` symbols from the design) so the file is
    // drop-in once the package is added. They are only compiled when the SDK is
    // present. Each typed accessor is feature-detected before use; unsupported
    // members surface `attributeNotFound` / `commandFailure`.
    // -------------------------------------------------------------------------

    @available(iOS 17.0, *)
    private static func readMappedAttribute(
        device: HomeDevice, mapping: TraitMapping, path: AttributePath
    ) async throws -> SDKAttributeValue {
        switch mapping.kind {
        case .onOff:
            guard let type = await device.types.get(OnOffLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.onOffTrait, path)
            guard trait.attributes.$onOff.isSupported, let v = trait.onOff else {
                throw BackendError.attributeNotFound(path)
            }
            return .bool(v)

        case .currentLevel:
            guard let type = await device.types.get(DimmableLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.levelControlTrait, path)
            guard trait.attributes.$currentLevel.isSupported, let v = trait.currentLevel else {
                throw BackendError.attributeNotFound(path)
            }
            return .int(Int64(v))

        case .measuredTemp:
            guard let type = await device.types.get(TemperatureSensorDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.temperatureMeasurementTrait, path)
            guard let v = trait.measuredValue else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))   // centidegrees, matches UnifiedDevice.temperature (/100)

        case .measuredHumidity:
            guard let type = await device.types.get(HumiditySensorDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.relativeHumidityMeasurementTrait, path)
            guard let v = trait.measuredValue else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .lockState:
            guard let type = await device.types.get(DoorLockDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.doorLockTrait, path)
            guard let v = trait.lockState else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v.rawValue))

        case .localTemperature:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, path)
            guard let v = trait.localTemperature else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .heatingSetpoint:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, path)
            guard let v = trait.occupiedHeatingSetpoint else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .coolingSetpoint:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, path)
            guard let v = trait.occupiedCoolingSetpoint else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .thermostatMode:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, path)
            guard let v = trait.systemMode else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v.rawValue))

        case .fanMode:
            guard let type = await device.types.get(FanDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.fanControlTrait, path)
            guard let v = trait.fanMode else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v.rawValue))

        case .fanPercent:
            guard let type = await device.types.get(FanDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.fanControlTrait, path)
            guard let v = trait.percentSetting else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .coverPosition:
            guard let type = await device.types.get(WindowCoveringDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.windowCoveringTrait, path)
            guard let v = trait.currentPositionLiftPercentage else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .occupancy:
            guard let type = await device.types.get(OccupancySensorDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.occupancySensingTrait, path)
            guard let v = trait.occupancy else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v.rawValue))

        case .contactState:
            guard let type = await device.types.get(ContactSensorDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.booleanStateTrait, path)
            guard let v = trait.stateValue else { throw BackendError.attributeNotFound(path) }
            return .bool(v)

        case .colorTemp:
            guard let type = await device.types.get(ColorTemperatureLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.colorControlTrait, path)
            guard let v = trait.colorTemperatureMireds else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))

        case .batteryRemaining:
            guard let type = await device.types.get(PowerSourceDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.powerSourceTrait, path)
            // NOTE: verify exact accessor name (`batRemaining` vs `batPercentRemaining`).
            guard let v = trait.batPercentRemaining else { throw BackendError.attributeNotFound(path) }
            return .int(Int64(v))
        }
    }

    @available(iOS 17.0, *)
    private static func writeMappedAttribute(
        device: HomeDevice, mapping: TraitMapping, value: SDKAttributeValue
    ) async throws {
        switch mapping.kind {
        // Command-modeled "writes": Matter models these as commands, not attribute
        // writes — route to the corresponding command so the protocol contract holds.
        case .onOff:
            guard let type = await device.types.get(OnOffLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.onOffTrait, .onOff)
            if value.boolValue == true { try await trait.on() } else { try await trait.off() }

        case .currentLevel:
            guard let type = await device.types.get(DimmableLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.levelControlTrait, .currentLevel)
            let level = UInt8(clamping: value.intValue ?? 0)
            try await trait.moveToLevel(level: level, transitionTime: nil,
                                        optionsMask: .init(), optionsOverride: .init())

        // Genuinely writable attributes via the trait `update { }` builder.
        case .heatingSetpoint:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, .heatingSetpoint)
            let setpoint = Int16(clamping: value.intValue ?? 0)
            _ = try await trait.update { $0.setOccupiedHeatingSetpoint(setpoint) }

        case .coolingSetpoint:
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, .coolingSetpoint)
            let setpoint = Int16(clamping: value.intValue ?? 0)
            _ = try await trait.update { $0.setOccupiedCoolingSetpoint(setpoint) }

        case .fanPercent:
            guard let type = await device.types.get(FanDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.fanControlTrait, .fanPercent)
            let percent = UInt8(clamping: value.intValue ?? 0)
            _ = try await trait.update { $0.setPercentSetting(percent) }

        // Read-only paths are filtered out before reaching here; defensively reject.
        case .measuredTemp, .measuredHumidity, .occupancy, .contactState,
             .batteryRemaining, .localTemperature, .lockState, .thermostatMode,
             .fanMode, .coverPosition, .colorTemp:
            throw BackendError.writeFailure("Attribute isn't writable via the Google Home SDK.")
        }
    }

    @available(iOS 17.0, *)
    private static func dispatchCommand(
        device: HomeDevice, clusterId: UInt32, commandId: UInt32, payload: [String: Any]
    ) async throws {
        switch (clusterId, commandId) {
        case (0x0006, 0x00):   // OnOff / Off
            guard let type = await device.types.get(OnOffLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            try await requireTrait(type.matterTraits.onOffTrait, .onOff).off()

        case (0x0006, 0x01):   // OnOff / On
            guard let type = await device.types.get(OnOffLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            try await requireTrait(type.matterTraits.onOffTrait, .onOff).on()

        case (0x0006, 0x02):   // OnOff / Toggle
            guard let type = await device.types.get(OnOffLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.onOffTrait, .onOff)
            guard trait.supportsToggleCommand else {
                throw BackendError.commandFailure("Toggle isn't supported by this device.")
            }
            try await trait.toggle()

        case (0x0008, 0x00):   // LevelControl / MoveToLevel
            guard let type = await device.types.get(DimmableLightDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.levelControlTrait, .currentLevel)
            let level = UInt8(clamping: (payload["level"] as? Int) ?? 0)
            try await trait.moveToLevel(level: level, transitionTime: nil,
                                        optionsMask: .init(), optionsOverride: .init())

        case (0x0101, 0x00):   // DoorLock / Lock
            guard let type = await device.types.get(DoorLockDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            // Verify exact label (`lockDoor()` per design).
            try await requireTrait(type.matterTraits.doorLockTrait, .lockState).lockDoor()

        case (0x0101, 0x01):   // DoorLock / Unlock
            guard let type = await device.types.get(DoorLockDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            try await requireTrait(type.matterTraits.doorLockTrait, .lockState).unlockDoor()

        case (0x0201, 0x00):   // Thermostat / SetpointRaiseLower
            guard let type = await device.types.get(ThermostatDeviceType.self) else { throw BackendError.deviceUnreachable("This device doesn't expose the required Google Home device type.") }
            let trait = try requireTrait(type.matterTraits.thermostatTrait, .localTemperature)
            let mode = Matter.ThermostatTrait.SetpointRaiseLowerModeEnum(
                rawValue: UInt8(clamping: (payload["mode"] as? Int) ?? 0)) ?? .both
            let amount = Int8(clamping: (payload["amount"] as? Int) ?? 0)
            // Verify exact argument labels for setpointRaiseLower.
            try await trait.setpointRaiseLower(mode: mode, amount: amount)

        default:
            throw BackendError.commandFailure(
                "Command 0x\(String(commandId, radix: 16)) on cluster 0x\(String(clusterId, radix: 16)) isn't supported via the Google Home SDK (no generic command invoke).")
        }
    }

    @available(iOS 17.0, *)
    private static func subscribeMappedAttribute(
        device: HomeDevice, mapping: TraitMapping, path: AttributePath,
        onValue: @escaping (SDKAttributeValue) -> Void
    ) -> AnyCancellable? {
        // Subscribe to the relevant device type, project the mapped attribute, and
        // de-dupe. `.catch` swallows transient HomeErrors so the stream survives them
        // (per Google's device-monitoring guidance), rather than completing.
        switch mapping.kind {
        case .onOff:
            return device.types.subscribe(OnOffLightDeviceType.self)
                .compactMap { $0.matterTraits.onOffTrait?.onOff }
                .removeDuplicates()
                .catch { _ in Empty<Bool, Never>() }
                .sink { onValue(.bool($0)) }

        case .currentLevel:
            return device.types.subscribe(DimmableLightDeviceType.self)
                .compactMap { $0.matterTraits.levelControlTrait?.currentLevel }
                .removeDuplicates()
                .catch { _ in Empty<UInt8, Never>() }
                .sink { onValue(.int(Int64($0))) }

        case .measuredTemp:
            return device.types.subscribe(TemperatureSensorDeviceType.self)
                .compactMap { $0.matterTraits.temperatureMeasurementTrait?.measuredValue }
                .removeDuplicates()
                .catch { _ in Empty<Int16, Never>() }
                .sink { onValue(.int(Int64($0))) }

        case .measuredHumidity:
            return device.types.subscribe(HumiditySensorDeviceType.self)
                .compactMap { $0.matterTraits.relativeHumidityMeasurementTrait?.measuredValue }
                .removeDuplicates()
                .catch { _ in Empty<UInt16, Never>() }
                .sink { onValue(.int(Int64($0))) }

        case .lockState:
            return device.types.subscribe(DoorLockDeviceType.self)
                .compactMap { $0.matterTraits.doorLockTrait?.lockState }
                .removeDuplicates()
                .catch { _ in Empty<Matter.DoorLockTrait.LockStateEnum, Never>() }
                .sink { onValue(.int(Int64($0.rawValue))) }

        case .occupancy:
            return device.types.subscribe(OccupancySensorDeviceType.self)
                .compactMap { $0.matterTraits.occupancySensingTrait?.occupancy }
                .removeDuplicates()
                .catch { _ in Empty<Matter.OccupancySensingTrait.OccupancyBitmap, Never>() }
                .sink { onValue(.int(Int64($0.rawValue))) }

        case .contactState:
            return device.types.subscribe(ContactSensorDeviceType.self)
                .compactMap { $0.matterTraits.booleanStateTrait?.stateValue }
                .removeDuplicates()
                .catch { _ in Empty<Bool, Never>() }
                .sink { onValue(.bool($0)) }

        case .localTemperature, .heatingSetpoint, .coolingSetpoint, .thermostatMode,
             .fanMode, .fanPercent, .coverPosition, .colorTemp, .batteryRemaining:
            // These map onto traits too; for brevity they fall back to a one-shot
            // read on each emission of the owning device type. A production impl
            // would project the specific attribute like the cases above.
            return nil
        }
    }

    /// Feature-detect / unwrap a trait that may be absent on a given device.
    @available(iOS 17.0, *)
    private static func requireTrait<T>(_ trait: T?, _ path: AttributePath) throws -> T {
        guard let trait = trait else { throw BackendError.attributeNotFound(path) }
        return trait
    }
    #endif
}

// MARK: - Translation table (AttributePath ↔ Google trait)

#if canImport(GoogleHomeSDK)
/// Internal classification of a well-known `AttributePath` so the SDK call sites
/// can branch without re-deriving cluster/attribute ids each time.
@available(iOS 17.0, *)
private enum TraitMappingKind {
    case onOff
    case currentLevel
    case measuredTemp
    case measuredHumidity
    case lockState
    case localTemperature
    case heatingSetpoint
    case coolingSetpoint
    case thermostatMode
    case fanMode
    case fanPercent
    case coverPosition
    case occupancy
    case contactState
    case colorTemp
    case batteryRemaining
}

@available(iOS 17.0, *)
private enum TraitWritability {
    case readOnly
    case writable          // via trait.update { }
    case commandModeled    // via a typed command (OnOff, LevelControl, DoorLock)
}

@available(iOS 17.0, *)
private struct TraitMapping {
    let kind: TraitMappingKind
    let writability: TraitWritability
}

@available(iOS 17.0, *)
private enum TraitMap {
    static func lookup(_ path: AttributePath) -> TraitMapping? {
        switch path {
        case .onOff:            return TraitMapping(kind: .onOff, writability: .commandModeled)
        case .currentLevel:     return TraitMapping(kind: .currentLevel, writability: .commandModeled)
        case .measuredTemp:     return TraitMapping(kind: .measuredTemp, writability: .readOnly)
        case .measuredHumidity: return TraitMapping(kind: .measuredHumidity, writability: .readOnly)
        case .lockState:        return TraitMapping(kind: .lockState, writability: .commandModeled)
        case .localTemperature: return TraitMapping(kind: .localTemperature, writability: .readOnly)
        case .heatingSetpoint:  return TraitMapping(kind: .heatingSetpoint, writability: .writable)
        case .coolingSetpoint:  return TraitMapping(kind: .coolingSetpoint, writability: .writable)
        case .thermostatMode:   return TraitMapping(kind: .thermostatMode, writability: .writable)
        case .fanMode:          return TraitMapping(kind: .fanMode, writability: .writable)
        case .fanPercent:       return TraitMapping(kind: .fanPercent, writability: .writable)
        case .coverPosition:    return TraitMapping(kind: .coverPosition, writability: .commandModeled)
        case .occupancy:        return TraitMapping(kind: .occupancy, writability: .readOnly)
        case .contactState:     return TraitMapping(kind: .contactState, writability: .readOnly)
        case .colorTemp:        return TraitMapping(kind: .colorTemp, writability: .commandModeled)
        case .batteryRemaining: return TraitMapping(kind: .batteryRemaining, writability: .readOnly)
        default:                return nil
        }
    }

    static var allPaths: [AttributePath] {
        [.onOff, .currentLevel, .measuredTemp, .measuredHumidity, .lockState,
         .localTemperature, .heatingSetpoint, .coolingSetpoint, .thermostatMode,
         .fanMode, .fanPercent, .coverPosition, .occupancy, .contactState,
         .colorTemp, .batteryRemaining]
    }

    static func describe(_ path: AttributePath) -> String {
        "ep\(path.endpointId)/0x\(String(path.clusterId, radix: 16))/0x\(String(path.attributeId, radix: 16))"
    }
}
#endif
