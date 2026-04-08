import SwiftUI

struct ChatView: View {
    @EnvironmentObject var sdk: MatterHomeSDK
    @State private var inputText = ""
    @FocusState private var inputFocused: Bool

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Messages
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(spacing: 12) {
                            if sdk.chatHistory.isEmpty {
                                WelcomeCard()
                                    .padding(.top, 40)
                            }

                            ForEach(sdk.chatHistory) { message in
                                MessageBubble(message: message)
                                    .id(message.id)
                            }

                            if sdk.isProcessingQuery {
                                HStack {
                                    ProgressView().scaleEffect(0.7)
                                    Text("Thinking...")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                    Spacer()
                                }
                                .padding(.horizontal, 12)
                            }
                        }
                        .padding()
                    }
                    .onChange(of: sdk.chatHistory.count) {
                        if let last = sdk.chatHistory.last {
                            withAnimation {
                                proxy.scrollTo(last.id, anchor: .bottom)
                            }
                        }
                    }
                }

                // Quick actions
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        QuickAction("What's on?") { send("What's on?") }
                        QuickAction("Home status") { send("status") }
                        QuickAction("Temperatures") { send("What are the temperatures?") }
                        QuickAction("Backends") { send("backend status") }
                        QuickAction("All off") { send("Turn everything off") }
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                }
                .background(.ultraThinMaterial)

                // Input bar
                HStack(spacing: 12) {
                    TextField("Ask about your home...", text: $inputText)
                        .textFieldStyle(.plain)
                        .focused($inputFocused)
                        .onSubmit { sendInput() }

                    Button(action: sendInput) {
                        Image(systemName: "arrow.up.circle.fill")
                            .font(.title2)
                            .foregroundColor(inputText.isEmpty ? .secondary : .blue)
                    }
                    .disabled(inputText.isEmpty || sdk.isProcessingQuery)
                }
                .padding()
                .background(.ultraThinMaterial)
            }
            .navigationTitle("Assistant")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(action: { sdk.clearChat() }) {
                        Image(systemName: "trash")
                    }
                    .disabled(sdk.chatHistory.isEmpty)
                }
            }
        }
    }

    private func sendInput() {
        guard !inputText.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        let text = inputText
        inputText = ""
        send(text)
    }

    private func send(_ text: String) {
        Task { await sdk.ask(text) }
    }
}

// MARK: - Subviews

struct MessageBubble: View {
    let message: SDKChatMessage

    var body: some View {
        HStack {
            if message.isUser { Spacer(minLength: 60) }

            Text(message.text)
                .padding(12)
                .background(message.isUser ? Color.blue : Color(.systemGray5))
                .foregroundStyle(message.isUser ? .white : .primary)
                .clipShape(RoundedRectangle(cornerRadius: 16))

            if !message.isUser { Spacer(minLength: 60) }
        }
    }
}

struct WelcomeCard: View {
    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: "house.and.flag.fill")
                .font(.system(size: 48))
                .foregroundStyle(.blue)

            Text("Matter Home Assistant")
                .font(.title2.bold())

            Text("Ask me about your devices, or tell me to control them.")
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .padding(24)
    }
}

struct QuickAction: View {
    let title: String
    let action: () -> Void

    init(_ title: String, action: @escaping () -> Void) {
        self.title = title
        self.action = action
    }

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.caption)
                .padding(.horizontal, 12)
                .padding(.vertical, 6)
                .background(Color(.systemGray5))
                .clipShape(Capsule())
        }
        .buttonStyle(.plain)
    }
}
