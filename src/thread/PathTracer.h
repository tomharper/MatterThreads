#pragma once

#include "core/Types.h"
#include "net/Channel.h"          // LinkParams
#include "thread/MeshTopology.h"  // MeshTopology, MESH_NODES
#include <vector>

namespace mt {

// One link traversed along a traced path.
struct TraceHop {
    NodeId from = 0;
    NodeId to = 0;
    LinkParams params{};      // modeled link quality for from->to
    float expected_loss = 0;  // link_up ? base_loss_rate : 1.0
};

// Result of tracing a path through the mesh's link-quality graph.
//
// This is the offline / predictive analogue of the diagnostic counter
// differencing you'd do on real hardware: instead of querying each node's
// tx/rx counters, we read the (sim-authoritative) per-link quality and
// compute the route plus where loss concentrates. In the sim the link
// matrix IS ground truth, so this localizes the weakest hop exactly.
struct TraceResult {
    bool reachable = false;
    NodeId src = 0;
    NodeId dst = 0;
    std::vector<TraceHop> hops;

    // End-to-end delivery probability: product of per-hop (1 - expected_loss).
    float cumulativeDelivery() const;
    // Index into hops of the weakest link (highest expected loss); -1 if none.
    int worstHop() const;
    // Number of links traversed (0 when src == dst).
    size_t hopCount() const { return hops.size(); }
};

// Least-hop path over a directed link-quality graph. An edge from->to exists
// when topo.getLinkParams(from,to).link_up is true. Mirrors what the
// distance-vector routing converges to (minimize hop count) with deterministic
// tie-breaking by ascending node id. Returns reachable=false (empty hops) when
// no path exists; reachable=true with zero hops when src == dst.
TraceResult tracePath(const MeshTopology& topo, NodeId src, NodeId dst);

} // namespace mt
