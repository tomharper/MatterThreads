import SwiftUI

/// Shows the real-time event log from the SDK event stream
struct EventLogView: View {
    @EnvironmentObject var eventStream: DeviceEventStream
    @State private var filter: DeviceEvent.EventType?

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Filter bar
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        FilterChip(label: "All", isSelected: filter == nil) {
                            filter = nil
                        }
                        ForEach(eventTypes, id: \.rawValue) { type in
                            FilterChip(label: type.rawValue, isSelected: filter == type) {
                                filter = filter == type ? nil : type
                            }
                        }
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                }
                .background(.bar)

                // Event list
                if filteredEvents.isEmpty {
                    ContentUnavailableView("No Events",
                        systemImage: "list.bullet.clipboard",
                        description: Text("Device events will appear here as they occur"))
                } else {
                    List(filteredEvents.reversed()) { event in
                        HStack(alignment: .top, spacing: 10) {
                            Image(systemName: event.icon)
                                .font(.body)
                                .foregroundStyle(Color(event.iconColor))
                                .frame(width: 24)

                            VStack(alignment: .leading, spacing: 2) {
                                HStack {
                                    Text(event.type.rawValue)
                                        .font(.caption.bold())
                                    Spacer()
                                    Text(event.timestamp, style: .time)
                                        .font(.caption2)
                                        .foregroundStyle(.tertiary)
                                }
                                Text(event.deviceName)
                                    .font(.caption)
                                HStack(spacing: 4) {
                                    Text(event.source.rawValue)
                                        .font(.caption2)
                                        .padding(.horizontal, 4)
                                        .padding(.vertical, 1)
                                        .background(.quaternary)
                                        .clipShape(Capsule())
                                    if !event.detail.isEmpty {
                                        Text(event.detail)
                                            .font(.caption2)
                                            .foregroundStyle(.secondary)
                                            .lineLimit(1)
                                    }
                                }
                            }
                        }
                        .listRowInsets(EdgeInsets(top: 6, leading: 12, bottom: 6, trailing: 12))
                    }
                    .listStyle(.plain)
                }
            }
            .navigationTitle("Event Log")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Text("\(filteredEvents.count) events")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var filteredEvents: [DeviceEvent] {
        guard let filter else { return eventStream.recentEvents }
        return eventStream.events(ofType: filter)
    }

    private var eventTypes: [DeviceEvent.EventType] {
        [.discovered, .stateChanged, .reachable, .unreachable, .attributeWrite, .commandInvoked, .error]
    }
}

private struct FilterChip: View {
    let label: String
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(label)
                .font(.caption)
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(isSelected ? Color.blue : Color(.systemGray5))
                .foregroundStyle(isSelected ? .white : .primary)
                .clipShape(Capsule())
        }
    }
}
