#include "gateway/GatewayServer.h"
#include "gateway/VanRegistry.h"
#include "gateway/SessionPool.h"
#include "gateway/FleetSubscriptionManager.h"
#include "gateway/CommandRelay.h"
#include "gateway/OfflineBuffer.h"
#include "gateway/FabricManager.h"
#include "core/Log.h"
#include "hw/ChipToolDriver.h"

#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

static volatile sig_atomic_t g_running = 1;

static void signalHandler(int) {
    g_running = 0;
}

int main(int argc, char* argv[]) {
    MT_INFO("gateway", "Fleet Gateway starting...");

    // Parse command-line args
    uint16_t port = 8090;
    std::string chip_tool_path = "/usr/local/bin/chip-tool";
    std::string storage_dir = "/tmp/mt-gateway";

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg == "--chip-tool" && i + 1 < argc) {
            chip_tool_path = argv[++i];
        } else if (arg == "--storage" && i + 1 < argc) {
            storage_dir = argv[++i];
        }
    }

    // Configure chip-tool driver
    mt::hw::ChipToolConfig chip_config;
    chip_config.binary_path = chip_tool_path;
    chip_config.storage_dir = storage_dir;
    chip_config.command_timeout = mt::Duration(30000);

    auto driver = std::make_shared<mt::hw::ChipToolDriver>(chip_config);

    // Create gateway components
    mt::gateway::VanRegistry registry;
    mt::gateway::CASESessionPool sessions(driver);
    mt::gateway::FleetSubscriptionManager subscriptions(driver, sessions);
    mt::gateway::CommandRelay commands(driver, sessions);
    mt::gateway::OfflineBuffer buffer;
    mt::gateway::FabricManager fabrics;

    // Load default subscription rules for delivery vans
    subscriptions.loadDefaultVanRules();

    // Configure and start server
    mt::gateway::GatewayConfig config;
    config.api_port = port;

    mt::gateway::GatewayServer server(config, registry, sessions, subscriptions,
                                      commands, buffer, fabrics);

    auto result = server.start();
    if (!result.ok()) {
        MT_ERROR("gateway", "Failed to start: " + result.error().message);
        return 1;
    }

    MT_INFO("gateway", "Fleet Gateway running on port " + std::to_string(port));

    // Install signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Main event loop
    auto poll_interval = std::chrono::milliseconds(10);
    auto tick_interval = std::chrono::milliseconds(1000);
    auto last_tick = std::chrono::steady_clock::now();

    while (g_running) {
        server.poll();

        auto now = std::chrono::steady_clock::now();
        if (now - last_tick >= tick_interval) {
            auto tp = mt::TimePoint(mt::Duration(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count()));
            sessions.tick(tp);
            subscriptions.tick(tp);
            last_tick = now;
        }

        std::this_thread::sleep_for(poll_interval);
    }

    server.stop();
    MT_INFO("gateway", "Fleet Gateway stopped.");
    return 0;
}
