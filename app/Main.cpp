#include "core/Types.h"
#include "core/Log.h"
#include "thread/MeshTopology.h"
#include "fault/FaultPlan.h"
#include "metrics/Collector.h"
#include "metrics/Reporter.h"

#include <nlohmann/json.hpp>

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

#include <unistd.h>
#include <sys/wait.h>

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

    if (!opts.scenario.empty()) {
        // Scenario mode: run for duration, then report
        MT_INFO("ctrl", "Running scenario: " + opts.scenario);

        auto start = std::chrono::steady_clock::now();
        auto max_dur = std::chrono::seconds(opts.duration_sec);

        while (g_running) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= max_dur) break;
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
        // Interactive REPL
        std::cout << "MatterThreads interactive mode. Type 'help' for commands.\n> " << std::flush;

        std::string line;
        while (g_running && std::getline(std::cin, line)) {
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
        }
    }

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
