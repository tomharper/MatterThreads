#include "thread/MeshTopology.h"

namespace mt {

MeshTopology::MeshTopology() {
    // Default: all links up with good quality
    for (size_t i = 0; i < MESH_NODES; ++i) {
        for (size_t j = 0; j < MESH_NODES; ++j) {
            links_[i][j] = LinkParams{};
        }
    }
}

void MeshTopology::setLinkParams(NodeId from, NodeId to, const LinkParams& params) {
    if (from < MESH_NODES && to < MESH_NODES) {
        links_[from][to] = params;
    }
}

LinkParams MeshTopology::getLinkParams(NodeId from, NodeId to) const {
    if (from < MESH_NODES && to < MESH_NODES) {
        return links_[from][to];
    }
    return LinkParams{.link_up = false};
}

void MeshTopology::setLinkLoss(NodeId from, NodeId to, float loss) {
    if (from < MESH_NODES && to < MESH_NODES) {
        links_[from][to].base_loss_rate = loss;
    }
}

void MeshTopology::setLinkDown(NodeId from, NodeId to) {
    if (from < MESH_NODES && to < MESH_NODES) {
        links_[from][to].link_up = false;
    }
}

void MeshTopology::setLinkUp(NodeId from, NodeId to) {
    if (from < MESH_NODES && to < MESH_NODES) {
        links_[from][to].link_up = true;
    }
}

void MeshTopology::setLinkLatency(NodeId from, NodeId to, float mean_ms, float stddev_ms) {
    if (from < MESH_NODES && to < MESH_NODES) {
        links_[from][to].latency_mean_ms = mean_ms;
        links_[from][to].latency_stddev_ms = stddev_ms;
    }
}

void MeshTopology::setBidirectionalLoss(NodeId a, NodeId b, float loss) {
    setLinkLoss(a, b, loss);
    setLinkLoss(b, a, loss);
}

void MeshTopology::setBidirectionalDown(NodeId a, NodeId b) {
    setLinkDown(a, b);
    setLinkDown(b, a);
}

void MeshTopology::setBidirectionalUp(NodeId a, NodeId b) {
    setLinkUp(a, b);
    setLinkUp(b, a);
}

MeshTopology MeshTopology::fullyConnected() {
    MeshTopology t;
    // Default constructor already sets all links up with good quality
    // Phone (3) connects only to BR (0) via backhaul — no mesh links to 1,2
    t.setBidirectionalDown(3, 1);
    t.setBidirectionalDown(3, 2);
    // Phone-to-BR link models Wi-Fi/cellular backhaul (higher latency)
    LinkParams backhaul;
    backhaul.latency_mean_ms = 50.0f;
    backhaul.latency_stddev_ms = 20.0f;
    backhaul.lqi = 255;
    backhaul.rssi = -40;
    t.setLinkParams(3, 0, backhaul);
    t.setLinkParams(0, 3, backhaul);
    return t;
}

MeshTopology MeshTopology::linearChain() {
    MeshTopology t;
    // 0 <-> 1 and 1 <-> 2 are good; 0 <-> 2 has no direct link
    t.setBidirectionalDown(0, 2);
    // Phone (3) connects only to BR (0) via backhaul
    t.setBidirectionalDown(3, 1);
    t.setBidirectionalDown(3, 2);
    LinkParams backhaul;
    backhaul.latency_mean_ms = 50.0f;
    backhaul.latency_stddev_ms = 20.0f;
    backhaul.lqi = 255;
    backhaul.rssi = -40;
    t.setLinkParams(3, 0, backhaul);
    t.setLinkParams(0, 3, backhaul);
    return t;
}

MeshTopology MeshTopology::starFromLeader() {
    MeshTopology t;
    // 0 is the hub; 1 <-> 2 has no direct link
    t.setBidirectionalDown(1, 2);
    // Phone (3) connects only to BR (0) via backhaul
    t.setBidirectionalDown(3, 1);
    t.setBidirectionalDown(3, 2);
    LinkParams backhaul;
    backhaul.latency_mean_ms = 50.0f;
    backhaul.latency_stddev_ms = 20.0f;
    backhaul.lqi = 255;
    backhaul.rssi = -40;
    t.setLinkParams(3, 0, backhaul);
    t.setLinkParams(0, 3, backhaul);
    return t;
}

MeshTopology MeshTopology::vanWithPhone() {
    MeshTopology t;
    // Van topology: linear chain (cab wall blocks 0↔2) + phone on backhaul
    t.setBidirectionalDown(0, 2);
    t.setBidirectionalDown(3, 1);
    t.setBidirectionalDown(3, 2);

    // Backhaul: phone to BR via cellular (high latency, occasional loss)
    LinkParams cellular_backhaul;
    cellular_backhaul.latency_mean_ms = 120.0f;   // Cellular RTT
    cellular_backhaul.latency_stddev_ms = 60.0f;   // High jitter
    cellular_backhaul.base_loss_rate = 0.02f;       // 2% base loss
    cellular_backhaul.lqi = 255;
    cellular_backhaul.rssi = -30;
    t.setLinkParams(3, 0, cellular_backhaul);
    t.setLinkParams(0, 3, cellular_backhaul);

    // Mesh links: BR-to-Relay and Relay-to-EndDevice
    LinkParams cab_to_relay;
    cab_to_relay.latency_mean_ms = 8.0f;
    cab_to_relay.lqi = 200;
    cab_to_relay.rssi = -45;
    t.setLinkParams(0, 1, cab_to_relay);
    t.setLinkParams(1, 0, cab_to_relay);

    LinkParams relay_to_sensor;
    relay_to_sensor.latency_mean_ms = 6.0f;
    relay_to_sensor.lqi = 180;
    relay_to_sensor.rssi = -55;
    t.setLinkParams(1, 2, relay_to_sensor);
    t.setLinkParams(2, 1, relay_to_sensor);

    return t;
}

} // namespace mt
