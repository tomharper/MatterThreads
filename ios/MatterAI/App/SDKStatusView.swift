import SwiftUI

struct SDKStatusView: View {
    @EnvironmentObject var sdk: MatterHomeSDK

    var body: some View {
        NavigationStack {
            List {
                // Backend Status
                Section("Backends") {
                    ForEach(BackendSource.allCases, id: \.rawValue) { source in
                        HStack {
                            Image(systemName: iconName(for: source))
                                .foregroundStyle(iconColor(for: source))
                                .frame(width: 28)
                            VStack(alignment: .leading) {
                                Text(source.rawValue)
                                    .font(.body)
                                let count = sdk.devices(from: source).count
                                Text(count > 0 ? "\(count) device(s)" : statusText(for: source))
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            Spacer()
                            if sdk.activeBackends.contains(source) {
                                Image(systemName: "checkmark.circle.fill")
                                    .foregroundStyle(.green)
                            } else {
                                Image(systemName: "circle")
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }

                // Unified Device List
                if !sdk.devices.isEmpty {
                    ForEach(sdk.rooms, id: \.self) { room in
                        Section(room) {
                            ForEach(sdk.devicesInRoom(room)) { device in
                                HStack {
                                    VStack(alignment: .leading, spacing: 2) {
                                        HStack(spacing: 4) {
                                            Text(device.name)
                                                .font(.body)
                                            Text(device.source.rawValue)
                                                .font(.caption2)
                                                .padding(.horizontal, 4)
                                                .padding(.vertical, 1)
                                                .background(.quaternary)
                                                .clipShape(Capsule())
                                        }
                                        Text(device.stateDescription)
                                            .font(.caption)
                                            .foregroundStyle(.secondary)
                                        if !device.vendor.isEmpty {
                                            Text(device.vendor)
                                                .font(.caption2)
                                                .foregroundStyle(.tertiary)
                                        }
                                    }
                                    Spacer()
                                    if !device.reachable {
                                        Image(systemName: "wifi.slash")
                                            .font(.caption)
                                            .foregroundStyle(.red)
                                    } else if device.hasToggle {
                                        Toggle("", isOn: .constant(device.isOn))
                                            .labelsHidden()
                                            .onTapGesture {
                                                Task { try? await sdk.toggleDevice(device) }
                                            }
                                    }
                                }
                                .opacity(device.reachable ? 1.0 : 0.5)
                            }
                        }
                    }
                }

                // Summary
                Section("Summary") {
                    Text(sdk.homeSummary())
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Integration SDK")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(action: { sdk.refresh() }) {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
    }

    private func iconName(for source: BackendSource) -> String {
        switch source {
        case .local: return "desktopcomputer"
        case .appleMatter: return "circle.hexagongrid.fill"
        case .homeKit: return "homekit"
        case .googleHome: return "globe"
        case .thread: return "antenna.radiowaves.left.and.right"
        }
    }

    private func iconColor(for source: BackendSource) -> Color {
        guard sdk.activeBackends.contains(source) else { return .secondary }
        switch source {
        case .local: return .blue
        case .appleMatter: return .orange
        case .homeKit: return .pink
        case .googleHome: return .green
        case .thread: return .purple
        }
    }

    private func statusText(for source: BackendSource) -> String {
        switch source {
        case .local: return "Demo devices"
        case .appleMatter: return "Requires iOS 16.1+"
        case .homeKit: return "Requires HomeKit entitlement"
        case .googleHome: return "Requires OAuth setup"
        case .thread: return "Requires Thread network"
        }
    }
}
