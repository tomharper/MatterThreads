#include "thread/BorderRouter.h"
#include "core/Log.h"
#include <algorithm>

namespace mt {

bool BorderRouterProxy::addEntry(NodeId controller, NodeId device, RLOC16 device_rloc,
                                  SessionId session, TimePoint now) {
    // Check for existing entry (update instead of duplicate)
    for (auto& e : entries_) {
        if (e.controller_id == controller && e.device_id == device) {
            e.device_rloc16 = device_rloc;
            e.session_id = session;
            e.last_activity = now;
            e.active = true;
            MT_DEBUG("border-router", "Updated proxy: controller=" +
                     std::to_string(controller) + " → device=" + std::to_string(device));
            return true;
        }
    }

    if (isFull()) {
        ++rejected_count_;
        MT_WARN("border-router", "Proxy table full (" + std::to_string(max_entries_) +
                " entries), rejecting controller=" + std::to_string(controller) +
                " → device=" + std::to_string(device));
        return false;
    }

    ProxyEntry entry;
    entry.controller_id = controller;
    entry.device_id = device;
    entry.device_rloc16 = device_rloc;
    entry.session_id = session;
    entry.created_at = now;
    entry.last_activity = now;
    entries_.push_back(entry);

    MT_INFO("border-router", "Added proxy: controller=" + std::to_string(controller) +
            " → device=" + std::to_string(device) +
            " (RLOC16=0x" + std::to_string(device_rloc) + ")");
    return true;
}

void BorderRouterProxy::removeEntry(NodeId controller, NodeId device) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [controller, device](const ProxyEntry& e) {
            return e.controller_id == controller && e.device_id == device;
        });
    if (it != entries_.end()) {
        entries_.erase(it, entries_.end());
        MT_DEBUG("border-router", "Removed proxy: controller=" +
                 std::to_string(controller) + " → device=" + std::to_string(device));
    }
}

void BorderRouterProxy::removeDevice(NodeId device) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [device](const ProxyEntry& e) { return e.device_id == device; });
    if (it != entries_.end()) {
        MT_INFO("border-router", "Removed all proxy entries for device=" + std::to_string(device));
        entries_.erase(it, entries_.end());
    }
}

void BorderRouterProxy::removeController(NodeId controller) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [controller](const ProxyEntry& e) { return e.controller_id == controller; });
    if (it != entries_.end()) {
        MT_INFO("border-router", "Removed all proxy entries for controller=" + std::to_string(controller));
        entries_.erase(it, entries_.end());
    }
}

std::optional<RLOC16> BorderRouterProxy::resolveDevice(NodeId device) const {
    for (const auto& e : entries_) {
        if (e.device_id == device && e.active) {
            return e.device_rloc16;
        }
    }
    return std::nullopt;
}

void BorderRouterProxy::updateDeviceRLOC(NodeId device, RLOC16 new_rloc) {
    for (auto& e : entries_) {
        if (e.device_id == device) {
            if (e.device_rloc16 != new_rloc) {
                MT_INFO("border-router", "Device " + std::to_string(device) +
                        " RLOC updated: 0x" + std::to_string(e.device_rloc16) +
                        " → 0x" + std::to_string(new_rloc));
                e.device_rloc16 = new_rloc;
            }
        }
    }
}

void BorderRouterProxy::refreshFromRouting(const RoutingTable& routing, TimePoint now) {
    for (auto& e : entries_) {
        uint8_t router_id = getRouterId(e.device_rloc16);
        const auto& route = routing.getEntry(router_id);
        if (!route.reachable) {
            if (e.active) {
                MT_WARN("border-router", "Device " + std::to_string(e.device_id) +
                        " at RLOC16=0x" + std::to_string(e.device_rloc16) +
                        " — router " + std::to_string(router_id) + " unreachable");
                e.active = false;
            }
        } else {
            e.active = true;
        }
    }
    (void)now;
}

void BorderRouterProxy::touchSession(NodeId controller, NodeId device, TimePoint now) {
    for (auto& e : entries_) {
        if (e.controller_id == controller && e.device_id == device) {
            e.last_activity = now;
            return;
        }
    }
}

void BorderRouterProxy::expireIdle(TimePoint now) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [this, now](const ProxyEntry& e) {
            if (e.isIdle(now)) {
                MT_INFO("border-router", "Expired idle proxy: controller=" +
                        std::to_string(e.controller_id) + " → device=" +
                        std::to_string(e.device_id));
                ++expired_count_;
                return true;
            }
            return false;
        });
    entries_.erase(it, entries_.end());
}

} // namespace mt
