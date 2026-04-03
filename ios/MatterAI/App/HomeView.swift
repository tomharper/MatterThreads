import SwiftUI

struct HomeView: View {
    @EnvironmentObject var homeManager: HomeManager

    var body: some View {
        NavigationStack {
            List {
                ForEach(homeManager.rooms, id: \.self) { room in
                    Section(room) {
                        let roomDevices = homeManager.devices.filter { $0.room == room }
                        ForEach(roomDevices) { device in
                            DeviceRow(device: device) {
                                homeManager.toggleDevice(device)
                            }
                        }
                    }
                }
            }
            .navigationTitle("Matter Home")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(action: { homeManager.refresh() }) {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
    }
}

struct DeviceRow: View {
    let device: DeviceInfo
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
                    if let bat = device.battery, bat < 30 {
                        Image(systemName: "battery.25")
                            .font(.caption2)
                            .foregroundStyle(.orange)
                    }
                }

                Text(device.stateDescription)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)

                if let vendor = device.vendor, !vendor.isEmpty {
                    Text(vendor)
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
        if name.contains("lock") { return device.isLocked ? "lock.fill" : "lock.open.fill" }
        if name.contains("thermostat") { return "thermostat.medium" }
        if name.contains("fan") { return "fan.fill" }
        if name.contains("blind") || name.contains("covering") { return "blinds.vertical.open" }
        if name.contains("smoke") || name.contains("alarm") { return "smoke.fill" }
        if name.contains("motion") || name.contains("occupancy") { return "sensor.fill" }
        if name.contains("contact") || name.contains("door sensor") || name.contains("fridge") { return "door.left.hand.closed" }
        if name.contains("plug") || name.contains("coffee") || name.contains("monitor") { return "powerplug.fill" }
        if device.temperature != nil && device.humidity != nil { return "humidity.fill" }
        if device.temperature != nil { return "thermometer.medium" }
        if name.contains("lamp") { return "lamp.floor.fill" }
        if name.contains("downlight") || name.contains("cabinet") || name.contains("ceiling light") { return "light.recessed.fill" }
        if name.contains("porch") { return "light.beacon.max.fill" }
        return "lightbulb.fill"
    }

    private var iconColor: Color {
        if !device.reachable { return .secondary }
        let name = device.name.lowercased()
        if name.contains("lock") { return device.isLocked ? .green : .orange }
        if name.contains("thermostat") { return .orange }
        if name.contains("smoke") { return .red }
        if name.contains("fan") { return device.isOn ? .blue : .secondary }
        if device.temperature != nil { return .teal }
        if name.contains("motion") || name.contains("contact") || name.contains("fridge") { return .indigo }
        if name.contains("plug") || name.contains("coffee") || name.contains("monitor") { return device.isOn ? .green : .secondary }
        return device.isOn ? .yellow : .secondary
    }
}
