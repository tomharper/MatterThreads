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
            // Device icon
            Image(systemName: iconName)
                .font(.title2)
                .foregroundStyle(device.isOn ? .yellow : .secondary)
                .frame(width: 40)

            VStack(alignment: .leading, spacing: 2) {
                Text(device.name)
                    .font(.body)

                if let temp = device.temperature {
                    Text(String(format: "%.1f°C", temp))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                } else if let brightness = device.brightness, brightness > 0 {
                    Text("Brightness: \(brightness)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Spacer()

            if !device.reachable {
                Image(systemName: "wifi.slash")
                    .foregroundStyle(.red)
            } else if hasToggle {
                Toggle("", isOn: .constant(device.isOn))
                    .labelsHidden()
                    .onTapGesture { onToggle() }
            }
        }
        .padding(.vertical, 4)
        .opacity(device.reachable ? 1.0 : 0.5)
    }

    private var iconName: String {
        if device.temperature != nil { return "thermometer.medium" }
        if device.name.lowercased().contains("lock") { return "lock.fill" }
        if device.name.lowercased().contains("lamp") { return "lamp.floor.fill" }
        return "lightbulb.fill"
    }

    private var hasToggle: Bool {
        device.temperature == nil && !device.name.lowercased().contains("lock")
    }
}
