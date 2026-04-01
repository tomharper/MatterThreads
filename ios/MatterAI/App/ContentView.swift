import SwiftUI

struct ContentView: View {
    @EnvironmentObject var homeManager: HomeManager

    var body: some View {
        TabView {
            HomeView()
                .tabItem {
                    Label("Home", systemImage: "house.fill")
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
