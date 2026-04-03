import SwiftUI

struct ContentView: View {
    @EnvironmentObject var homeManager: HomeManager
    @EnvironmentObject var sdk: MatterHomeSDK

    var body: some View {
        TabView {
            HomeView()
                .tabItem {
                    Label("Home", systemImage: "house.fill")
                }

            SDKStatusView()
                .tabItem {
                    Label("SDK", systemImage: "puzzlepiece.extension.fill")
                }

            EventLogView()
                .tabItem {
                    Label("Events", systemImage: "list.bullet.clipboard")
                }

            SimulationView()
                .tabItem {
                    Label("Mesh", systemImage: "network")
                }

            ChatView()
                .tabItem {
                    Label("Chat", systemImage: "bubble.left.and.bubble.right.fill")
                }
        }
    }
}
