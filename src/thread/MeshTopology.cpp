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
    return t;
}

MeshTopology MeshTopology::linearChain() {
    MeshTopology t;
    // 0 <-> 1 and 1 <-> 2 are good; 0 <-> 2 has no direct link
    t.setBidirectionalDown(0, 2);
    return t;
}

MeshTopology MeshTopology::starFromLeader() {
    MeshTopology t;
    // 0 is the hub; 1 <-> 2 has no direct link
    t.setBidirectionalDown(1, 2);
    return t;
}

} // namespace mt
