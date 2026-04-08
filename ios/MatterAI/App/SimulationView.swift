import SwiftUI

struct SimulationView: View {
    @EnvironmentObject var homeManager: HomeManager

    var body: some View {
        NavigationStack {
            List {
                // Connection status
                Section {
                    ConnectionRow(
                        label: "Dashboard",
                        port: 8080,
                        connected: homeManager.dashboardConnected
                    )
                    ConnectionRow(
                        label: "Gateway",
                        port: 8090,
                        connected: homeManager.gatewayConnected
                    )
                } header: {
                    Text("Connection")
                }

                // Mesh nodes
                if !homeManager.simNodes.isEmpty {
                    Section {
                        ForEach(homeManager.simNodes, id: \.nodeId) { node in
                            NodeRow(node: node)
                        }
                    } header: {
                        HStack {
                            Text("Mesh Nodes")
                            Spacer()
                            if homeManager.meshHealthy {
                                Text("Healthy")
                                    .font(.caption)
                                    .foregroundStyle(.green)
                            } else {
                                Text("Issues")
                                    .font(.caption)
                                    .foregroundStyle(.red)
                            }
                        }
                    }

                    // Topology
                    Section("Link Quality") {
                        TopologyGrid(linkInfo: homeManager.linkInfoForPair)
                    }
                }

                // Fleet vans
                if !homeManager.simVans.isEmpty {
                    Section {
                        ForEach(homeManager.simVans, id: \.vanId) { van in
                            VanRow(van: van) {
                                homeManager.toggleVanLock(van)
                            }
                        }
                    } header: {
                        Text("Fleet")
                    }
                }

                // No connection state
                if homeManager.simNodes.isEmpty && !homeManager.dashboardConnected {
                    Section {
                        VStack(spacing: 12) {
                            Image(systemName: "network.slash")
                                .font(.system(size: 40))
                                .foregroundStyle(.secondary)
                            Text("Not Connected")
                                .font(.headline)
                            Text("Start MatterThreads with --dashboard 8080 to connect.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                        }
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 20)
                    }
                }
            }
            .navigationTitle("Simulation")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    HStack {
                        NavigationLink(destination: ThreadDiagnosticsView()) {
                            Image(systemName: "point.3.connected.trianglepath.dotted")
                        }
                        Button(action: { homeManager.refreshSimulation() }) {
                            Image(systemName: "arrow.clockwise")
                        }
                    }
                }
                ToolbarItem(placement: .topBarLeading) {
                    Toggle(isOn: $homeManager.autoRefresh) {
                        Image(systemName: "timer")
                    }
                    .toggleStyle(.button)
                    .tint(homeManager.autoRefresh ? .green : .secondary)
                }
            }
        }
    }
}

// MARK: - Subviews

struct ConnectionRow: View {
    let label: String
    let port: Int
    let connected: Bool

    var body: some View {
        HStack {
            Circle()
                .fill(connected ? .green : .red)
                .frame(width: 10, height: 10)
            Text(label)
            Spacer()
            Text(":\(port)")
                .font(.caption.monospaced())
                .foregroundStyle(.secondary)
        }
    }
}

struct NodeRow: View {
    let node: SimNodeInfo

    var body: some View {
        HStack {
            Image(systemName: nodeIcon)
                .font(.title3)
                .foregroundStyle(node.reachable ? roleColor : .secondary)
                .frame(width: 30)

            VStack(alignment: .leading, spacing: 2) {
                Text("Node \(node.nodeId)")
                    .font(.body.bold())
                Text(node.role.capitalized)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Text(node.state)
                .font(.caption)
                .padding(.horizontal, 8)
                .padding(.vertical, 3)
                .background(stateColor.opacity(0.2))
                .foregroundStyle(stateColor)
                .clipShape(Capsule())
        }
    }

    private var nodeIcon: String {
        switch node.role.lowercased() {
        case "leader": return "crown.fill"
        case "router": return "arrow.triangle.branch"
        case "sed", "enddevice": return "sensor.fill"
        case "phone": return "iphone"
        default: return "circle.fill"
        }
    }

    private var roleColor: Color {
        switch node.role.lowercased() {
        case "leader": return .yellow
        case "router": return .blue
        case "sed", "enddevice": return .green
        case "phone": return .purple
        default: return .secondary
        }
    }

    private var stateColor: Color {
        switch node.state.lowercased() {
        case "running": return .green
        case "crashed": return .red
        case "frozen": return .orange
        default: return .secondary
        }
    }
}

struct TopologyGrid: View {
    let linkInfo: (Int, Int) -> LinkInfoData?

    var body: some View {
        VStack(spacing: 4) {
            // Header
            HStack(spacing: 0) {
                Text("")
                    .frame(width: 30)
                ForEach(0..<4, id: \.self) { col in
                    Text("\(col)")
                        .font(.caption.bold())
                        .frame(maxWidth: .infinity)
                }
            }
            // Rows
            ForEach(0..<4, id: \.self) { row in
                HStack(spacing: 0) {
                    Text("\(row)")
                        .font(.caption.bold())
                        .frame(width: 30)
                    ForEach(0..<4, id: \.self) { col in
                        if row == col {
                            Text("-")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity)
                        } else if let link = linkInfo(row, col) {
                            LinkCell(link: link)
                                .frame(maxWidth: .infinity)
                        } else {
                            Text("?")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity)
                        }
                    }
                }
            }
        }
        .padding(.vertical, 4)
    }
}

struct LinkCell: View {
    let link: LinkInfoData

    var body: some View {
        VStack(spacing: 1) {
            if !link.up {
                Image(systemName: "xmark")
                    .font(.caption2)
                    .foregroundStyle(.red)
            } else {
                Text(String(format: "%.0f%%", link.lossPercent))
                    .font(.system(size: 9).monospaced())
                    .foregroundStyle(lossColor)
                Text(String(format: "%.0fms", link.latencyMs))
                    .font(.system(size: 8).monospaced())
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var lossColor: Color {
        if link.lossPercent > 20 { return .red }
        if link.lossPercent > 5 { return .orange }
        return .green
    }
}

struct VanRow: View {
    let van: SimVanInfo
    let onToggleLock: () -> Void

    var body: some View {
        HStack {
            Image(systemName: "box.truck.fill")
                .font(.title3)
                .foregroundStyle(stateColor)
                .frame(width: 30)

            VStack(alignment: .leading, spacing: 2) {
                Text(van.name)
                    .font(.body)
                Text(van.vanId)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Text(van.state)
                .font(.caption)
                .padding(.horizontal, 8)
                .padding(.vertical, 3)
                .background(stateColor.opacity(0.2))
                .foregroundStyle(stateColor)
                .clipShape(Capsule())

            Button(action: onToggleLock) {
                Image(systemName: van.locked ? "lock.fill" : "lock.open.fill")
                    .foregroundStyle(van.locked ? .green : .orange)
            }
            .buttonStyle(.plain)
        }
    }

    private var stateColor: Color {
        switch van.state.lowercased() {
        case "online": return .green
        case "offline": return .secondary
        case "unreachable": return .red
        default: return .orange
        }
    }
}
