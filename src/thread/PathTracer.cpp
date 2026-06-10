#include "thread/PathTracer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>

namespace mt {

size_t fragmentCount(size_t payload_bytes, size_t frag_payload) {
    if (frag_payload == 0 || payload_bytes == 0) return 1;
    return (payload_bytes + frag_payload - 1) / frag_payload;  // ceil division
}

float TraceResult::cumulativeDelivery() const {
    float p = 1.0f;
    for (const auto& h : hops) {
        p *= (1.0f - h.expected_loss);
    }
    return p;
}

float TraceResult::deliveryForSize(size_t payload_bytes, size_t frag_payload) const {
    size_t n = fragmentCount(payload_bytes, frag_payload);
    return std::pow(cumulativeDelivery(), static_cast<float>(n));
}

float TraceResult::deliveryForSizeWithTrace(size_t payload_bytes,
                                            size_t trace_bytes_per_hop,
                                            size_t frag_payload) const {
    size_t inband = hops.size() * trace_bytes_per_hop;
    return deliveryForSize(payload_bytes + inband, frag_payload);
}

int TraceResult::worstHop() const {
    int worst = -1;
    float worst_loss = -1.0f;
    for (size_t i = 0; i < hops.size(); ++i) {
        if (hops[i].expected_loss > worst_loss) {
            worst_loss = hops[i].expected_loss;
            worst = static_cast<int>(i);
        }
    }
    return worst;
}

TraceResult tracePath(const MeshTopology& topo, NodeId src, NodeId dst) {
    TraceResult result;
    result.src = src;
    result.dst = dst;

    if (src >= MESH_NODES || dst >= MESH_NODES) return result;
    if (src == dst) {
        result.reachable = true;
        return result;
    }

    // BFS over up-links: fewest hops, deterministic neighbor order (ascending id).
    std::array<int, MESH_NODES> prev;
    std::array<bool, MESH_NODES> seen{};
    prev.fill(-1);

    std::queue<NodeId> q;
    q.push(src);
    seen[src] = true;

    while (!q.empty()) {
        NodeId cur = q.front();
        q.pop();
        if (cur == dst) break;
        for (NodeId nxt = 0; nxt < MESH_NODES; ++nxt) {
            if (seen[nxt] || nxt == cur) continue;
            if (!topo.getLinkParams(cur, nxt).link_up) continue;
            seen[nxt] = true;
            prev[nxt] = static_cast<int>(cur);
            q.push(nxt);
        }
    }

    if (!seen[dst]) return result;  // unreachable

    // Reconstruct path src -> dst by walking predecessors backward, then reverse.
    std::vector<NodeId> path;
    for (int at = static_cast<int>(dst); at != -1; at = prev[static_cast<size_t>(at)]) {
        path.push_back(static_cast<NodeId>(at));
        if (at == static_cast<int>(src)) break;
    }
    std::reverse(path.begin(), path.end());

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        TraceHop hop;
        hop.from = path[i];
        hop.to = path[i + 1];
        hop.params = topo.getLinkParams(hop.from, hop.to);
        hop.expected_loss = hop.params.link_up ? hop.params.base_loss_rate : 1.0f;
        result.hops.push_back(hop);
    }

    result.reachable = true;
    return result;
}

} // namespace mt
