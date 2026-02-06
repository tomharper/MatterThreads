#include "matter/DataModel.h"

namespace mt {

void DataModel::registerEndpoint(EndpointId ep, const std::vector<ClusterId>& clusters) {
    for (auto cluster : clusters) {
        store_[ep][cluster]; // Create empty cluster entry
    }
}

void DataModel::setAttribute(const AttributePath& path, AttributeValue value) {
    store_[path.endpoint_id][path.cluster_id][path.attribute_id] = std::move(value);
}

Result<AttributeValue> DataModel::readAttribute(const AttributePath& path) const {
    auto ep_it = store_.find(path.endpoint_id);
    if (ep_it == store_.end()) return Error("Endpoint not found: " + std::to_string(path.endpoint_id));

    auto cl_it = ep_it->second.find(path.cluster_id);
    if (cl_it == ep_it->second.end()) return Error("Cluster not found");

    auto at_it = cl_it->second.find(path.attribute_id);
    if (at_it == cl_it->second.end()) return Error("Attribute not found");

    return at_it->second;
}

Result<void> DataModel::writeAttribute(const AttributePath& path, const AttributeValue& value) {
    auto ep_it = store_.find(path.endpoint_id);
    if (ep_it == store_.end()) return Error("Endpoint not found");

    auto cl_it = ep_it->second.find(path.cluster_id);
    if (cl_it == ep_it->second.end()) return Error("Cluster not found");

    cl_it->second[path.attribute_id] = value;

    for (const auto& cb : change_callbacks_) {
        cb(path, value);
    }

    return Result<void>::success();
}

bool DataModel::hasEndpoint(EndpointId ep) const {
    return store_.find(ep) != store_.end();
}

bool DataModel::hasCluster(EndpointId ep, ClusterId cluster) const {
    auto ep_it = store_.find(ep);
    if (ep_it == store_.end()) return false;
    return ep_it->second.find(cluster) != ep_it->second.end();
}

bool DataModel::hasAttribute(const AttributePath& path) const {
    auto result = readAttribute(path);
    return result.ok();
}

DataModel DataModel::lightBulb() {
    DataModel dm;
    // Endpoint 0: Root (Descriptor, BasicInfo)
    dm.registerEndpoint(0, {Clusters::Descriptor, Clusters::BasicInfo});

    // Endpoint 1: Light
    dm.registerEndpoint(1, {Clusters::OnOff, Clusters::LevelControl, Clusters::ColorControl});
    dm.setAttribute({1, Clusters::OnOff, Attributes::OnOff_OnOff}, false);
    dm.setAttribute({1, Clusters::LevelControl, Attributes::Level_CurrentLevel}, uint32_t(254));
    return dm;
}

DataModel DataModel::doorLock() {
    DataModel dm;
    dm.registerEndpoint(0, {Clusters::Descriptor, Clusters::BasicInfo});
    dm.registerEndpoint(1, {Clusters::DoorLock});
    // LockState: 1 = Locked
    dm.setAttribute({1, Clusters::DoorLock, 0x0000}, uint32_t(1));
    return dm;
}

DataModel DataModel::thermostat() {
    DataModel dm;
    dm.registerEndpoint(0, {Clusters::Descriptor, Clusters::BasicInfo});
    dm.registerEndpoint(1, {Clusters::Thermostat});
    dm.setAttribute({1, Clusters::Thermostat, Attributes::Thermo_LocalTemp}, int32_t(2200)); // 22.00 C
    dm.setAttribute({1, Clusters::Thermostat, Attributes::Thermo_SetpointHeat}, int32_t(2000));
    dm.setAttribute({1, Clusters::Thermostat, Attributes::Thermo_SetpointCool}, int32_t(2600));
    return dm;
}

} // namespace mt
