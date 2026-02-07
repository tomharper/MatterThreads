#pragma once

#include "core/Types.h"
#include "core/Clock.h"
#include "thread/Routing.h"
#include "thread/MLE.h"
#include "thread/Leader.h"
#include "thread/ChildTable.h"
#include "net/Frame.h"
#include "net/SelfHealing.h"
#include "net/Discovery.h"
#include <functional>
#include <string>
#include <vector>

namespace mt {

enum class DeviceRole : uint8_t {
    Detached = 0,
    EndDevice = 1,
    REED = 2,
    Router = 3,
    Leader = 4
};

enum class DeviceMode : uint8_t {
    FTD = 0,      // Full Thread Device
    MTD_MED = 1,  // Minimal Thread Device (receiver always on)
    MTD_SED = 2   // Sleepy End Device
};

inline const char* roleToString(DeviceRole role) {
    switch (role) {
        case DeviceRole::Detached:  return "Detached";
        case DeviceRole::EndDevice: return "EndDevice";
        case DeviceRole::REED:      return "REED";
        case DeviceRole::Router:    return "Router";
        case DeviceRole::Leader:    return "Leader";
    }
    return "Unknown";
}

class ThreadNode {
public:
    using FrameSender = std::function<void(const MacFrame&)>;

    ThreadNode(NodeId id, DeviceMode mode, uint64_t ext_addr);

    // Configuration
    NodeId id() const { return id_; }
    uint64_t extAddr() const { return ext_addr_; }
    DeviceRole role() const { return role_; }
    DeviceMode mode() const { return mode_; }
    RLOC16 rloc16() const { return rloc16_; }
    uint8_t routerId() const { return router_id_; }

    // Set callback for sending frames (wired to broker connection)
    void setFrameSender(FrameSender sender) { send_frame_ = std::move(sender); }

    // Thread lifecycle
    void attach();
    void detach();
    void promoteToRouter(uint8_t assigned_router_id);
    void becomeLeader();

    // Clock for time-based operations
    void setClock(Clock* clock) { clock_ = clock; }

    // Periodic tick — call regularly to process timers
    void tick();

    // Frame handling
    void onFrameReceived(const MacFrame& frame);

    // Direct access to internals for testing
    RoutingTable& routing() { return routing_; }
    const RoutingTable& routing() const { return routing_; }
    MLEEngine& mle() { return mle_; }
    LeaderData& leaderData() { return leader_data_; }
    ChildTable& childTable() { return child_table_; }
    SelfHealingEngine& healing() { return healing_; }
    const SelfHealingEngine& healing() const { return healing_; }

    // Discovery: register this node's services with the SRP server (Border Router)
    void registerServices(ServiceRegistry& registry, TimePoint now);

    // Send data to a destination RLOC16
    void sendData(RLOC16 dst, const std::vector<uint8_t>& payload);

    // Status
    std::string statusString() const;

private:
    void processMLEFrame(const MacFrame& frame);
    void processDataFrame(const MacFrame& frame);
    void sendMLEAdvertisement();

    NodeId id_;
    DeviceMode mode_;
    uint64_t ext_addr_;
    DeviceRole role_ = DeviceRole::Detached;
    RLOC16 rloc16_ = INVALID_RLOC16;
    uint8_t router_id_ = INVALID_ROUTER_ID;
    uint8_t seq_counter_ = 0;

    RoutingTable routing_;
    MLEEngine mle_;
    LeaderData leader_data_;
    ChildTable child_table_;

    Clock* clock_ = nullptr;
    FrameSender send_frame_;
    SelfHealingEngine healing_;

    // MLE timeout: if no advertisement from a neighbor within this window, consider them lost
    Duration neighbor_timeout_ = Duration(25000); // 2.5x advertisement interval
};

} // namespace mt
