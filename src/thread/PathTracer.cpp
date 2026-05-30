#include "thread/PathTracer.h"

#include <algorithm>
#include <array>
#include <queue>

namespace mt {

float TraceResult::cumulativeDelivery() const {
    float p = 1.0f;
    for (const auto& h : hops) {
        p *= (1.0f - h.expected_loss);
    }
    return p;
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
