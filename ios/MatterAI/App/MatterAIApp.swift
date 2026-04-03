import SwiftUI

@main
struct MatterAIApp: App {
    @StateObject private var homeManager = HomeManager()
    @StateObject private var sdk = MatterHomeSDK()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(homeManager)
                .environmentObject(sdk)
                .task {
                    sdk.enableLocal()
                    sdk.enableMatter()
                    sdk.enableHomeKit()
                    sdk.enableThread()
                    await sdk.start()
                }
        }
    }
}
