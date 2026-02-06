#pragma once

#include "core/Types.h"
#include "core/Result.h"
#include <vector>
#include <string>
#include <optional>

namespace mt {

static constexpr size_t MAX_FABRICS = 5;

struct FabricInfo {
    FabricIndex fabric_index = 0;
    FabricId fabric_id = 0;
    NodeId node_id = INVALID_NODE;
    std::string label;
    std::vector<uint8_t> noc;       // Simulated NOC (opaque)
    std::vector<uint8_t> root_cert; // Simulated root cert (opaque)
};

class FabricTable {
    std::vector<FabricInfo> fabrics_;
    FabricIndex next_index_ = 1;

public:
    Result<FabricIndex> addFabric(FabricId fabric_id, NodeId node_id, std::string label = "");
    Result<void> removeFabric(FabricIndex index);

    const FabricInfo* getFabric(FabricIndex index) const;
    FabricInfo* getFabric(FabricIndex index);

    const FabricInfo* getFabricByNodeId(NodeId node_id) const;

    size_t fabricCount() const { return fabrics_.size(); }
    bool isFull() const { return fabrics_.size() >= MAX_FABRICS; }

    const std::vector<FabricInfo>& fabrics() const { return fabrics_; }
};

} // namespace mt
