# IPv6 Addressing and Device Discovery in Matter over Thread

## Overview

Thread is an IPv6-only mesh network built on IEEE 802.15.4 radio. Every Thread device has multiple IPv6 addresses, and Matter uses a multi-phase discovery protocol to find and communicate with devices. This document explains how the full stack works in production, what our simulation covers, and where the real-world bugs live.

---

## Thread IPv6 Addressing

Every Thread node is assigned at least three IPv6 addresses. Understanding the distinction between them is critical for debugging reachability issues.

### The Three Address Types

```
Type              Prefix          Derived From       Stable?    Scope
---------------------------------------------------------------------------
Link-Local        fe80::/64       EUI-64 MAC addr    Yes        Single hop
Mesh-Local EID    fd00::/64       EUI-64 MAC addr    Yes        Entire mesh
RLOC              fd00::ff:fe00:  Topology position   No        Entire mesh
```

### Link-Local Address (fe80::/64)

Derived from the device's EUI-64 hardware address using the modified EUI-64 algorithm (RFC 4291: flip bit 1 of the first byte).

```
EUI-64:        00:11:22:33:44:55:66:77
Modified IID:  02:11:22:33:44:55:66:77   (flipped U/L bit)
Link-Local:    fe80::0211:2233:4455:6677
```

Only usable for single-hop communication with direct radio neighbors. Used for:
- MLE (Mesh Link Establishment) advertisements
- Neighbor discovery
- Initial parent attachment

Our implementation: `AddressManager::computeLinkLocal()` in `src/thread/AddressManager.cpp`.

### Mesh-Local EID (fd00::/64)

The device's **stable identity** within the mesh. Same prefix as the RLOC, but the IID comes from the EUI-64 rather than the topology. This address does not change when the device switches parent routers or changes role.

```
Mesh-Local EID:  fd00::0211:2233:4455:6677
```

This is the address Matter uses to reach devices. When a phone sends a command to a light bulb, it ultimately targets the light bulb's Mesh-Local EID. The Thread routing layer maps this to the current RLOC for forwarding.

Our implementation: `AddressManager::computeMLEID()` in `src/thread/AddressManager.cpp`.

### RLOC (Routing Locator)

The topology-dependent address. Encodes the device's position in the mesh using a 16-bit RLOC16 value:

```
RLOC16 bit layout:
  Bits [15:10] = Router ID  (0-62, assigned by the Leader)
  Bits [9:0]   = Child ID   (0 = the router itself, 1+ = attached children)

Examples:
  Router ID=0, self:      RLOC16 = 0x0000  ->  fd00::ff:fe00:0000
  Router ID=1, self:      RLOC16 = 0x0400  ->  fd00::ff:fe00:0400
  Router ID=1, child #3:  RLOC16 = 0x0403  ->  fd00::ff:fe00:0403
  Router ID=2, self:      RLOC16 = 0x0800  ->  fd00::ff:fe00:0800
```

The RLOC changes whenever:
- A device is promoted from End Device to Router (gets a new Router ID)
- A Sleepy End Device re-attaches to a different parent router (new Child ID under new parent)
- Network partition and rejoin (may get a different Router ID)

Our implementation: `makeRLOC16()`, `getRouterId()`, `getChildId()` in `src/core/Types.h`.

### Address Lifecycle Example

```
1. Device powers on
   - Has EUI-64: 00:11:22:33:44:55:66:77
   - Computes link-local: fe80::0211:2233:4455:6677
   - Sends MLE Parent Request on link-local

2. Attaches to Router ID=1 as child #2
   - Gets RLOC16 = 0x0402
   - RLOC address: fd00::ff:fe00:0402
   - Mesh-Local EID: fd00::0211:2233:4455:6677 (stable)

3. Parent router goes down, re-attaches to Router ID=0 as child #5
   - RLOC16 changes to 0x0005
   - RLOC address: fd00::ff:fe00:0005 (CHANGED)
   - Mesh-Local EID: fd00::0211:2233:4455:6677 (UNCHANGED)

4. Promoted to Router, assigned Router ID=3
   - RLOC16 changes to 0x0C00
   - RLOC address: fd00::ff:fe00:0C00 (CHANGED again)
   - Mesh-Local EID: fd00::0211:2233:4455:6677 (still stable)
```

This is why Matter targets the Mesh-Local EID, not the RLOC. The EID is stable across topology changes. The Thread routing layer handles the EID-to-RLOC mapping transparently.

---

## 6LoWPAN Compression

Real Thread uses 6LoWPAN (RFC 6282) to compress IPv6 headers into the tiny IEEE 802.15.4 frames (127 bytes max payload). A full IPv6 header is 40 bytes. With 6LoWPAN:

```
Full IPv6 + UDP header:  40 + 8 = 48 bytes
6LoWPAN compressed:      2-7 bytes (typical)
Savings:                 ~85%
```

6LoWPAN achieves this by:
- Eliding the prefix when it matches the mesh-local prefix (known to all nodes)
- Deriving the IID from the 802.15.4 source/destination addresses in the MAC header
- Compressing common fields (traffic class, flow label, hop limit)
- Using context-based compression for known address prefixes

**Our simulation skips 6LoWPAN entirely.** We pass payloads directly in `MacFrame.payload` and use RLOC16 in the MAC frame header for routing. This is a deliberate simplification: 6LoWPAN compression bugs exist but are rare causes of Matter-level issues. The interesting bugs happen at the routing and discovery layers.

---

## Device Discovery

Matter uses a two-phase discovery model. Phase 1 finds new (uncommissioned) devices. Phase 2 finds previously commissioned devices for normal operation.

### Phase 1: Commissioning Discovery

When a user scans a Matter QR code or enters a setup code on their phone:

```
Phone (Wi-Fi/Ethernet)           Thread Border Router            Thread Device
        |                                |                            |
        |                                |  (BR is on both Wi-Fi      |
        |                                |   and Thread networks)      |
        |                                |                            |
   1. User scans QR code                 |                            |
        |                                |                            |
   2. Phone sends mDNS query             |                            |
      _matterc._udp.local         3. BR responds with              |
        |------- mDNS query ----------->|   its own IP:port           |
        |<------ mDNS response ---------|                            |
        |                                |                            |
   4. Phone opens UDP to BR              |                            |
      with discriminator from QR   5. BR proxies PASE msgs           |
        |------- PBKDFParamReq -------->|------- PBKDFParamReq ----->|
        |<------ PBKDFParamResp --------|<------ PBKDFParamResp -----|
        |------- PASE1 ---------------->|------- PASE1 ------------->|
        |<------ PASE2 -----------------|<------ PASE2 --------------|
        |------- PASE3 ---------------->|------- PASE3 ------------->|
        |                                |                            |
   6. PASE session established (phone <-> device, through BR)        |
        |                                |                            |
   7. Phone provisions Thread credentials + fabric via CASE          |
        |                                |                            |
   8. Device joins the Thread network as an operational node         |
```

**DNS-SD service type for commissioning:** `_matterc._udp.local`

The mDNS TXT record includes:
- `D` — Discriminator (12-bit, from QR code, used to identify the specific device)
- `VP` — Vendor ID + Product ID
- `CM` — Commissioning Mode (0 = not in commissioning, 1 = basic, 2 = enhanced)
- `DT` — Device Type
- `DN` — Device Name

### Phase 2: Operational Discovery

After commissioning, the device needs to be discoverable for normal operation. This uses SRP (Service Registration Protocol) on the Thread side and mDNS/DNS-SD on the Wi-Fi side.

```
Thread Device                    Border Router                    Phone
     |                                |                              |
  1. Device generates SRP             |                              |
     registration with its            |                              |
     operational info                 |                              |
     |                                |                              |
     |-- SRP Register -------------->|                              |
     |   Host: <MAC>.local            |                              |
     |   Service: _matter._tcp        |                              |
     |   Instance: <fabric>-<node>    |                              |
     |   TXT: SII, SAI, T            |                              |
     |                                |                              |
     |                           2. BR caches SRP entry             |
     |                              and advertises via mDNS          |
     |                                |                              |
     |                                |-- mDNS advertisement ------>|
     |                                |   _matter._tcp.local         |
     |                                |   Instance: ABCDEF-001       |
     |                                |   Target: <BR IP>:5540       |
     |                                |                              |
     |                                |                         3. Phone
     |                                |                            caches
     |                                |                            mDNS entry
     |                                |                              |
     |                                |<--- CASE Sigma1 ------------|
     |<--- CASE Sigma1 (proxied) -----|                              |
     |---- CASE Sigma2 ------------->|---- CASE Sigma2 ------------>|
     |                                |<--- CASE Sigma3 ------------|
     |<--- CASE Sigma3 (proxied) -----|                              |
     |                                |                              |
  4. CASE operational session established                            |
     |                                |                              |
     |<--- Read/Write/Subscribe/Invoke (all proxied through BR) ----|
```

**DNS-SD service type for operation:** `_matter._tcp.local`

The instance name is: `<compressed-fabric-id>-<node-id-hex>`

Example: Fabric ID 0x1234, Node ID 5 -> instance `1234-0000000000000005`

The mDNS TXT record includes:
- `SII` — Session Idle Interval (how long before a session is considered idle)
- `SAI` — Session Active Interval
- `T` — TCP supported flag

### SRP Registration Details

SRP (RFC 8415 derivative used by Thread) is how Thread devices register themselves with the Border Router's DNS-SD proxy. A device sends an SRP update containing:

```
SRP Update:
  Host:     <eui64>.local
  Address:  fd00::0211:2233:4455:6677  (Mesh-Local EID)
  Service:  _matter._tcp.local
  Instance: <compressed-fabric-id>-<node-id>
  Port:     5540 (Matter default)
  TXT:      SII=300, SAI=300, T=0
  Lease:    7200 seconds (default 2 hours)
  Key Lease: 1209600 seconds (14 days)
```

The Border Router:
1. Stores the SRP entry in its registry
2. Maps the device's Mesh-Local EID to its current RLOC (via the Thread routing table)
3. Advertises the service on the Wi-Fi/Ethernet interface via mDNS
4. Proxies incoming Matter connections from Wi-Fi into the Thread mesh

**Critical detail:** The SRP lease expires. If the device fails to renew (e.g., it was asleep too long, or the BR rebooted), the device becomes undiscoverable until it re-registers.

---

## Where Real Bugs Happen

The discovery and IPv6 addressing layers are where a large class of real-world Matter/Thread issues originate. These are the most common categories:

### 1. Stale SRP Registration

**Symptom:** "Device unreachable" in the Home app after it was working fine.

**Root cause:** The device's RLOC changed (parent router went down, device re-attached to a new parent), but the SRP registration still points to the old RLOC. The Border Router tries to proxy traffic to the old RLOC, which no longer routes to the device.

**Timeline:**
```
T=0     Device is at RLOC16=0x0402 (child of Router 1)
        SRP registered: EID -> RLOC 0x0402
        Phone can reach device via BR proxy

T=30s   Router 1 goes down
T=35s   Device re-attaches to Router 0 as child #5, RLOC16=0x0005
        SRP re-registration queued but not yet sent

T=35-45s  WINDOW OF UNREACHABILITY
          BR still has old SRP entry pointing to RLOC 0x0402
          Packets to 0x0402 are dropped (Router 1 is down)

T=45s   Device sends SRP update with new address info
        BR updates its proxy table
        Device reachable again
```

**How to simulate:** Use our `crash` command to kill a router node, measure the time window where the device's address is stale.

### 2. mDNS Cache Staleness

**Symptom:** Device responds from one phone but not another, or takes 30+ seconds to respond.

**Root cause:** Each phone has its own mDNS cache with TTL-based expiry. When the Border Router's IP changes (e.g., DHCP renewal) or the BR reboots, phones with stale caches send traffic to the wrong address.

**How it plays out:**
```
Phone A: cached BR at 192.168.1.50 (stale, BR moved to .51)  -> timeout
Phone B: fresh mDNS query -> finds BR at 192.168.1.51        -> works
```

**TTLs involved:**
- mDNS A/AAAA record: typically 120 seconds
- mDNS SRV record: typically 120 seconds
- mDNS TXT record: typically 4500 seconds

### 3. Sleepy End Device Polling Latency

**Symptom:** Device takes 2-10 seconds to respond to commands.

**Root cause:** Sleepy End Devices (SEDs) turn off their radio to save battery and only wake up periodically to poll their parent router for queued messages. The poll period is typically 2 seconds for a door sensor, up to 30 seconds for extremely battery-constrained devices.

```
Phone sends "Turn on light" at T=0
     |
     v
Border Router receives, forwards into Thread mesh
     |
     v
Parent router of the SED queues the frame
     |
     ... SED is asleep ...
     |
T=1.8s  SED wakes up, sends Data Poll to parent
     |
     v
Parent sends queued frame to SED
     |
     v
SED processes command, sends response
     |
     v
T=2.0s  Phone receives response
```

**How to simulate:** Configure Node 2 as `MTD_SED` mode with a poll period, inject latency in the broker matching the poll period.

### 4. Border Router Proxy Table Overflow

**Symptom:** New devices can't be commissioned, or recently added devices are unreachable.

**Root cause:** The Border Router maintains a proxy table mapping Wi-Fi-side connections to Thread-side sessions. This table has a limited size (typically 32-64 entries). In a large smart home with many devices and multiple phones/hubs, the table can fill up.

### 5. Thread Network Partition During Discovery

**Symptom:** Device commissioned successfully but immediately becomes unreachable.

**Root cause:** The Thread network partitioned during or just after commissioning. The device ended up in a different partition than the Border Router. The device has valid credentials and fabric info, but no route to the BR.

```
Pre-partition:  [BR] -- [Router A] -- [Device]    (all connected)

Post-partition: [BR] -- [Router A]    [Device]    (Device isolated)
                                       (has valid fabric, no route to BR)
                                       (SRP registration impossible)
                                       (Phone says "no response")
```

### 6. Multicast Discovery Failure

**Symptom:** Commissioning discovery doesn't find the device even though it's in commissioning mode.

**Root cause:** Thread uses specific multicast addresses for discovery. The `ff03::1` (mesh-local all-nodes) and `ff03::2` (mesh-local all-routers) multicast groups must be correctly joined. If 6LoWPAN multicast compression or the multicast forwarding table is misconfigured, discovery packets don't reach the device.

---

## What Our Simulation Covers

### Currently Implemented

| Component | Implementation | File |
|-----------|---------------|------|
| RLOC16 encoding/decoding | Full | `src/core/Types.h` |
| Mesh-Local EID computation | Full | `src/thread/AddressManager.cpp` |
| Link-Local address computation | Full | `src/thread/AddressManager.cpp` |
| Modified EUI-64 (RFC 4291) | Full | `src/thread/AddressManager.cpp` |
| Distance-vector routing | Full (simplified) | `src/thread/Routing.cpp` |
| MLE advertisements | Full (simplified) | `src/thread/MLE.cpp` |
| RLOC16 assignment by Leader | Full | `src/thread/Leader.cpp` |
| PASE commissioning flow | Full (stubbed crypto) | `src/matter/PASE.cpp` |
| CASE session establishment | Full (stubbed crypto) | `src/matter/CASE.cpp` |
| Subscription lifecycle | Full | `src/matter/SubscriptionManager.cpp` |

### Now Implemented

| Component | Status | File |
|-----------|--------|------|
| DNS-SD / mDNS simulation | **Done** | `src/net/Discovery.h/.cpp` |
| SRP registration + expiry | **Done** | `src/thread/SRP.h/.cpp` |
| Border Router proxy table | **Done** | `src/thread/BorderRouter.h/.cpp` |
| Self-healing (neighbor liveness, partitions) | **Done** | `src/net/SelfHealing.h/.cpp` |
| Backhaul (cellular) management | **Done** | `src/net/SelfHealing.h` (BackhaulState) |

### Not Yet Implemented

| Component | Priority | Needed For |
|-----------|----------|------------|
| Full IPv6 packet routing by EID/RLOC | Medium | Realistic address-based routing |
| 6LoWPAN compression | Low | Rarely causes Matter bugs |
| UDP/CoAP transport | Medium | Transport-level failure simulation |
| Sleepy End Device polling | Medium | Battery device latency testing |
| Multicast forwarding | Low | Discovery edge cases |
| Per-controller mDNS cache with TTL | Low | Multi-phone staleness testing |

### Implementation Details

**DNS-SD Discovery** (`src/net/Discovery.h/.cpp`):
- `ServiceRegistry` — simulated SRP server on the Border Router, stores DNS-SD records
- `ServiceRecord` — stores `_matterc._udp` (commissioning) and `_matter._tcp` (operational) service types
- `DiscoveryClient` — phone browses/resolves services, with callback for new discoveries
- TTL-based expiration of stale registrations

**SRP Client/Server** (`src/thread/SRP.h/.cpp`):
- `SRPServer` — runs on Node 0 (BR), manages leases with configurable TTL (default 2 hours)
- `SRPClient` — runs on each device, registers services and handles RLOC change re-registration
- `SRPLease` — state machine: Active → Expiring → Expired
- `forceExpireLease()` — fault injection to simulate premature lease expiry
- Event callbacks for lease lifecycle monitoring

**Border Router Proxy** (`src/thread/BorderRouter.h/.cpp`):
- `BorderRouterProxy` — maps controller sessions (phone/cloud) to mesh device addresses
- Configurable table size limit (default 32, simulates constrained BR)
- `resolveDevice()` — EID-to-RLOC resolution
- `updateDeviceRLOC()` — handles re-attach with new RLOC
- `refreshFromRouting()` — marks entries inactive when routes become unreachable
- Session idle timeout with `expireIdle()`
- Rejection counting for table overflow debugging
