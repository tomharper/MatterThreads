#include "core/Types.h"
#include "core/Log.h"
#include "core/Clock.h"
#include "net/Socket.h"
#include "net/Frame.h"
#include "thread/ThreadNode.h"
#include "matter/DataModel.h"
#include "matter/Session.h"
#include "matter/Exchange.h"
#include "matter/SubscriptionManager.h"
#include "matter/InteractionModel.h"

#include <csignal>
#include <string>
#include <poll.h>

static bool g_running = true;

static void signalHandler(int /*sig*/) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    mt::NodeId node_id = 0;
    std::string role_str = "router";
    uint16_t broker_port = mt::BROKER_PORT;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) {
            node_id = static_cast<mt::NodeId>(std::stoi(argv[++i]));
        } else if (arg == "--role" && i + 1 < argc) {
            role_str = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            broker_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--verbose") {
            mt::Logger::instance().setLevel(mt::LogLevel::Trace);
        }
    }

    mt::Logger::instance().setNodeTag("node" + std::to_string(node_id));

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Connect to broker
    MT_INFO("node", "Connecting to broker on port " + std::to_string(broker_port));
    auto conn_result = mt::Socket::connect("127.0.0.1", broker_port);
    if (!conn_result) {
        MT_ERROR("node", "Failed to connect to broker: " + conn_result.error().message);
        return 1;
    }
    mt::Socket broker_conn = std::move(*conn_result);

    // Register with broker
    uint8_t reg[2] = {
        static_cast<uint8_t>(node_id & 0xFF),
        static_cast<uint8_t>((node_id >> 8) & 0xFF)
    };
    auto send_result = broker_conn.sendAll(reg, 2);
    if (!send_result) {
        MT_ERROR("node", "Failed to register: " + send_result.error().message);
        return 1;
    }

    MT_INFO("node", "Registered as node " + std::to_string(node_id));

    // Set up Thread node
    uint64_t ext_addr = 0x1000 + node_id;
    mt::DeviceMode mode = (role_str == "sed") ? mt::DeviceMode::MTD_SED : mt::DeviceMode::FTD;
    mt::ThreadNode thread_node(node_id, mode, ext_addr);

    mt::Clock clock;
    thread_node.setClock(&clock);

    // Wire frame sender to broker connection
    thread_node.setFrameSender([&broker_conn, node_id](const mt::MacFrame& frame) {
        auto payload = frame.serialize();
        mt::WireHeader hdr;
        hdr.src_node = node_id;
        hdr.dst_node = (frame.dst_addr == 0xFFFF) ? mt::BROADCAST_NODE : frame.dst_addr;
        hdr.payload_len = static_cast<uint16_t>(payload.size());

        uint8_t hdr_buf[mt::WireHeader::SIZE];
        hdr.serialize(hdr_buf);

        broker_conn.sendAll(hdr_buf, mt::WireHeader::SIZE);
        if (!payload.empty()) {
            broker_conn.sendAll(payload.data(), payload.size());
        }
    });

    // Initialize node role
    thread_node.attach();
    if (node_id == 0) {
        thread_node.becomeLeader();
    } else {
        thread_node.promoteToRouter(static_cast<uint8_t>(node_id));
    }

    // Set up Matter stack
    mt::DataModel data_model = mt::DataModel::lightBulb();
    mt::SessionManager session_mgr;
    mt::ExchangeManager exchange_mgr;
    mt::SubscriptionManager sub_mgr;
    mt::InteractionModel im(data_model, session_mgr, sub_mgr, exchange_mgr);

    MT_INFO("node", "Running as " + role_str + " (" + thread_node.statusString() + ")");

    // Main loop
    struct pollfd pfd;
    pfd.fd = broker_conn.fd();
    pfd.events = POLLIN;

    while (g_running) {
        int ready = poll(&pfd, 1, 100); // 100ms timeout for tick

        if (ready > 0 && (pfd.revents & POLLIN)) {
            // Read frame from broker
            uint8_t hdr_buf[mt::WireHeader::SIZE];
            auto recv_result = broker_conn.recvAll(hdr_buf, mt::WireHeader::SIZE);
            if (!recv_result) {
                MT_ERROR("node", "Broker disconnected");
                break;
            }

            mt::WireHeader hdr = mt::WireHeader::deserialize(hdr_buf);
            if (hdr.magic != mt::WIRE_MAGIC) {
                MT_WARN("node", "Bad magic from broker");
                continue;
            }

            std::vector<uint8_t> payload(hdr.payload_len);
            if (hdr.payload_len > 0) {
                recv_result = broker_conn.recvAll(payload.data(), hdr.payload_len);
                if (!recv_result) break;
            }

            mt::MacFrame frame = mt::MacFrame::deserialize(payload.data(), payload.size());
            thread_node.onFrameReceived(frame);
        }

        if (pfd.revents & (POLLHUP | POLLERR)) {
            MT_ERROR("node", "Broker connection lost");
            break;
        }

        // Periodic tick
        thread_node.tick();
        im.tick(clock.now());
    }

    MT_INFO("node", "Shutting down");
    return 0;
}
