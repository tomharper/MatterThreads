import SwiftUI
import CoreImage.CIFilterBuiltins
#if canImport(UIKit)
import UIKit
#endif

/// Multi-admin share sheet. Opens a Matter commissioning window on a device the
/// app already owns and presents the pairing code (QR + manual) to add it to
/// Apple Home and Google Home. The device stays controllable from this app while
/// the other ecosystems join as additional fabric administrators.
struct ShareDeviceView: View {
    @EnvironmentObject var sdk: MatterHomeSDK
    @Environment(\.dismiss) private var dismiss
    let device: UnifiedDevice

    @State private var handoff: PairingHandoff?
    @State private var errorMessage: String?
    @State private var isOpening = true
    @State private var secondsRemaining = 0
    @State private var copied = false

    private let ticker = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        NavigationStack {
            Group {
                if isOpening {
                    openingState
                } else if let handoff = handoff {
                    pairingState(handoff)
                } else {
                    errorState
                }
            }
            .padding()
            .navigationTitle("Share Device")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { dismiss() }
                }
            }
        }
        .task { await openWindow() }
        .onReceive(ticker) { _ in
            if secondsRemaining > 0 { secondsRemaining -= 1 }
        }
    }

    // MARK: - States

    private var openingState: some View {
        VStack(spacing: 16) {
            ProgressView()
            Text("Opening a commissioning window on \(device.name)…")
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
        }
    }

    private var errorState: some View {
        VStack(spacing: 16) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.largeTitle)
                .foregroundStyle(.orange)
            Text(errorMessage ?? "Couldn't share this device.")
                .multilineTextAlignment(.center)
            Button("Try Again") { Task { await openWindow() } }
                .buttonStyle(.borderedProminent)
        }
    }

    private func pairingState(_ handoff: PairingHandoff) -> some View {
        ScrollView {
            VStack(spacing: 20) {
                Text("Add **\(device.name)** to another app")
                    .font(.headline)

                if let qr = handoff.qrCode, let image = Self.qrImage(from: qr) {
                    Image(uiImage: image)
                        .interpolation(.none)
                        .resizable()
                        .frame(width: 200, height: 200)
                        .padding(8)
                        .background(.white)
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                        .overlay(RoundedRectangle(cornerRadius: 12).stroke(.quaternary))
                }

                VStack(spacing: 6) {
                    Text("Manual pairing code")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Text(handoff.manualCode)
                        .font(.system(.title2, design: .monospaced).weight(.semibold))
                        .textSelection(.enabled)
                    Button {
                        copyCode(handoff.manualCode)
                    } label: {
                        Label(copied ? "Copied" : "Copy code", systemImage: copied ? "checkmark" : "doc.on.doc")
                            .font(.caption)
                    }
                    .buttonStyle(.bordered)
                }

                if secondsRemaining > 0 {
                    Label("Window open for \(timeString(secondsRemaining))", systemImage: "clock")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                } else {
                    Label("Window expired — tap Try Again", systemImage: "clock.badge.xmark")
                        .font(.caption)
                        .foregroundStyle(.orange)
                }

                VStack(spacing: 10) {
                    EcosystemButton(
                        title: "Add to Apple Home",
                        systemImage: "homekit",
                        tint: .blue,
                        instructions: "Open Home → + → Add Accessory → More options, then scan the code above.",
                        url: URL(string: "x-apple-home://"))
                    EcosystemButton(
                        title: "Add to Google Home",
                        systemImage: "g.circle.fill",
                        tint: .green,
                        instructions: "Open Google Home → + → Set up device → Matter-enabled device, then scan the code above.",
                        url: URL(string: "googlehome://"))
                }
                .padding(.top, 4)

                Text("The device stays controllable here while the other apps join as additional admins.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
        }
    }

    // MARK: - Actions

    private func openWindow() async {
        isOpening = true
        errorMessage = nil
        do {
            let result = try await sdk.shareDevice(device)
            handoff = result
            secondsRemaining = result.durationSeconds
        } catch {
            errorMessage = error.localizedDescription
            handoff = nil
        }
        isOpening = false
    }

    private func copyCode(_ code: String) {
        #if canImport(UIKit)
        UIPasteboard.general.string = code
        #endif
        withAnimation { copied = true }
    }

    private func timeString(_ seconds: Int) -> String {
        String(format: "%d:%02d", seconds / 60, seconds % 60)
    }

    // MARK: - QR

    private static func qrImage(from string: String) -> UIImage? {
        #if canImport(UIKit)
        let filter = CIFilter.qrCodeGenerator()
        filter.message = Data(string.utf8)
        filter.correctionLevel = "M"
        guard let output = filter.outputImage else { return nil }
        let scaled = output.transformed(by: CGAffineTransform(scaleX: 10, y: 10))
        let context = CIContext()
        guard let cg = context.createCGImage(scaled, from: scaled.extent) else { return nil }
        return UIImage(cgImage: cg)
        #else
        return nil
        #endif
    }
}

/// A button that opens an ecosystem's app (best-effort) and always shows the
/// manual fallback instructions.
private struct EcosystemButton: View {
    let title: String
    let systemImage: String
    let tint: Color
    let instructions: String
    let url: URL?

    @Environment(\.openURL) private var openURL

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Button {
                if let url = url { openURL(url) { _ in } }
            } label: {
                Label(title, systemImage: systemImage)
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .tint(tint)

            Text(instructions)
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
    }
}
