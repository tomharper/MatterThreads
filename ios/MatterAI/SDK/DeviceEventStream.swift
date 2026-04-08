import Foundation
import Combine

/// Centralized event stream for device state changes across all backends.
/// Provides both Combine publishers and AsyncSequence interfaces.
@MainActor
class DeviceEventStream: ObservableObject {
    /// All events since the stream was created (ring buffer, last 1000)
    @Published private(set) var recentEvents: [DeviceEvent] = []

    /// Combine publisher for real-time events
    let eventPublisher = PassthroughSubject<DeviceEvent, Never>()

    private let maxEvents = 1000

    func emit(_ event: DeviceEvent) {
        recentEvents.append(event)
        if recentEvents.count > maxEvents {
            recentEvents.removeFirst(recentEvents.count - maxEvents)
        }
        eventPublisher.send(event)
    }

    /// AsyncStream wrapper for Swift concurrency consumers
    func events() -> AsyncStream<DeviceEvent> {
        AsyncStream { continuation in
            let cancellable = eventPublisher.sink { event in
                continuation.yield(event)
            }
            continuation.onTermination = { _ in
                cancellable.cancel()
            }
        }
    }

    /// Filter events for a specific device
    func events(for deviceId: String) -> [DeviceEvent] {
        recentEvents.filter { $0.deviceId == deviceId }
    }

    /// Filter events by type
    func events(ofType type: DeviceEvent.EventType) -> [DeviceEvent] {
        recentEvents.filter { $0.type == type }
    }
}

/// A discrete event in the device lifecycle
struct DeviceEvent: Identifiable, Sendable {
    let id = UUID()
    let timestamp: Date
    let deviceId: String
    let deviceName: String
    let source: BackendSource
    let type: EventType
    let detail: String

    enum EventType: String, Sendable {
        case discovered       = "Discovered"
        case stateChanged     = "State Changed"
        case reachable        = "Reachable"
        case unreachable      = "Unreachable"
        case commissioned     = "Commissioned"
        case removed          = "Removed"
        case attributeWrite   = "Attribute Write"
        case attributeChanged = "Attribute Changed"
        case commandInvoked   = "Command Invoked"
        case error            = "Error"
    }
}

extension DeviceEvent {
    var icon: String {
        switch type {
        case .discovered: return "plus.circle.fill"
        case .stateChanged: return "arrow.triangle.2.circlepath"
        case .reachable: return "wifi"
        case .unreachable: return "wifi.slash"
        case .commissioned: return "checkmark.seal.fill"
        case .removed: return "minus.circle.fill"
        case .attributeWrite: return "pencil.circle.fill"
        case .attributeChanged: return "dot.radiowaves.left.and.right"
        case .commandInvoked: return "bolt.circle.fill"
        case .error: return "exclamationmark.triangle.fill"
        }
    }

    var iconColor: String {
        switch type {
        case .discovered, .commissioned: return "green"
        case .stateChanged, .attributeWrite, .attributeChanged, .commandInvoked: return "blue"
        case .reachable: return "green"
        case .unreachable, .error: return "red"
        case .removed: return "orange"
        }
    }
}
