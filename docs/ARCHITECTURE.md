# MatterThreads Architecture

## Overview

MatterThreads is a C++20 testing framework for debugging Matter over Thread issues in delivery van fleets. It simulates a 4-node Thread mesh network (Border Router, Relay Router, End Device, Phone) with deterministic fault injection, DNS-SD discovery simulation, SRP lease management, and self-healing mesh validation.

## Process Architecture

6 OS processes running on localhost:

```
                    +---------------------+
                    |   matterthreads      |  CLI controller
                    |   (user interface)   |
                    +----------+----------+
                               | control (Unix domain sockets)
           +----------+--------+--------+----------+
           v          v        v        v          v
    +----------+  +----------+  +-----------+  +-----------+
    | mt_node 0 |  | mt_node 1|  | mt_node 2 |  | mt_node 3 |
    | (Leader/  |  | (Router/ |  | (End      |  | (Phone/   |
    |  Border   |  |  Relay)  |  |  Device/  |  |  Control- |
    |  Router)  |  |          |  |  Sensor)  |  |  ler)     |
    +-----+-----+  +----+-----+  +-----+-----+  +-----+-----+
          +---------------+---------------+-----------+
                          v
             +-------------------------+
             |       mt_broker         |  Radio medium simulation
             |  - 4x4 Link matrix     |  + fault injection
             |  - Topology presets     |  + backhaul simulation
             +-------------------------+
```

### IPC Channels

**Radio Medium (Nodes <-> Broker): localhost TCP**
- Broker listens on `127.0.0.1:19000`
- Each node connects and registers with its NodeId
- TCP ensures broker has full control over simulated packet loss
- Wire format: 12-byte header (magic, src, dst, len, channel, flags) + payload

**Control Plane (Controller <-> all): Unix domain sockets**
- `/tmp/matterthreads/control_broker.sock`
- `/tmp/matterthreads/control_node_N.sock`
- Length-prefixed JSON messages

## Library Layers

```
mt_core  <--  mt_net  <--  mt_thread  <--  mt_matter  <--  mt_fault
                                                       <--  mt_metrics
                                                       <--  mt_hw (optional)
```

### Layer 0: Core (`src/core/`)
Shared primitives: Types (NodeId, RLOC16, FabricId), Clock (monotonic + simulated), Log (structured with severity + node tag), Random (seeded PRNG), ByteBuffer (zero-copy span), Result (error handling).

### Layer 1: Network (`src/net/`)
Socket (RAII TCP wrapper), Frame (simulated 802.15.4 MAC), Channel (per-link loss/latency/LQI model), Broker (central TCP server + topology presets), FaultInjector (packet drop/delay/reorder/corrupt/duplicate), Discovery (ServiceRegistry + DiscoveryClient for DNS-SD simulation), SelfHealing (neighbor liveness, partition detection, backhaul management).

### Layer 2: Thread (`src/thread/`)
ThreadNode (device lifecycle), MeshTopology (4x4 link matrix with van/phone presets), MLE (mesh link establishment), Routing (distance-vector), AddressManager (RLOC16/ML-EID), Leader election, ChildTable, BorderRouter (proxy table for controller↔mesh sessions), SRP (lease-based service registration client/server).

### Layer 3: Matter (`src/matter/`)
TLV (encoder/decoder), Session/PASE/CASE (stubbed crypto, real message flow), Fabric (NOC management), InteractionModel (Read/Write/Subscribe/Invoke), SubscriptionManager (lifecycle + liveness), Exchange (request-response + retransmission), DataModel (attribute store + device presets).

### Layer 4: Fault Injection (`src/fault/`)
FaultPlan (declarative JSON scenarios), Scheduler (time-based activation), Chaos (random fault engine).

### Layer 5: Metrics (`src/metrics/`)
Counter, Histogram (P50/P95/P99), Timeline (ordered event log), Collector, Reporter (text/JSON/ANSI dashboard).

### Layer 6: Hardware Bridge (`src/hw/`, optional)
OTBRClient (REST API), ChipToolDriver (chip-tool CLI wrapper), HardwareNode (INode adapter for real devices).

## Thread Network Simulation

### Node Roles
- **Leader**: Manages router ID assignment, network data distribution
- **Router**: Full Thread Device, forwards mesh traffic, maintains routing table
- **REED**: Router-Eligible End Device, can be promoted to Router
- **SED/MED**: Sleepy/Minimal End Device, attached to a parent router

### Routing
Simplified distance-vector routing. Each router periodically sends MLE advertisements containing its route table. Neighbors update their tables based on received advertisements. Route cost = hop count. Max cost = 16 (unreachable).

### Mesh Healing
When a router goes down:
1. Remaining routers detect missing MLE advertisements (timeout = 2x interval)
2. Route entries through downed router marked unreachable
3. Costs recomputed via alternative paths
4. Children of downed router re-attach to new parent

## Fault Injection

### Fault Types
| Type | Description |
|------|-------------|
| PacketDrop | Drop N% of packets on a link |
| LatencySpike | Add extra one-way latency |
| Reorder | Deliver packets out of order |
| Corrupt | Flip random bits in payload |
| Duplicate | Deliver same packet twice |
| LinkDown | Completely sever a link |
| LinkDegrade | Gradually worsen link quality |
| NodeCrash | Kill a node process (SIGKILL) |
| NodeFreeze | Pause a node (SIGSTOP) for duration |
| PartialPartition | Node can reach A but not B |

### Chaos Mode
Random fault injection for soak testing. Configurable: fault probability per second, duration range, allowed fault types.

## Metrics

| Metric | Type | Description |
|--------|------|-------------|
| rtt_ms | Histogram | Round-trip time for Matter interactions |
| packet_delivery_rate | Gauge | % packets delivered per link |
| packet_drop_count | Counter | Dropped packets per link |
| retransmit_count | Counter | Retransmissions per exchange |
| subscription_report_interval_ms | Histogram | Actual intervals between reports |
| subscription_drop_count | Counter | Dropped subscriptions |
| subscription_recovery_time_ms | Histogram | Time to re-establish |
| commissioning_success_rate | Gauge | % successful commissions |
| commissioning_duration_ms | Histogram | Time to complete |
| mesh_healing_time_ms | Histogram | Time to reconverge |
| message_ordering_violations | Counter | Out-of-order messages |
| duplicate_message_count | Counter | Duplicates received |
