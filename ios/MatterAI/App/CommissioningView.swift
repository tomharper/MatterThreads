import SwiftUI

/// View for commissioning/pairing new devices through any backend
struct CommissioningView: View {
    @EnvironmentObject var sdk: MatterHomeSDK
    @Environment(\.dismiss) var dismiss

    @State private var selectedBackend: BackendSource = .appleMatter
    @State private var setupCode: String = ""
    @State private var isCommissioning = false
    @State private var error: String?
    @State private var success = false

    var body: some View {
        NavigationStack {
            Form {
                Section("Backend") {
                    Picker("Commission via", selection: $selectedBackend) {
                        Text("Apple Matter").tag(BackendSource.appleMatter)
                        Text("HomeKit").tag(BackendSource.homeKit)
                    }
                    .pickerStyle(.segmented)

                    Text(backendDescription)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Section("Setup Code") {
                    TextField("MT: or 11-digit code", text: $setupCode)
                        .textContentType(.oneTimeCode)
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.characters)

                    Text("Enter the setup code from the device QR code or manual pairing code")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                if let error {
                    Section {
                        Label(error, systemImage: "exclamationmark.triangle.fill")
                            .foregroundStyle(.red)
                    }
                }

                if success {
                    Section {
                        Label("Device commissioned successfully!", systemImage: "checkmark.circle.fill")
                            .foregroundStyle(.green)
                    }
                }

                Section {
                    Button(action: startCommissioning) {
                        HStack {
                            if isCommissioning {
                                ProgressView()
                                    .controlSize(.small)
                                Text("Commissioning...")
                            } else {
                                Image(systemName: "plus.circle.fill")
                                Text("Commission Device")
                            }
                        }
                        .frame(maxWidth: .infinity)
                    }
                    .disabled(setupCode.isEmpty || isCommissioning)
                }

                Section("How it works") {
                    VStack(alignment: .leading, spacing: 8) {
                        StepRow(number: 1, text: "SDK routes to the selected backend")
                        StepRow(number: 2, text: "Backend initiates platform-specific pairing")
                        StepRow(number: 3, text: "Device appears in unified device list")
                        StepRow(number: 4, text: "Control via SDK regardless of backend")
                    }
                    .font(.caption)
                }
            }
            .navigationTitle("Add Device")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
            }
        }
    }

    private var backendDescription: String {
        switch selectedBackend {
        case .appleMatter:
            return "Uses Matter.framework (MTRDeviceController) to commission Matter devices directly. Requires iOS 16.1+."
        case .homeKit:
            return "Adds device through Apple Home. The device will appear in both HomeKit and the SDK."
        default:
            return ""
        }
    }

    private func startCommissioning() {
        isCommissioning = true
        error = nil
        success = false

        Task {
            do {
                let _ = try await sdk.commission(via: selectedBackend,
                                                  deviceId: setupCode,
                                                  payload: setupCode)
                success = true
            } catch {
                self.error = error.localizedDescription
            }
            isCommissioning = false
        }
    }
}

private struct StepRow: View {
    let number: Int
    let text: String

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            Text("\(number)")
                .font(.caption.bold())
                .frame(width: 20, height: 20)
                .background(Circle().fill(.blue.opacity(0.15)))
            Text(text)
                .foregroundStyle(.secondary)
        }
    }
}
