import SwiftUI

struct SDKStatusView: View {
    @EnvironmentObject var sdk: MatterHomeSDK
    @State private var showCommissioning = false
    @State private var showSettings = false

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
                                NavigationLink(value: device.id) {
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
                }

                // Architecture Info
                Section("Architecture") {
                    VStack(alignment: .leading, spacing: 6) {
                        ArchRow(icon: "app.connected.to.app.below.fill", label: "MatterHomeSDK", detail: "Public API")
                        ArchRow(icon: "arrow.triangle.branch", label: "BackendRouter", detail: "Routes by device source")
                        ArchRow(icon: "square.stack.3d.up", label: "DeviceBackend", detail: "Protocol — 5 implementations")
                        ArchRow(icon: "cpu", label: "C++ Core", detail: "DeviceModel + TLV + SimulationModel")
                    }
                    .font(.caption)
                }
            }
            .navigationTitle("Integration SDK")
            .navigationDestination(for: String.self) { deviceId in
                if let device = sdk.devices.first(where: { $0.id == deviceId }) {
                    DeviceDetailView(device: device)
                }
            }
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button(action: { showCommissioning = true }) {
                        Image(systemName: "plus")
                    }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    HStack {
                        Button(action: { showSettings = true }) {
                            Image(systemName: "gearshape")
                        }
                        Button(action: { sdk.refresh() }) {
                            Image(systemName: "arrow.clockwise")
                        }
                    }
                }
            }
            .sheet(isPresented: $showCommissioning) {
                CommissioningView()
            }
            .sheet(isPresented: $showSettings) {
                SettingsView()
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

private struct ArchRow: View {
    let icon: String
    let label: String
    let detail: String

    var body: some View {
        HStack(spacing: 8) {
            Image(systemName: icon)
                .frame(width: 20)
                .foregroundStyle(.blue)
            Text(label).bold()
            Text("— \(detail)")
                .foregroundStyle(.secondary)
        }
    }
}
