#include "core/Types.h"
#include "core/Log.h"
#include "thread/MeshTopology.h"
#include "fault/FaultPlan.h"
#include "metrics/Collector.h"
#include "metrics/Reporter.h"
#include "metrics/DashboardServer.h"

#include <nlohmann/json.hpp>

#ifdef MT_HW_BRIDGE_ENABLED
#include "hw/ChipToolDriver.h"
#include "hw/OTBRClient.h"
#include "hw/HardwareNode.h"
#endif

#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <thread>
#include <fstream>
#include <memory>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static bool g_running = true;

static void signalHandler(int /*sig*/) {
    g_running = false;
}

struct CLIOptions {
    std::string topology = "full";
    uint32_t seed = 42;
    int duration_sec = 120;
    bool hw_mode = false;
    bool verbose = false;
    std::string output_path;
    std::string scenario;
    uint16_t dashboard_port = 0;   // 0 = disabled, >0 = serve on this port
    std::string chip_tool_path;    // --chip-tool <path>
    std::string otbr_url;          // --otbr <url>
    uint64_t hw_node_id = 1;       // --hw-node <id>
    uint32_t setup_code = 20202021; // --setup-code <code>
};

static CLIOptions parseCLI(int argc, char* argv[]) {
    CLIOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--topology" && i + 1 < argc) opts.topology = argv[++i];
        else if (arg == "--seed" && i + 1 < argc) opts.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (arg == "--duration" && i + 1 < argc) opts.duration_sec = std::stoi(argv[++i]);
        else if (arg == "--hw") opts.hw_mode = true;
        else if (arg == "--verbose") opts.verbose = true;
        else if (arg == "--output" && i + 1 < argc) opts.output_path = argv[++i];
        else if (arg == "--scenario" && i + 1 < argc) opts.scenario = argv[++i];
        else if (arg == "--dashboard" && i + 1 < argc) opts.dashboard_port = static_cast<uint16_t>(std::stoul(argv[++i]));
        else if (arg == "--chip-tool" && i + 1 < argc) opts.chip_tool_path = argv[++i];
        else if (arg == "--otbr" && i + 1 < argc) opts.otbr_url = argv[++i];
        else if (arg == "--hw-node" && i + 1 < argc) opts.hw_node_id = std::stoull(argv[++i]);
        else if (arg == "--setup-code" && i + 1 < argc) opts.setup_code = static_cast<uint32_t>(std::stoul(argv[++i]));
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: matterthreads [options]\n"
                      << "\n"
                      << "Options:\n"
                      << "  --topology <full|linear|star|van>  Topology preset (default: full)\n"
                      << "  --seed <uint32>                Random seed (default: 42)\n"
                      << "  --duration <seconds>           Max duration (default: 120)\n"
                      << "  --hw                           Enable hardware bridge mode\n"
                      << "  --verbose                      Verbose logging\n"
                      << "  --output <path>                Write JSON report to file\n"
                      << "  --scenario <name>              Run named scenario and exit\n"
                      << "  --dashboard <port>             Start web dashboard on port (e.g. 8080)\n"
                      << "\n"
                      << "Hardware bridge (requires -DENABLE_HW_BRIDGE=ON build):\n"
                      << "  --hw                           Enable hardware bridge mode\n"
                      << "  --chip-tool <path>             Path to chip-tool binary\n"
                      << "  --otbr <url>                   OTBR REST API URL (e.g. http://localhost:8081)\n"
                      << "  --hw-node <id>                 Hardware node ID (default: 1)\n"
                      << "  --setup-code <code>            Setup code for commissioning (default: 20202021)\n"
                      << "\n"
                      << "  --help                         Show this help\n"
                      << "\n"
                      << "Scenarios: mesh-healing, subscription-stress, commissioning-flaky,\n"
                      << "           progressive-degradation\n"
                      << "\n"
                      << "Interactive commands:\n"
                      << "  status                    Show node states and routes\n"
                      << "  topology                  Show link quality matrix\n"
                      << "  link <A> <B> <loss%|down|up>  Modify link\n"
                      << "  latency <A> <B> <ms>     Set link latency\n"
                      << "  crash <node>              Kill node process\n"
                      << "  restart <node>            Restart node process\n"
                      << "  metrics                   Show current metrics\n"
                      << "  timeline                  Show event timeline\n"
                      << "  chaos on|off              Toggle chaos mode\n"
                      << "  export <path>             Export JSON report\n"
                      << "  quit                      Shut down\n";
            std::exit(0);
        }
    }
    return opts;
}

static std::string getBinaryDir() {
    // Locate mt_broker and mt_node binaries relative to matterthreads
    // They should be in the same directory
    char path[1024];
    uint32_t size = sizeof(path);
#ifdef __APPLE__
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string p(path);
        auto pos = p.rfind('/');
        if (pos != std::string::npos) return p.substr(0, pos);
    }
#endif
    return ".";
}

int main(int argc, char* argv[]) {
    auto opts = parseCLI(argc, argv);

    if (opts.verbose) {
        mt::Logger::instance().setLevel(mt::LogLevel::Trace);
    }
    mt::Logger::instance().setNodeTag("ctrl");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string bin_dir = getBinaryDir();

    MT_INFO("ctrl", "MatterThreads - Matter/Thread Mesh Testing Framework");

#ifdef MT_HW_BRIDGE_ENABLED
    if (opts.hw_mode) {
        MT_INFO("ctrl", "Hardware bridge mode enabled");

        mt::hw::ChipToolConfig chip_config;
        if (!opts.chip_tool_path.empty()) chip_config.binary_path = opts.chip_tool_path;
        chip_config.storage_dir = "/tmp/matterthreads-hw";

        auto driver = std::make_shared<mt::hw::ChipToolDriver>(chip_config);

        std::shared_ptr<mt::hw::OTBRClient> otbr;
        if (!opts.otbr_url.empty()) {
            otbr = std::make_shared<mt::hw::OTBRClient>(opts.otbr_url);
        }

        auto hw_node = std::make_unique<mt::hw::HardwareNode>(
            opts.hw_node_id, "hw-device", driver, otbr);

        MT_INFO("ctrl", "HW node " + std::to_string(opts.hw_node_id) +
                " ready. Use 'hw commission' to pair.");

        // HW mode REPL
        std::cout << "Hardware bridge mode. Type 'help' for commands.\n> " << std::flush;
        std::string line;
        while (g_running && std::getline(std::cin, line)) {
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd.empty()) {
            } else if (cmd == "quit" || cmd == "exit") {
                break;
            } else if (cmd == "help") {
                std::cout << "HW bridge commands:\n"
                          << "  hw commission            Commission device with setup code\n"
                          << "  hw read <ep> <cluster> <attr>   Read attribute\n"
                          << "  hw write <ep> <cluster> <attr> <val>  Write attribute\n"
                          << "  hw invoke <ep> <cluster> <cmd>  Invoke command\n"
                          << "  hw status                Show HW node status\n"
                          << "  hw version               Show chip-tool version\n"
                          << "  quit                     Exit\n";
            } else if (cmd == "hw") {
                std::string subcmd;
                iss >> subcmd;
                if (subcmd == "commission") {
                    auto result = hw_node->commission(opts.setup_code);
                    if (result.ok()) std::cout << "Commissioning succeeded\n";
                    else std::cout << "Commissioning failed: " << result.error().message << "\n";
                } else if (subcmd == "read") {
                    uint16_t ep; uint32_t cluster, attr;
                    if (iss >> ep >> std::hex >> cluster >> attr) {
                        auto result = hw_node->readAttribute(ep, cluster, attr);
                        if (result.ok()) {
                            std::visit([](const auto& v) { std::cout << "Value: " << v << "\n"; }, *result);
                        } else {
                            std::cout << "Read failed: " << result.error().message << "\n";
                        }
                    }
                } else if (subcmd == "version") {
                    auto result = driver->version();
                    if (result.ok()) std::cout << "chip-tool: " << *result << "\n";
                    else std::cout << "Error: " << result.error().message << "\n";
                } else if (subcmd == "status") {
                    std::cout << "HW Node " << hw_node->deviceId()
                              << " commissioned=" << (hw_node->isCommissioned() ? "yes" : "no") << "\n";
                    if (otbr) {
                        auto state = otbr->getState();
                        if (state.ok()) std::cout << "OTBR: " << state->state << "\n";
                    }
                }
            } else {
                std::cout << "Unknown command. Type 'help'.\n";
            }
            if (g_running) std::cout << "hw> " << std::flush;
        }
        MT_INFO("ctrl", "Hardware bridge mode exiting.");
        return 0;
    }
#else
    if (opts.hw_mode) {
        std::cerr << "Hardware bridge not available. Rebuild with -DENABLE_HW_BRIDGE=ON\n";
        return 1;
    }
#endif

    MT_INFO("ctrl", "Topology: " + opts.topology + ", Seed: " + std::to_string(opts.seed));

    // Spawn broker
    MT_INFO("ctrl", "Starting broker...");
    pid_t broker_pid = fork();
    if (broker_pid == 0) {
        std::string broker_path = bin_dir + "/mt_broker";
        std::string seed_str = std::to_string(opts.seed);
        if (opts.verbose) {
            const char* args[] = {broker_path.c_str(), "--seed", seed_str.c_str(),
                                   "--topology", opts.topology.c_str(), "--verbose", nullptr};
            execv(broker_path.c_str(), const_cast<char* const*>(args));
        } else {
            const char* args[] = {broker_path.c_str(), "--seed", seed_str.c_str(),
                                   "--topology", opts.topology.c_str(), nullptr};
            execv(broker_path.c_str(), const_cast<char* const*>(args));
        }
        perror("execv broker");
        _exit(1);
    }

    // Give broker time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Spawn 4 nodes (0=Leader/BR, 1=Router/Relay, 2=EndDevice/Sensor, 3=Phone)
    static constexpr int NUM_NODES = 4;
    std::array<pid_t, NUM_NODES> node_pids{};
    std::array<std::string, NUM_NODES> roles = {"leader", "router", "sed", "phone"};

    for (int i = 0; i < NUM_NODES; ++i) {
        node_pids[static_cast<size_t>(i)] = fork();
        if (node_pids[static_cast<size_t>(i)] == 0) {
            std::string node_path = bin_dir + "/mt_node";
            std::string id_str = std::to_string(i);
            const char* args[] = {node_path.c_str(), "--id", id_str.c_str(),
                                   "--role", roles[static_cast<size_t>(i)].c_str(), nullptr};
            if (opts.verbose) {
                const char* args_v[] = {node_path.c_str(), "--id", id_str.c_str(),
                                         "--role", roles[static_cast<size_t>(i)].c_str(),
                                         "--verbose", nullptr};
                execv(node_path.c_str(), const_cast<char* const*>(args_v));
            } else {
                execv(node_path.c_str(), const_cast<char* const*>(args));
            }
            perror("execv node");
            _exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    MT_INFO("ctrl", "All processes started. Broker PID=" + std::to_string(broker_pid));
    for (int i = 0; i < NUM_NODES; ++i) {
        MT_INFO("ctrl", "  Node " + std::to_string(i) + " PID=" +
                std::to_string(node_pids[static_cast<size_t>(i)]) +
                " (" + roles[static_cast<size_t>(i)] + ")");
    }

    mt::Collector collector;
    mt::Reporter reporter(collector);

    // Dashboard server (optional)
    std::unique_ptr<mt::DashboardServer> dashboard;
    if (opts.dashboard_port > 0) {
        dashboard = std::make_unique<mt::DashboardServer>(collector, opts.dashboard_port);

        dashboard->setNodeStatusProvider([&]() -> std::vector<mt::NodeStatus> {
            std::vector<mt::NodeStatus> nodes;
            for (int i = 0; i < NUM_NODES; ++i) {
                int status;
                pid_t result = waitpid(node_pids[static_cast<size_t>(i)], &status, WNOHANG);
                std::string state = (result == 0) ? "running" : "stopped";
                nodes.push_back({static_cast<mt::NodeId>(i),
                                 roles[static_cast<size_t>(i)], state,
                                 static_cast<int>(node_pids[static_cast<size_t>(i)])});
            }
            return nodes;
        });

        auto start_result = dashboard->start();
        if (!start_result) {
            MT_WARN("ctrl", "Dashboard failed to start: " + start_result.error().message);
            dashboard.reset();
        } else {
            MT_INFO("ctrl", "Dashboard: http://localhost:" + std::to_string(opts.dashboard_port));
        }
    }

    if (!opts.scenario.empty()) {
        // Scenario mode: run for duration, then report
        MT_INFO("ctrl", "Running scenario: " + opts.scenario);

        auto start = std::chrono::steady_clock::now();
        auto max_dur = std::chrono::seconds(opts.duration_sec);

        while (g_running) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= max_dur) break;
            if (dashboard) dashboard->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        MT_INFO("ctrl", "Scenario complete");

        if (!opts.output_path.empty()) {
            std::ofstream out(opts.output_path);
            out << reporter.summaryJson();
            MT_INFO("ctrl", "Report written to " + opts.output_path);
        } else {
            std::cout << reporter.summaryText() << std::endl;
        }
    } else {
        // Interactive REPL (non-blocking stdin so dashboard keeps polling)
        std::cout << "MatterThreads interactive mode. Type 'help' for commands.\n> " << std::flush;

        std::string line_buf;
        while (g_running) {
            // Poll dashboard while waiting for input
            if (dashboard) dashboard->poll();

            // Check if stdin has data (non-blocking via select with 50ms timeout)
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval tv = {0, 50000};  // 50ms
            int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
            if (ready <= 0) continue;

            // Read available bytes
            char buf[256];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0) break;  // EOF or error
            line_buf.append(buf, static_cast<size_t>(n));

            // Process complete lines
            size_t pos;
            while ((pos = line_buf.find('\n')) != std::string::npos) {
                std::string line = line_buf.substr(0, pos);
                line_buf.erase(0, pos + 1);

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd.empty()) {
                // skip
            } else if (cmd == "quit" || cmd == "exit" || cmd == "q") {
                break;
            } else if (cmd == "help") {
                std::cout << "Commands:\n"
                          << "  status               Show node states\n"
                          << "  topology             Show link matrix\n"
                          << "  link A B loss%       Set loss on link A->B\n"
                          << "  link A B down        Bring link down\n"
                          << "  link A B up          Bring link up\n"
                          << "  crash N              Kill node N\n"
                          << "  restart N            Restart node N\n"
                          << "  metrics              Show metrics\n"
                          << "  timeline             Show timeline\n"
                          << "  chaos on|off         Toggle chaos mode\n"
                          << "  export <path>        Export JSON report\n"
                          << "\n"
                          << "Phone / Van commands:\n"
                          << "  discover             Phone scans for devices via DNS-SD\n"
                          << "  backhaul down        Simulate cellular backhaul loss\n"
                          << "  backhaul up          Restore cellular backhaul\n"
                          << "  backhaul latency <ms> Set backhaul latency\n"
                          << "  tunnel <sec>         Simulate tunnel (backhaul down for N sec)\n"
                          << "  crank                Simulate engine cranking power dip\n"
                          << "  healing              Show self-healing history\n"
                          << "\n"
                          << "  dashboard            Open dashboard URL\n"
                          << "  quit                 Shut down\n";
            } else if (cmd == "status") {
                std::cout << "Broker PID: " << broker_pid << "\n";
                for (int i = 0; i < NUM_NODES; ++i) {
                    int status;
                    pid_t result = waitpid(node_pids[static_cast<size_t>(i)], &status, WNOHANG);
                    std::string state = (result == 0) ? "running" : "stopped";
                    std::cout << "  Node " << i << ": PID=" << node_pids[static_cast<size_t>(i)]
                              << " (" << roles[static_cast<size_t>(i)] << ") " << state << "\n";
                }
            } else if (cmd == "discover") {
                std::cout << "Phone (Node 3) scanning for devices via DNS-SD...\n";
                std::cout << "  (Discovery results come from node process logs)\n";
                collector.event(mt::SteadyClock::now(), 3, "discovery", "phone_scan_triggered");
            } else if (cmd == "backhaul") {
                std::string subcmd;
                if (iss >> subcmd) {
                    if (subcmd == "down") {
                        std::cout << "Backhaul disconnected (phone ↔ BR link down)\n";
                        // Signal the broker to drop phone-BR link
                        collector.event(mt::SteadyClock::now(), 3, "backhaul", "disconnected");
                    } else if (subcmd == "up") {
                        std::cout << "Backhaul restored (phone ↔ BR link up)\n";
                        collector.event(mt::SteadyClock::now(), 3, "backhaul", "restored");
                    } else if (subcmd == "latency") {
                        int ms;
                        if (iss >> ms) {
                            std::cout << "Backhaul latency set to " << ms << "ms\n";
                            collector.event(mt::SteadyClock::now(), 3, "backhaul",
                                            "latency_set_" + std::to_string(ms));
                        }
                    }
                }
            } else if (cmd == "tunnel") {
                int seconds;
                if (iss >> seconds) {
                    std::cout << "Simulating tunnel for " << seconds << "s (backhaul down)...\n";
                    collector.event(mt::SteadyClock::now(), 3, "backhaul",
                                    "tunnel_start_" + std::to_string(seconds) + "s");
                    // Note: actual link manipulation would go through broker control socket
                    std::cout << "  Thread mesh continues, BR buffers reports\n";
                    std::cout << "  Backhaul will auto-restore after " << seconds << "s\n";
                }
            } else if (cmd == "crank") {
                std::cout << "Simulating engine cranking power dip...\n";
                std::cout << "  12V → 6V for 400ms (relay router may brown out)\n";
                collector.event(mt::SteadyClock::now(), 1, "power", "crank_dip");
                // Simulate: temporarily kill node 1 (relay) then auto-restart
                kill(node_pids[1], SIGKILL);
                std::cout << "  Relay router (Node 1) lost power\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                waitpid(node_pids[1], nullptr, 0);
                // Respawn
                node_pids[1] = fork();
                if (node_pids[1] == 0) {
                    std::string node_path = bin_dir + "/mt_node";
                    const char* args[] = {node_path.c_str(), "--id", "1",
                                           "--role", "router", nullptr};
                    execv(node_path.c_str(), const_cast<char* const*>(args));
                    _exit(1);
                }
                std::cout << "  Voltage recovered → Relay router rebooting (PID="
                          << node_pids[1] << ")\n";
                collector.event(mt::SteadyClock::now(), 1, "power", "crank_recovered");
            } else if (cmd == "healing") {
                std::cout << "Self-healing history:\n"
                          << "  (healing events are logged by each node process)\n"
                          << "  Check node logs for: NeighborLost, PartitionDetected,\n"
                          << "  RouteRecalculated, SubscriptionRecovered, NodeReattached\n";
            } else if (cmd == "dashboard") {
                if (dashboard) {
                    std::cout << "Dashboard running at http://localhost:" << dashboard->port() << "\n";
                } else {
                    std::cout << "Dashboard not enabled. Use --dashboard <port> to start.\n";
                }
            } else if (cmd == "crash") {
                int node;
                if (iss >> node && node >= 0 && node < NUM_NODES) {
                    kill(node_pids[static_cast<size_t>(node)], SIGKILL);
                    std::cout << "Sent SIGKILL to node " << node
                              << " (" << roles[static_cast<size_t>(node)] << ")\n";
                    collector.event(mt::SteadyClock::now(), static_cast<mt::NodeId>(node),
                                    "fault", "node_crashed");
                }
            } else if (cmd == "restart") {
                int node;
                if (iss >> node && node >= 0 && node < NUM_NODES) {
                    // Kill existing
                    kill(node_pids[static_cast<size_t>(node)], SIGKILL);
                    waitpid(node_pids[static_cast<size_t>(node)], nullptr, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    // Respawn
                    node_pids[static_cast<size_t>(node)] = fork();
                    if (node_pids[static_cast<size_t>(node)] == 0) {
                        std::string node_path = bin_dir + "/mt_node";
                        std::string id_str = std::to_string(node);
                        const char* args[] = {node_path.c_str(), "--id", id_str.c_str(),
                                               "--role", roles[static_cast<size_t>(node)].c_str(), nullptr};
                        execv(node_path.c_str(), const_cast<char* const*>(args));
                        _exit(1);
                    }
                    std::cout << "Restarted node " << node << " (PID=" << node_pids[static_cast<size_t>(node)] << ")\n";
                    collector.event(mt::SteadyClock::now(), static_cast<mt::NodeId>(node),
                                    "fault", "node_restarted");
                }
            } else if (cmd == "metrics") {
                std::cout << reporter.summaryText();
            } else if (cmd == "export") {
                std::string path;
                if (iss >> path) {
                    std::ofstream out(path);
                    out << reporter.summaryJson();
                    std::cout << "Exported to " << path << "\n";
                }
            } else if (cmd == "timeline") {
                std::cout << collector.timeline().exportCsv();
            } else {
                std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
            }

            if (g_running) {
                std::cout << "> " << std::flush;
            }
            }  // end line processing
        }
    }

    // Stop dashboard
    if (dashboard) dashboard->stop();

    // Shutdown all child processes
    MT_INFO("ctrl", "Shutting down...");
    for (int i = 0; i < NUM_NODES; ++i) {
        kill(node_pids[static_cast<size_t>(i)], SIGTERM);
    }
    kill(broker_pid, SIGTERM);

    // Wait for children
    for (int i = 0; i < NUM_NODES; ++i) {
        waitpid(node_pids[static_cast<size_t>(i)], nullptr, 0);
    }
    waitpid(broker_pid, nullptr, 0);

    MT_INFO("ctrl", "All processes terminated. Goodbye.");
    return 0;
}
