import SwiftUI

/// Settings screen — manages backend credentials (Google OAuth), persisted
/// commissioned devices, and backend enable/disable.
struct SettingsView: View {
    @EnvironmentObject var sdk: MatterHomeSDK

    @State private var showGoogleConfig = false
    @State private var showClearConfirm = false

    var body: some View {
        NavigationStack {
            Form {
                // Backend toggles
                Section("Backends") {
                    ForEach(BackendSource.allCases, id: \.self) { source in
                        BackendToggleRow(source: source)
                    }
                }

                // Google OAuth configuration
                Section {
                    Button(action: { showGoogleConfig = true }) {
                        Label {
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Google Home OAuth")
                                    .foregroundStyle(.primary)
                                Text(googleConfigStatus)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        } icon: {
                            Image(systemName: "key.fill")
                                .foregroundStyle(.blue)
                        }
                    }
                } header: {
                    Text("Credentials")
                } footer: {
                    Text("Project ID, client ID/secret, and refresh token are stored in the iOS Keychain.")
                        .font(.caption2)
                }

                // Commissioned devices
                Section {
                    if sdk.commissionedStore.records.isEmpty {
                        Text("No devices commissioned yet")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(sdk.commissionedStore.records, id: \.deviceId) { record in
                            VStack(alignment: .leading, spacing: 2) {
                                Text(record.name)
                                    .font(.body)
                                Text("\(record.source) · \(record.commissionedAt.formatted(date: .abbreviated, time: .shortened))")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .onDelete { indexSet in
                            for idx in indexSet {
                                let record = sdk.commissionedStore.records[idx]
                                sdk.commissionedStore.remove(deviceId: record.deviceId)
                            }
                        }
                    }
                } header: {
                    Text("Commissioned Devices")
                } footer: {
                    Text("\(sdk.commissionedStore.records.count) device(s) persisted across app launches.")
                        .font(.caption2)
                }

                // Event log size
                Section("Event Stream") {
                    HStack {
                        Text("Buffered events")
                        Spacer()
                        Text("\(sdk.eventStream.recentEvents.count)")
                            .foregroundStyle(.secondary)
                    }
                    Button("Clear event log") {
                        sdk.eventStream.clear()
                    }
                    .disabled(sdk.eventStream.recentEvents.isEmpty)
                }

                // Danger zone
                Section {
                    Button(role: .destructive, action: { showClearConfirm = true }) {
                        Label("Clear All SDK Data", systemImage: "trash")
                    }
                } footer: {
                    Text("Removes persisted OAuth tokens, commissioned devices, and chat history.")
                        .font(.caption2)
                }
            }
            .navigationTitle("Settings")
            .sheet(isPresented: $showGoogleConfig) {
                GoogleOAuthConfigView()
            }
            .confirmationDialog("Clear all SDK data?", isPresented: $showClearConfirm, titleVisibility: .visible) {
                Button("Clear Everything", role: .destructive) { clearAllSDKData() }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("OAuth tokens, commissioned devices, chat history, and event log will be removed. Backends will need to be reconfigured.")
            }
        }
    }

    private var googleConfigStatus: String {
        if SDKKeychain.get("google.projectId") != nil {
            return "Configured"
        }
        return "Not configured"
    }

    private func clearAllSDKData() {
        SDKKeychain.delete("google.projectId")
        SDKKeychain.delete("google.clientId")
        SDKKeychain.delete("google.clientSecret")
        SDKKeychain.delete("google.refreshToken")
        SDKKeychain.delete("google.accessToken")
        for record in sdk.commissionedStore.records {
            sdk.commissionedStore.remove(deviceId: record.deviceId)
        }
        sdk.clearChat()
        sdk.eventStream.clear()
    }
}

// MARK: - Backend Toggle Row

private struct BackendToggleRow: View {
    let source: BackendSource
    @EnvironmentObject var sdk: MatterHomeSDK

    var body: some View {
        HStack {
            Image(systemName: icon)
                .foregroundStyle(color)
                .frame(width: 24)

            VStack(alignment: .leading, spacing: 2) {
                Text(source.rawValue)
                    .font(.body)
                Text(statusText)
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

    private var icon: String {
        switch source {
        case .local: return "desktopcomputer"
        case .appleMatter: return "m.square.fill"
        case .homeKit: return "house.fill"
        case .googleHome: return "g.circle.fill"
        case .thread: return "point.3.connected.trianglepath.dotted"
        }
    }

    private var color: Color {
        switch source {
        case .local: return .gray
        case .appleMatter: return .blue
        case .homeKit: return .orange
        case .googleHome: return .red
        case .thread: return .purple
        }
    }

    private var statusText: String {
        let count = sdk.devices(from: source).count
        if sdk.activeBackends.contains(source) {
            return count == 0 ? "Active · no devices" : "Active · \(count) device(s)"
        }
        return "Inactive"
    }
}

// MARK: - Google OAuth Config Sheet

struct GoogleOAuthConfigView: View {
    @Environment(\.dismiss) var dismiss
    @EnvironmentObject var sdk: MatterHomeSDK

    @State private var projectId: String = SDKKeychain.get("google.projectId") ?? ""
    @State private var clientId: String = SDKKeychain.get("google.clientId") ?? ""
    @State private var clientSecret: String = SDKKeychain.get("google.clientSecret") ?? ""
    @State private var refreshToken: String = SDKKeychain.get("google.refreshToken") ?? ""
    @State private var saveMessage: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Project ID", text: $projectId)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                    TextField("OAuth Client ID", text: $clientId)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                    SecureField("OAuth Client Secret", text: $clientSecret)
                    SecureField("Refresh Token", text: $refreshToken)
                } header: {
                    Text("Google Device Access")
                } footer: {
                    Text("Obtain these from Google Cloud Console → Device Access. The refresh token is generated via the OAuth consent flow.")
                        .font(.caption2)
                }

                Section {
                    Button("Save to Keychain", action: save)
                        .disabled(projectId.isEmpty || clientId.isEmpty || clientSecret.isEmpty)

                    Button("Test Connection", action: testConnection)
                        .disabled(projectId.isEmpty || refreshToken.isEmpty)

                    if let msg = saveMessage {
                        Text(msg)
                            .font(.caption)
                            .foregroundStyle(msg.hasPrefix("✓") ? .green : .red)
                    }
                }

                Section {
                    Link("Open Google Device Access Console",
                         destination: URL(string: "https://console.nest.google.com/device-access")!)
                    Link("OAuth Setup Documentation",
                         destination: URL(string: "https://developers.google.com/nest/device-access/authorize")!)
                }
            }
            .navigationTitle("Google OAuth")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }

    private func save() {
        sdk.enableGoogleHome(
            projectId: projectId,
            clientId: clientId,
            clientSecret: clientSecret,
            refreshToken: refreshToken.isEmpty ? nil : refreshToken
        )
        saveMessage = "✓ Saved to Keychain"
        Task { await sdk.start() }
    }

    private func testConnection() {
        saveMessage = "Testing..."
        Task {
            await sdk.start()
            let devices = sdk.devices(from: .googleHome)
            await MainActor.run {
                if sdk.activeBackends.contains(.googleHome) {
                    saveMessage = "✓ Connected · \(devices.count) device(s)"
                } else {
                    saveMessage = "✗ Could not connect — check credentials"
                }
            }
        }
    }
}
