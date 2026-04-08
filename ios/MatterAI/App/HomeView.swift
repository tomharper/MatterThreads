import SwiftUI

struct HomeView: View {
    @EnvironmentObject var sdk: MatterHomeSDK

    var body: some View {
        NavigationStack {
            List {
                ForEach(sdk.rooms, id: \.self) { room in
                    Section(room) {
                        ForEach(sdk.devicesInRoom(room)) { device in
                            NavigationLink(value: device.id) {
                                UnifiedDeviceRow(device: device) {
                                    Task { try? await sdk.toggleDevice(device) }
                                }
                            }
                        }
                    }
                }

                if sdk.devices.isEmpty {
                    Section {
                        VStack(spacing: 8) {
                            Image(systemName: "house.slash")
                                .font(.largeTitle)
                                .foregroundStyle(.secondary)
                            Text("No devices yet")
                                .font(.headline)
                            Text("Enable a backend in the SDK tab to discover devices.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                        }
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 24)
                    }
                }
            }
            .navigationTitle("Matter Home")
            .navigationDestination(for: String.self) { deviceId in
                if let device = sdk.devices.first(where: { $0.id == deviceId }) {
                    DeviceDetailView(device: device)
                }
            }
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(action: { sdk.refresh() }) {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
    }
}

struct UnifiedDeviceRow: View {
    let device: UnifiedDevice
    let onToggle: () -> Void

    var body: some View {
        HStack {
            Image(systemName: iconName)
                .font(.title2)
                .foregroundStyle(iconColor)
                .frame(width: 36)

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
                    if let bat = device.attributes[.batteryRemaining]?.intValue, bat < 60 {
                        Image(systemName: "battery.25")
                            .font(.caption2)
                            .foregroundStyle(.orange)
                    }
                }

                Text(device.stateDescription)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)

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
                    .onTapGesture { onToggle() }
            }
        }
        .padding(.vertical, 2)
        .opacity(device.reachable ? 1.0 : 0.5)
    }

    private var iconName: String {
        let name = device.name.lowercased()
        if name.contains("lock") {
            let locked = (device.attributes[.lockState]?.intValue ?? 0) == 1
            return locked ? "lock.fill" : "lock.open.fill"
        }
        if name.contains("thermostat") { return "thermostat.medium" }
        if name.contains("fan") { return "fan.fill" }
        if name.contains("blind") || name.contains("covering") { return "blinds.vertical.open" }
        if name.contains("smoke") || name.contains("alarm") { return "smoke.fill" }
        if name.contains("motion") || name.contains("occupancy") { return "sensor.fill" }
        if name.contains("contact") || name.contains("door sensor") || name.contains("fridge") { return "door.left.hand.closed" }
        if name.contains("plug") || name.contains("coffee") || name.contains("monitor") { return "powerplug.fill" }
        if device.attributes[.measuredTemp] != nil && device.attributes[.measuredHumidity] != nil { return "humidity.fill" }
        if device.attributes[.measuredTemp] != nil { return "thermometer.medium" }
        if name.contains("lamp") { return "lamp.floor.fill" }
        if name.contains("downlight") || name.contains("cabinet") || name.contains("ceiling light") { return "light.recessed.fill" }
        if name.contains("porch") { return "light.beacon.max.fill" }
        return "lightbulb.fill"
    }

    private var iconColor: Color {
        if !device.reachable { return .secondary }
        let name = device.name.lowercased()
        if name.contains("lock") {
            let locked = (device.attributes[.lockState]?.intValue ?? 0) == 1
            return locked ? .green : .orange
        }
        if name.contains("thermostat") { return .orange }
        if name.contains("smoke") { return .red }
        if name.contains("fan") { return device.isOn ? .blue : .secondary }
        if device.attributes[.measuredTemp] != nil { return .teal }
        if name.contains("motion") || name.contains("contact") || name.contains("fridge") { return .indigo }
        if name.contains("plug") || name.contains("coffee") || name.contains("monitor") { return device.isOn ? .green : .secondary }
        return device.isOn ? .yellow : .secondary
    }
}
