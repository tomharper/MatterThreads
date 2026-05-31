#include "net/Broker.h"
#include "thread/MeshTopology.h"
#include "core/Log.h"
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <string>

static mt::Broker* g_broker = nullptr;

static void signalHandler(int /*sig*/) {
    if (g_broker) g_broker->stop();
}

int main(int argc, char* argv[]) {
    mt::Logger::instance().setNodeTag("broker");

    uint32_t seed = 42;
    uint16_t port = mt::BROKER_PORT;
    std::string topology_name = "full";
    bool tracer = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--topology" && i + 1 < argc) {
            topology_name = argv[++i];
        } else if (arg == "--trace") {
            tracer = true;
        } else if (arg == "--verbose") {
            mt::Logger::instance().setLevel(mt::LogLevel::Trace);
        }
    }

    mt::Broker broker(seed);
    g_broker = &broker;

    if (tracer) {
        broker.enableTracer();
        MT_INFO("broker", "Tracer mode ON — recording ground-truth per-link counts");
    }

    // Apply topology preset
    if (topology_name == "linear") {
        broker.applyTopology(mt::MeshTopology::linearChain());
        MT_INFO("broker", "Topology: linear chain");
    } else if (topology_name == "star") {
        broker.applyTopology(mt::MeshTopology::starFromLeader());
        MT_INFO("broker", "Topology: star from leader");
    } else if (topology_name == "van") {
        broker.applyTopology(mt::MeshTopology::vanWithPhone());
        MT_INFO("broker", "Topology: van with phone (cellular backhaul)");
    } else {
        broker.applyTopology(mt::MeshTopology::fullyConnected());
        MT_INFO("broker", "Topology: fully connected");
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    auto result = broker.start(port);
    if (!result) {
        MT_ERROR("broker", "Failed to start: " + result.error().message);
        return 1;
    }

    broker.run();

    MT_INFO("broker", "Frames forwarded: " + std::to_string(broker.framesForwarded()) +
            ", dropped: " + std::to_string(broker.framesDropped()));

    // Tracer mode: dump the ground-truth per-link tally (the sim's privileged
    // observer view — what real hardware can't see passively).
    if (tracer) {
        MT_INFO("broker", "Tracer per-link ground truth (forwarded / dropped):");
        for (mt::NodeId a = 0; a < mt::MAX_NODES; ++a) {
            for (mt::NodeId b = 0; b < mt::MAX_NODES; ++b) {
                if (a == b) continue;
                auto s = broker.linkStat(a, b);
                uint64_t total = s.forwarded + s.dropped;
                if (total == 0) continue;
                double loss_pct = 100.0 * static_cast<double>(s.dropped) /
                                  static_cast<double>(total);
                char line[96];
                std::snprintf(line, sizeof(line), "  %u->%u: fwd=%llu drop=%llu loss=%.1f%%",
                              static_cast<unsigned>(a), static_cast<unsigned>(b),
                              static_cast<unsigned long long>(s.forwarded),
                              static_cast<unsigned long long>(s.dropped), loss_pct);
                MT_INFO("broker", line);
            }
        }
    }

    return 0;
}
