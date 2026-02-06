#include "matter/Fabric.h"
#include <algorithm>

namespace mt {

Result<FabricIndex> FabricTable::addFabric(FabricId fabric_id, NodeId node_id, std::string label) {
    if (isFull()) return Error("Fabric table full (max " + std::to_string(MAX_FABRICS) + ")");

    FabricInfo info;
    info.fabric_index = next_index_++;
    info.fabric_id = fabric_id;
    info.node_id = node_id;
    info.label = std::move(label);
    fabrics_.push_back(std::move(info));
    return fabrics_.back().fabric_index;
}

Result<void> FabricTable::removeFabric(FabricIndex index) {
    auto it = std::find_if(fabrics_.begin(), fabrics_.end(),
        [index](const FabricInfo& f) { return f.fabric_index == index; });
    if (it == fabrics_.end()) return Error("Fabric not found");
    fabrics_.erase(it);
    return Result<void>::success();
}

const FabricInfo* FabricTable::getFabric(FabricIndex index) const {
    for (const auto& f : fabrics_) {
        if (f.fabric_index == index) return &f;
    }
    return nullptr;
}

FabricInfo* FabricTable::getFabric(FabricIndex index) {
    for (auto& f : fabrics_) {
        if (f.fabric_index == index) return &f;
    }
    return nullptr;
}

const FabricInfo* FabricTable::getFabricByNodeId(NodeId node_id) const {
    for (const auto& f : fabrics_) {
        if (f.node_id == node_id) return &f;
    }
    return nullptr;
}

} // namespace mt
