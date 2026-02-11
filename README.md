# MatterThreads

A simulation and testing framework for Matter over Thread mesh networks in delivery van fleets. Validates mesh reliability, self-healing, phone controller integration, and cloud fleet management — all in software, before deploying to real hardware.

## What It Does

MatterThreads runs a 4-node Thread mesh network as separate OS processes on your machine. A central broker simulates the radio medium between nodes, letting you inject faults (packet loss, latency, node crashes, link failures) and watch the mesh detect, adapt, and recover.

```
    matterthreads (CLI / Dashboard)
           |
    +----- | ------+-----------+-----------+
    |      |       |           |           |
  Node 0   |     Node 1     Node 2     Node 3
  Leader/  |     Router/    Sensor/    Phone/
  Border   |     Relay      EndDevice  Controller
  Router   |
           v
       mt_broker (radio medium simulator)
```

| Node | Role | What It Simulates |
|------|------|-------------------|
| 0 | Leader / Border Router | Cab-mounted gateway bridging Thread mesh to cellular |
| 1 | Router / Relay | Partition wall relay extending mesh into cargo bay |
| 2 | End Device / Sensor | Cargo sensors (temperature, humidity, door, occupancy) |
| 3 | Phone / Controller | Driver's phone for commissioning and diagnostics |

## Quick Start

```bash
# Build
cmake -B build && cmake --build build

# Run tests (112 pass)
ctest --test-dir build

# Launch interactive simulation with web dashboard
./build/app/matterthreads --topology van --dashboard 8080
# Open http://localhost:8080 in your browser

# Run the scripted healing demo (2 minutes, automated)
./scripts/healing-demo.sh
# Watch at http://localhost:8080
```

## Simulations

### Topology Presets

Choose how the 4 nodes are connected using `--topology`:

| Preset | Description | Use Case |
|--------|-------------|----------|
| `full` | All nodes directly connected, phone via backhaul | Baseline — best case mesh |
| `linear` | 0↔1↔2 chain (0↔2 blocked), phone via backhaul | Multi-hop routing, relay dependency |
| `star` | Node 0 is hub (1↔2 blocked), phone via backhaul | Hub-and-spoke, single point of failure |
| `van` | Linear chain + phone on cellular (120ms latency, 60ms jitter, 2% loss) | Realistic delivery van deployment |

```bash
./build/app/matterthreads --topology van        # Van with cellular backhaul
./build/app/matterthreads --topology linear      # Multi-hop chain
./build/app/matterthreads --topology star         # Hub-and-spoke
./build/app/matterthreads --topology full         # Fully connected (default)
```

### Interactive Commands

Once running, type commands at the `>` prompt:

**Observe the mesh:**
```
status              Show all nodes — PID, role, running/stopped
topology            Show link quality matrix between all nodes
metrics             Show packet counters, latencies, histograms
timeline            Show chronological event log
healing             Show self-healing event history
discover            Phone scans for devices via DNS-SD
```

**Break things:**
```
crash 2             Kill cargo sensor (SIGKILL)
crash 1             Kill relay router
link 0 1 50         Set 50% packet loss on leader→relay link
link 0 2 down       Drop leader↔sensor link entirely
backhaul down       Cut cellular (phone loses cloud connection)
crank               Engine cranking voltage dip (relay browns out and reboots)
chaos on            Random fault injection
```

**Fix things:**
```
restart 2           Restart a crashed node
link 0 1 up         Restore a dropped link
link 0 1 0          Set loss back to 0%
backhaul up         Restore cellular backhaul
chaos off           Stop random faults
```

**Van-specific:**
```
tunnel 10           Simulate 10-second tunnel (backhaul drops, auto-restores)
backhaul latency 200  Set cellular latency to 200ms
```

### Predefined Scenarios

Run automated scenarios with `--scenario <name>`. These run to completion and output results.

**Core mesh scenarios:**

| Scenario | What It Tests |
|----------|---------------|
| `mesh-healing` | Kill the middle node in a linear chain. Measure how long the mesh takes to detect the partition and reroute. |
| `subscription-recovery` | Establish a Matter subscription, drop the link for 90 seconds, verify the subscription recovers when the link comes back. |
| `commissioning-loss` | Commission a device under increasing packet loss (10%, 20%, 30%, 50%). Measure success rate and retry counts. |
| `route-failure` | Send 100 commands over a direct link, then degrade it to 80% loss. Verify traffic reroutes through alternate paths. |
| `message-ordering` | Rapid-fire commands with simulated jitter and packet duplication. Verify the Interaction Model handles reordering correctly. |

**Van-specific scenarios:**

| Scenario | What It Tests |
|----------|---------------|
| `phone-commission-tunnel` | Phone commissions a sensor, subscribes to temperature, then enters a tunnel. Verifies reports buffer on the border router and deliver when backhaul returns. |
| `backhaul-loss-recovery` | Phone has active subscriptions when cellular drops. Verifies the mesh continues locally and subscriptions recover after reconnection. |
| `unlock-during-crank` | Driver sends a door unlock command exactly when the engine cranks. Relay router browns out mid-command. Verifies the command retries and succeeds after the relay reboots. |
| `srp-lease-expiry-parking` | Van parked overnight — SRP leases expire after 2 hours. On morning startup, verifies all services re-register with the border router. |
| `proxy-table-overflow` | Multiple phone controllers connect through the border router simultaneously. Verifies graceful handling when the proxy table fills up (older idle sessions evicted). |

**Power lifecycle scenarios:**

| Scenario | What It Tests |
|----------|---------------|
| `GracefulShutdownAndBoot` | Engine off → orderly shutdown (sensors first, relay, then border router) → cold boot in reverse order. |
| `HardCutoffAt45s` | Battery dies at 45 seconds into shutdown — nodes that haven't finished graceful shutdown get hard-killed. |
| `ColdBootAfterExtendedParking` | Van sat for days. Cold boot with stale routing tables, expired SRP leases, and dead subscriptions. Everything must re-establish. |
| `RaceConditionSensorVsRelay` | Sensor tries to send a report while the relay is mid-reboot. Tests the window where the mesh is partially up. |
| `PowerCycleSubscriptionRecovery` | Full power cycle — verify all Matter subscriptions re-establish after boot without manual intervention. |

**Fleet gateway scenarios** (requires `ENABLE_GATEWAY=ON` build):

| Scenario | What It Tests |
|----------|---------------|
| `FleetCommissioningLifecycle` | Create tenant, register 3 vans, issue NOC credentials, verify all vans start as Registered. |
| `SubscriptionRulesFleetWide` | Load 8 default monitoring rules, verify critical flags, add custom GPS rule, confirm disconnected van can't subscribe. |
| `OfflineBufferAccumulationAndDrain` | Push events from 3 offline vans, drain per-van and fleet-wide with sequence cursors, test age-based eviction. |
| `MultiTenantIsolation` | Two tenants (Acme, Globex) each with vans. Verify cryptographic fabric isolation — Acme can't access Globex vans. |
| `CommandRelayDisconnectedFleet` | Send lock and read commands to 5 disconnected vans. All fail gracefully with error messages, no crashes. |
| `VanStateTransitionsWithEvents` | Walk a van through Registered → Commissioning → Online → Offline → Unreachable, buffering events at each stage. |

```bash
# Run a specific scenario
./build/app/matterthreads --scenario mesh-healing --seed 42

# Run with JSON output
./build/app/matterthreads --scenario unlock-during-crank --output results.json

# Run with verbose logging
./build/app/matterthreads --scenario backhaul-loss-recovery --verbose
```

### Scripted Healing Demo

An automated 2-minute demo that runs through 8 fault/recovery acts with a live dashboard:

```bash
./scripts/healing-demo.sh
# Open http://localhost:8080 to watch
```

| Time | Act | What Happens |
|------|-----|-------------|
| 0:00 | Healthy mesh | All 4 nodes running, phone discovers services via DNS-SD |
| 0:12 | Sensor crash | Cargo sensor (Node 2) killed — mesh detects the loss |
| 0:28 | Sensor recovery | Node 2 restarted — reattaches to mesh, subscriptions recover |
| 0:44 | Engine crank | Voltage dip kills relay router (Node 1) — auto-reboots after 400ms |
| 1:00 | Backhaul loss | Cellular goes down — phone isolated, mesh continues locally |
| 1:16 | Backhaul restored | Phone reconnects, re-discovers services |
| 1:32 | Chaos | Relay crash AND backhaul down simultaneously — worst case |
| 1:50 | Full recovery | Everything restored — all nodes healthy, mesh converged |

### Web Dashboard

Add `--dashboard <port>` to any simulation to get a live browser UI:

```bash
./build/app/matterthreads --topology van --dashboard 8080
```

Open http://localhost:8080 to see:
- Node status cards (role, PID, running/stopped)
- Mesh topology link matrix
- Metrics (packet counts, latencies, P50/P95/P99)
- Event timeline

The dashboard auto-refreshes every 2 seconds. JSON API endpoints are also available:
- `/api/status` — node states
- `/api/metrics` — counters and histograms
- `/api/topology` — link quality matrix
- `/api/timeline` — event log

### Fleet Gateway Server

A standalone REST API server for fleet-wide van management (requires `ENABLE_GATEWAY=ON` build):

```bash
# Build with gateway support
cmake -B build -DENABLE_HW_BRIDGE=ON -DENABLE_GATEWAY=ON
cmake --build build

# Run the gateway server
./build/src/gateway/mt_gateway_server --port 8090
```

24 REST endpoints covering:
- Van registration and lifecycle management
- Remote attribute reads and command invocation (lock/unlock doors, read temperature)
- Fleet-wide subscription rules (8 defaults for cargo monitoring)
- Offline event buffering and replay
- Multi-tenant fabric isolation
- Fleet status, alerts, and telemetry

See [docs/FLEET_GATEWAY.md](docs/FLEET_GATEWAY.md) for the full API reference.

## Build Configurations

| Build | Command | Tests |
|-------|---------|-------|
| Default | `cmake -B build && cmake --build build` | 112 |
| + Hardware Bridge | `cmake -B build -DENABLE_HW_BRIDGE=ON && cmake --build build` | 144 |
| + Fleet Gateway | `cmake -B build -DENABLE_HW_BRIDGE=ON -DENABLE_GATEWAY=ON && cmake --build build` | 206 |

```bash
# Run all tests
ctest --test-dir build

# Run with output on failure
ctest --test-dir build --output-on-failure
```

Requirements: C++20, CMake 3.20+, Apple Clang 17 (macOS) or GCC 12+ (Linux). Dependencies (nlohmann/json, GoogleTest) are fetched automatically via CMake FetchContent.

## Documentation

| Doc | What It Covers |
|-----|----------------|
| [EXEC_SUMMARY.md](docs/EXEC_SUMMARY.md) | Executive summary for technical leadership |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Process architecture, library layers, IPC, simulation design |
| [CLI.md](docs/CLI.md) | Full CLI reference — all options, commands, and scenarios |
| [DELIVERY_VAN_ENDPOINT.md](docs/DELIVERY_VAN_ENDPOINT.md) | Van endpoint design, hardware BOM, commissioning, power management |
| [FLEET_GATEWAY.md](docs/FLEET_GATEWAY.md) | Fleet gateway REST API, subscriptions, sessions, offline buffer |
| [POWER_LIFECYCLE.md](docs/POWER_LIFECYCLE.md) | Power state machine, shutdown/boot sequences, battery budget |
| [IPV6_AND_DISCOVERY.md](docs/IPV6_AND_DISCOVERY.md) | Thread IPv6 addressing, DNS-SD discovery, SRP |
| [MOBILE_SDK_GUIDE.md](docs/MOBILE_SDK_GUIDE.md) | Matter SDK comparison (Apple vs Google vs connectedhomeip) |
| [ROADMAP.md](docs/ROADMAP.md) | Project phases, status, and what's next |
| [diagrams/](docs/diagrams/) | 14 SVG architecture diagrams |
