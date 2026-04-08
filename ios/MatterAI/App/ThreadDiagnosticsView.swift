import SwiftUI

/// Thread mesh diagnostics view — reads Thread Network Diagnostics cluster (0x0035)
/// attributes from every device exposed by the Thread backend and displays a
/// role-annotated mesh overview.
struct ThreadDiagnosticsView: View {
    @EnvironmentObject var sdk: MatterHomeSDK

    private var threadNodes: [UnifiedDevice] {
        sdk.devices(from: .thread)
    }

    private var leaderCount: Int { threadNodes.filter { role(of: $0) == .leader }.count }
    private var routerCount: Int { threadNodes.filter { role(of: $0) == .router }.count }
    private var endDeviceCount: Int { threadNodes.filter { role(of: $0) == .endDevice }.count }
    private var reachableCount: Int { threadNodes.filter { $0.reachable }.count }

    var body: some View {
        NavigationStack {
            List {
                // Mesh summary header
                Section {
                    VStack(alignment: .leading, spacing: 12) {
                        HStack {
                            Image(systemName: "point.3.connected.trianglepath.dotted")
                                .font(.largeTitle)
                                .foregroundStyle(meshOnline ? .green : .secondary)
                            VStack(alignment: .leading) {
                                Text(meshOnline ? "Thread mesh online" : "No Thread backend")
                                    .font(.headline)
                                Text("\(threadNodes.count) node(s) · \(reachableCount) reachable")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }

                        if meshOnline {
                            HStack(spacing: 16) {
                                MeshStat(label: "Leader", count: leaderCount, color: .yellow)
                                MeshStat(label: "Routers", count: routerCount, color: .blue)
                                MeshStat(label: "End", count: endDeviceCount, color: .gray)
                            }
                        }
                    }
                    .padding(.vertical, 4)
                }

                // Per-node rows
                if !threadNodes.isEmpty {
                    Section("Nodes") {
                        ForEach(threadNodes) { node in
                            ThreadNodeRow(node: node, role: role(of: node))
                        }
                    }
                }

                // Diagnostics cluster reference
                Section("Diagnostics Cluster 0x0035") {
                    ClusterAttrRow(id: 0x0000, name: "Channel")
                    ClusterAttrRow(id: 0x0001, name: "Routing Role")
                    ClusterAttrRow(id: 0x0002, name: "Network Name")
                    ClusterAttrRow(id: 0x0003, name: "PAN ID")
                    ClusterAttrRow(id: 0x0007, name: "Neighbor Table Count")
                    ClusterAttrRow(id: 0x0008, name: "Route Table Count")
                    ClusterAttrRow(id: 0x000B, name: "Leader Router ID")
                }

                if !meshOnline {
                    Section {
                        Text("Enable the Thread backend in the SDK tab, and ensure the MatterThreads dashboard is running (default: localhost:8080).")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle("Thread Mesh")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(action: { sdk.refresh() }) {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
    }

    private var meshOnline: Bool { sdk.activeBackends.contains(.thread) && !threadNodes.isEmpty }

    // MARK: - Role extraction

    enum ThreadRole { case leader, router, endDevice, unknown }

    private func role(of node: UnifiedDevice) -> ThreadRole {
        let path = AttributePath(endpointId: 0, clusterId: 0x0035, attributeId: 0x0001)
        guard case .int(let v) = node.attributes[path] else { return .unknown }
        switch v {
        case 6: return .leader
        case 5: return .router
        case 4: return .endDevice
        default: return .unknown
        }
    }
}

private struct MeshStat: View {
    let label: String
    let count: Int
    let color: Color

    var body: some View {
        VStack(spacing: 2) {
            Text("\(count)")
                .font(.title2.bold())
                .foregroundStyle(color)
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 8)
        .background(color.opacity(0.1))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

private struct ThreadNodeRow: View {
    let node: UnifiedDevice
    let role: ThreadDiagnosticsView.ThreadRole

    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: icon)
                .font(.title3)
                .foregroundStyle(color)
                .frame(width: 28)

            VStack(alignment: .leading, spacing: 2) {
                Text(node.name)
                    .font(.body)
                HStack(spacing: 4) {
                    Text(roleLabel)
                        .font(.caption2)
                        .padding(.horizontal, 5)
                        .padding(.vertical, 1)
                        .background(color.opacity(0.2))
                        .foregroundStyle(color)
                        .clipShape(Capsule())
                    if let name = networkName {
                        Text(name)
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }
            }

            Spacer()

            if !node.reachable {
                Image(systemName: "wifi.slash")
                    .foregroundStyle(.red)
            } else {
                Circle().fill(.green).frame(width: 8, height: 8)
            }
        }
        .padding(.vertical, 2)
    }

    private var networkName: String? {
        let path = AttributePath(endpointId: 0, clusterId: 0x0035, attributeId: 0x0002)
        if case .string(let s) = node.attributes[path] { return s }
        return nil
    }

    private var icon: String {
        switch role {
        case .leader: return "star.circle.fill"
        case .router: return "dot.radiowaves.left.and.right"
        case .endDevice: return "leaf.fill"
        case .unknown: return "questionmark.circle"
        }
    }

    private var color: Color {
        switch role {
        case .leader: return .yellow
        case .router: return .blue
        case .endDevice: return .gray
        case .unknown: return .secondary
        }
    }

    private var roleLabel: String {
        switch role {
        case .leader: return "LEADER"
        case .router: return "ROUTER"
        case .endDevice: return "END"
        case .unknown: return "UNKNOWN"
        }
    }
}

private struct ClusterAttrRow: View {
    let id: UInt32
    let name: String

    var body: some View {
        HStack {
            Text(String(format: "0x%04X", id))
                .font(.caption.monospaced())
                .foregroundStyle(.secondary)
            Text(name)
                .font(.caption)
            Spacer()
        }
    }
}
