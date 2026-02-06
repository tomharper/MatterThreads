#include "net/Broker.h"
#include "core/Log.h"
#include <cstdlib>
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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--verbose") {
            mt::Logger::instance().setLevel(mt::LogLevel::Trace);
        }
    }

    mt::Broker broker(seed);
    g_broker = &broker;

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

    return 0;
}
