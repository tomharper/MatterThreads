#pragma once

#include "core/Types.h"
#include "net/Channel.h"
#include <array>

namespace mt {

static constexpr size_t MESH_NODES = 4;  // 0=Leader/BR, 1=Router, 2=EndDevice, 3=Phone

class MeshTopology {
    std::array<std::array<LinkParams, MESH_NODES>, MESH_NODES> links_;

public:
    MeshTopology();

    void setLinkParams(NodeId from, NodeId to, const LinkParams& params);
    LinkParams getLinkParams(NodeId from, NodeId to) const;

    void setLinkLoss(NodeId from, NodeId to, float loss);
    void setLinkDown(NodeId from, NodeId to);
    void setLinkUp(NodeId from, NodeId to);
    void setLinkLatency(NodeId from, NodeId to, float mean_ms, float stddev_ms = 1.0f);

    // Bidirectional convenience — sets both A->B and B->A
    void setBidirectionalLoss(NodeId a, NodeId b, float loss);
    void setBidirectionalDown(NodeId a, NodeId b);
    void setBidirectionalUp(NodeId a, NodeId b);

    // Presets
    static MeshTopology fullyConnected();
    static MeshTopology linearChain();      // 0 <-> 1 <-> 2, no direct 0 <-> 2
    static MeshTopology starFromLeader();   // 0 is hub; 1 and 2 connect through 0
    static MeshTopology vanWithPhone();     // Linear chain + phone on backhaul to BR
};

} // namespace mt
