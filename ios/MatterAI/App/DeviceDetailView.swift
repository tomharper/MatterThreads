import SwiftUI

/// Detailed view of a single device from the unified SDK.
/// Shows all cached attributes and allows control.
struct DeviceDetailView: View {
    @EnvironmentObject var sdk: MatterHomeSDK
    let device: UnifiedDevice

    @State private var isRefreshing = false

    var body: some View {
        List {
            // Identity
            Section("Device Info") {
                InfoRow(label: "Name", value: device.name)
                InfoRow(label: "Room", value: device.room)
                InfoRow(label: "Backend", value: device.source.rawValue)
                InfoRow(label: "Vendor", value: device.vendor.isEmpty ? "Unknown" : device.vendor)
                if device.deviceType != 0 {
                    InfoRow(label: "Device Type", value: String(format: "0x%04X", device.deviceType))
                }
                InfoRow(label: "Native ID", value: device.nativeId)
                HStack {
                    Text("Reachable")
                        .foregroundStyle(.secondary)
                    Spacer()
                    Image(systemName: device.reachable ? "checkmark.circle.fill" : "xmark.circle.fill")
                        .foregroundStyle(device.reachable ? .green : .red)
                }
            }

            // Controls
            if device.hasToggle {
                Section("Controls") {
                    HStack {
                        Text("Power")
                        Spacer()
                        Toggle("", isOn: Binding(
                            get: { device.isOn },
                            set: { _ in
                                Task { try? await sdk.toggleDevice(device) }
                            }
                        ))
                        .labelsHidden()
                    }

                    if let brightness = device.brightness {
                        VStack(alignment: .leading) {
                            Text("Brightness: \(Int(Float(brightness) / 254.0 * 100))%")
                                .font(.caption)
                            Slider(value: .constant(Double(brightness) / 254.0), in: 0...1)
                                .disabled(true) // Read-only for now
                        }
                    }
                }
            }

            // State
            Section("State") {
                Text(device.stateDescription)
                    .font(.body)

                if let temp = device.temperature {
                    InfoRow(label: "Temperature", value: String(format: "%.1f°C", temp))
                }
                if let hum = device.humidity {
                    InfoRow(label: "Humidity", value: String(format: "%.0f%%", hum))
                }
                if let bat = device.batteryPercent {
                    HStack {
                        Text("Battery")
                            .foregroundStyle(.secondary)
                        Spacer()
                        BatteryView(percent: Int(bat))
                    }
                }
            }

            // Raw Attributes
            if !device.attributes.isEmpty {
                Section("Attributes (\(device.attributes.count))") {
                    ForEach(sortedAttributes, id: \.0) { path, value in
                        HStack {
                            VStack(alignment: .leading) {
                                Text(attributeLabel(path))
                                    .font(.caption.monospaced())
                                Text(clusterName(path.clusterId))
                                    .font(.caption2)
                                    .foregroundStyle(.tertiary)
                            }
                            Spacer()
                            Text(valueDescription(value))
                                .font(.caption.monospaced())
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }

            // Last Updated
            Section {
                InfoRow(label: "Last Updated", value: device.lastUpdated.formatted(.relative(presentation: .named)))
            }
        }
        .navigationTitle(device.name)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button(action: refresh) {
                    if isRefreshing {
                        ProgressView()
                            .controlSize(.small)
                    } else {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
    }

    private var sortedAttributes: [(AttributePath, SDKAttributeValue)] {
        device.attributes.sorted { lhs, rhs in
            if lhs.key.clusterId != rhs.key.clusterId { return lhs.key.clusterId < rhs.key.clusterId }
            return lhs.key.attributeId < rhs.key.attributeId
        }
    }

    private func attributeLabel(_ path: AttributePath) -> String {
        "ep\(path.endpointId)/0x\(String(format: "%04X", path.clusterId))/0x\(String(format: "%04X", path.attributeId))"
    }

    private func clusterName(_ id: UInt32) -> String {
        switch id {
        case 0x0006: return "OnOff"
        case 0x0008: return "LevelControl"
        case 0x0035: return "ThreadDiagnostics"
        case 0x0045: return "BooleanState"
        case 0x0101: return "DoorLock"
        case 0x0102: return "WindowCovering"
        case 0x0201: return "Thermostat"
        case 0x0202: return "FanControl"
        case 0x002F: return "PowerSource"
        case 0x0300: return "ColorControl"
        case 0x0402: return "TemperatureMeas"
        case 0x0405: return "HumidityMeas"
        case 0x0406: return "OccupancySensing"
        default: return String(format: "0x%04X", id)
        }
    }

    private func valueDescription(_ value: SDKAttributeValue) -> String {
        switch value {
        case .bool(let v): return v ? "true" : "false"
        case .int(let v): return "\(v)"
        case .float(let v): return String(format: "%.2f", v)
        case .string(let v): return v
        case .bytes(let d): return "\(d.count) bytes"
        }
    }

    private func refresh() {
        isRefreshing = true
        sdk.refresh()
        Task {
            try? await Task.sleep(for: .milliseconds(500))
            isRefreshing = false
        }
    }
}

private struct InfoRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .foregroundStyle(.secondary)
            Spacer()
            Text(value)
        }
    }
}

private struct BatteryView: View {
    let percent: Int

    var body: some View {
        HStack(spacing: 4) {
            Text("\(percent)%")
            Image(systemName: batteryIcon)
                .foregroundStyle(batteryColor)
        }
    }

    private var batteryIcon: String {
        if percent > 75 { return "battery.100" }
        if percent > 50 { return "battery.75" }
        if percent > 25 { return "battery.50" }
        return "battery.25"
    }

    private var batteryColor: Color {
        if percent > 50 { return .green }
        if percent > 20 { return .orange }
        return .red
    }
}
