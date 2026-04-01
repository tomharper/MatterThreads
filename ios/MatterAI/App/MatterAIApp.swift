import SwiftUI

@main
struct MatterAIApp: App {
    @StateObject private var homeManager = HomeManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(homeManager)
        }
    }
}
