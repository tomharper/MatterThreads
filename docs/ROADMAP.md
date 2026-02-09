# MatterThreads — Project Roadmap

## Mission

Build a simulation and testing framework for Matter over Thread in delivery van fleets. Validate mesh reliability, phone controller integration, and cloud fleet management before deploying to real hardware.

---

## What's Built (Phases 1-3 Complete)

### Phase 1: Core Simulation Framework

**Status: DONE**

The foundation — a multi-process Thread mesh simulator with deterministic fault injection.

| Component | Files | What It Does |
|-----------|-------|-------------|
| Core primitives | `src/core/` (Types, Clock, Log, Random, ByteBuffer, Result) | NodeId, RLOC16, FabricId, monotonic clock, seeded PRNG, `Result<T>` error handling |
| Network layer | `src/net/` (Socket, Frame, Channel, Broker) | TCP IPC between nodes, simulated 802.15.4 MAC frames, per-link loss/latency/LQI model |
| Fault injection | `src/fault/` (FaultInjector, FaultPlan, Scheduler, Chaos) | PacketDrop, LatencySpike, Reorder, Corrupt, Duplicate, LinkDown, LinkDegrade, NodeCrash, NodeFreeze, PartialPartition |
| Thread mesh | `src/thread/` (ThreadNode, MeshTopology, MLE, Routing, AddressManager, Leader, ChildTable) | Device lifecycle, 3x3 link matrix, MLE advertisements, distance-vector routing, RLOC16 assignment, child management |
| Matter stack | `src/matter/` (TLV, Session, PASE, CASE, Fabric, InteractionModel, SubscriptionManager, Exchange, DataModel) | TLV encoding, PASE/CASE (stubbed crypto), Read/Write/Subscribe/Invoke, subscription liveness |
| Metrics | `src/metrics/` (Counter, Histogram, Timeline, Collector, Reporter) | P50/P95/P99 histograms, event timeline, text/JSON/ANSI dashboard |
| CLI controller | `app/Main.cpp` | Process orchestrator: spawns broker + nodes, interactive commands |
| Broker | `src/net/Broker.cpp` | Central TCP relay simulating the radio medium |
| Node process | `src/node/NodeMain.cpp` | Per-node process running Thread + Matter stacks |
| Hardware bridge | `src/hw/` (optional, `ENABLE_HW_BRIDGE=ON`) | OTBRClient REST API, ChipToolDriver CLI wrapper, HardwareNode adapter |

**Phase 1 tests:** 37 (6 TLV, 6 Routing, 5 Channel, 6 InteractionModel, 8 FaultInjector, 6 Scenarios)

**Topology presets:** `fullyConnected()`, `linearChain()`, `starFromLeader()`

---

### Phase 2: Delivery Van Scenario

**Status: DONE**

Extended the 3-node simulation to model an instrumented delivery van.

| Deliverable | Details |
|-------------|---------|
| Van endpoint design | 8 Matter endpoints: Root Node, Cargo Door Lock, Temp Sensor, Humidity, Door Contact, Interior Light, Occupancy, Power Source. Plus optional: Reefer, GPS, Fuel/EV. |
| Network architecture | In-van Thread mesh: Border Router (cab) → Relay Router (partition wall) → Cargo Bay sensors. Cellular backhaul (LTE-M/NB-IoT/5G) to cloud. |
| Hardware BOM | nRF5340+nRF21540 (BR), nRF52840 (relay), sensor-specific MCUs. Estimated $45-80/van for BR, $12-20 for relay. |
| Commissioning flow | Factory provisioning (assembly line) + field replacement (phone → cloud → BR → new sensor over Thread). |
| Power management | Engine ON / Ignition OFF / Deep Sleep state machine. Battery budget analysis (312+ days in low power). |
| Connectivity challenges | Cellular dead zones (ring buffer, 69hr offline capacity), van-to-van interference, EMI, cold chain compliance. |
| Fleet integration | Fleet Gateway (Matter controller in cloud), REST/gRPC API, periodic telemetry + event-driven alerts + remote commands. |
| Regulatory | Matter certification, FCC/CE radio, SAE J1455, ISO 16750, FDA 21 CFR Part 11, FSMA, GDP. |

**Docs:** `DELIVERY_VAN_ENDPOINT.md`, `DELIVERY_VAN_ARCH_DIAGRAM.md`

---

### Phase 3: Phone Controller + Self-Healing

**Status: DONE**

Added 4th node (phone), discovery, self-healing, Border Router proxy, SRP lease management, van-specific CLI commands, and 5 van integration scenarios.

| Component | Files | What It Does |
|-----------|-------|-------------|
| 4-node mesh | `Broker.h`, `MeshTopology.h` (MAX_NODES 3→4) | Node 0=Leader/BR, 1=Router/Relay, 2=EndDevice/Sensor, 3=Phone |
| Van topology | `MeshTopology::vanWithPhone()` | Linear chain (cab wall blocks 0↔2) + phone on cellular backhaul (120ms latency, 60ms jitter, 2% loss) |
| Topology wiring | `Broker.h` (`applyTopology`), `BrokerMain.cpp`, `Main.cpp` | `--topology` CLI flag passes preset to broker process, applies 4x4 link matrix |
| Discovery | `src/net/Discovery.h/.cpp` | ServiceRegistry (SRP server on BR), ServiceRecord (`_matterc._udp`, `_matter._tcp`), DiscoveryClient (phone browses/resolves), TTL expiry |
| Self-healing | `src/net/SelfHealing.h/.cpp` | Neighbor liveness (MLE timeout), partition detection, subscription recovery, backhaul state tracking, node re-attachment, event history |
| Border Router proxy | `src/thread/BorderRouter.h/.cpp` | Proxy table mapping controller↔mesh sessions (EID-to-RLOC resolution), configurable table size, idle expiry, RLOC updates, routing refresh |
| SRP Client/Server | `src/thread/SRP.h/.cpp` | SRPServer: lease management with TTL (2hr default), tick-based expiry, registry integration. SRPClient: auto-hostname from EUI-64, RLOC change re-registration, renewal tracking |
| Phone node | `src/node/NodeMain.cpp` | Phone role support, discovery scanning, healing callbacks, service registration |
| Van CLI commands | `app/Main.cpp` | `discover`, `backhaul down/up`, `backhaul latency <ms>`, `tunnel <sec>`, `crank`, `healing` |
| Van scenarios | `tests/scenarios/TestVanScenarios.cpp` | 5 integration tests: PhoneCommissionTunnel, BackhaulLossRecovery, UnlockDuringCrank, SRPLeaseExpiryDuringParking, ProxyTableOverflow |
| Power lifecycle | `src/thread/PowerManager.h/.cpp` | Power state machine (EngineOn→ShuttingDown→Off→Booting), priority-ordered shutdown (sensors→relay→BR), hard cutoff, cold boot recovery, 60-90s battery window simulation |
| Unit tests | `tests/unit/Test{Discovery,SelfHealing,BorderRouter,SRP,PowerManager}.cpp` | 58 unit tests covering network/thread modules |
| Power lifecycle scenarios | `tests/scenarios/TestPowerLifecycle.cpp` | 5 scenarios: GracefulShutdownAndBoot, HardCutoffAt45s, ColdBootAfterExtendedParking, RaceConditionSensorVsRelay, PowerCycleSubscriptionRecovery |
| Mobile SDK guide | `docs/MOBILE_SDK_GUIDE.md` | Evaluated Apple Matter Framework vs Google Home SDK vs connectedhomeip. Recommendation: connectedhomeip for fleet use. |
| Web dashboard | `src/metrics/DashboardServer.h/.cpp` | Non-blocking HTTP server on configurable port. Serves inline HTML dashboard + JSON API endpoints (`/api/status`, `/api/metrics`, `/api/timeline`, `/api/topology`). Auto-refresh every 2s. |
| Dashboard tests | `tests/unit/TestDashboardServer.cpp` | 7 tests: start/listen, serve HTML, serve JSON metrics/timeline/status/topology, 404 handling |
| Architecture diagrams | `docs/diagrams/` (13 SVGs) | Full system, in-van network, protocol stack, Thread topology, IPv6 addresses, temp report flow, door unlock flow, power state machine, shutdown/boot sequence, failure scenarios, simulation mapping |

**Updated topology presets:** `fullyConnected()`, `linearChain()`, `starFromLeader()`, `vanWithPhone()` — all handle 4 nodes (phone connects to BR via backhaul).

**Total tests after Phase 3: 112** (37 from Phase 1 + 10 Discovery + 12 SelfHealing + 10 BorderRouter + 12 SRP + 5 Van Scenarios + 14 PowerManager + 5 Power Lifecycle + 7 Dashboard)

---

## What's Built (Phases 4-5 Complete)

### Phase 4: connectedhomeip Integration

**Status: DONE**
**Approach:** Sidecar — chip-tool runs as a subprocess; ChipToolDriver wraps its CLI. connectedhomeip added as git submodule but NOT built by CMake (user builds chip-tool separately via connectedhomeip's own GN build).

| Component | Files | What It Does |
|-----------|-------|-------------|
| ProcessManager | `src/hw/ProcessManager.h/.cpp` | POSIX subprocess RAII wrapper (fork/exec/pipes, sync+async, timeout, SIGTERM/SIGKILL) |
| ChipToolOutputParser | `src/hw/ChipToolOutputParser.h/.cpp` | Regex-based parsing of chip-tool stdout/stderr into Result types (pairing, read, write, invoke, subscribe) |
| ChipToolDriver | `src/hw/ChipToolDriver.h/.cpp` | Core sidecar: builds CLI commands, manages cluster/attr/command name lookup, subscription polling |
| OTBRClient | `src/hw/OTBRClient.h/.cpp` | libcurl REST client for OpenThread Border Router API (state, dataset, neighbors, SRP, commissioner) |
| HardwareNode | `src/hw/HardwareNode.h/.cpp` | IHardwareNode interface + concrete impl delegating to Driver+OTBR |
| Darwin bridge | `src/hw/darwin/DarwinBridge.h/.mm` | Obj-C++ bridge to Matter.framework using MTRDeviceController (gated by `ENABLE_DARWIN_BRIDGE`) |
| Cert harness | `tests/scenarios/TestCertHarness.cpp` | 7 DISABLED_ tests covering CSA interop: PASE, CASE, Attestation, Read, Write, Invoke, Subscribe |
| CLI hw mode | `app/Main.cpp` | `--hw` mode REPL: commission, read, write, invoke, version, status commands |

**Build:** `cmake -B build -DENABLE_HW_BRIDGE=ON && cmake --build build` (requires libcurl)

**Tests:** 32 new hw unit tests (ProcessManager 6, ChipToolOutputParser 8, ChipToolDriver 6, OTBRClient 6, HardwareNode 6) + 7 disabled cert tests

**Total tests after Phase 4: 144** (112 existing + 32 new hw unit tests)

---

### Phase 5: Fleet Gateway Service

**Status: DONE**

Cloud-side Matter controller managing all delivery vans in the fleet. Gated by `ENABLE_GATEWAY=ON` (requires `ENABLE_HW_BRIDGE=ON`).

| Component | Files | What It Does |
|-----------|-------|-------------|
| GatewayTypes | `src/gateway/GatewayTypes.h` | VanId, TenantId, VanState enum, VanRegistration, GatewayConfig |
| VanRegistry | `src/gateway/VanRegistry.h/.cpp` | CRUD for van records, per-tenant filtering, JSON serialization |
| CASESessionPool | `src/gateway/SessionPool.h/.cpp` | Persistent CASE sessions, keepalive (60s), exponential backoff reconnect (5s->5min, max 10) |
| FleetSubscriptionManager | `src/gateway/FleetSubscriptionManager.h/.cpp` | Fleet-wide subscription rules (8 defaults), per-van state, 3-strike liveness, observer callbacks |
| CommandRelay | `src/gateway/CommandRelay.h/.cpp` | Routes REST commands to ChipToolDriver per van/endpoint, connection check |
| OfflineBuffer | `src/gateway/OfflineBuffer.h/.cpp` | Per-van ring buffer (10K/van, 100K total), sequence-based drain, age eviction |
| FabricManager | `src/gateway/FabricManager.h/.cpp` | Per-tenant fabric isolation, simulated NOC issuance, van limit enforcement |
| GatewayServer | `src/gateway/GatewayServer.h/.cpp` | HTTP server: GET/POST/PUT/DELETE, path params, JSON body, 24 REST endpoints |
| GatewayMain | `src/gateway/GatewayMain.cpp` | Standalone executable with signal handling and poll-based event loop |

**REST API:** 24 endpoints covering van CRUD, attribute read/write, command invoke, lock/unlock shortcuts, subscription rules, fleet status/alerts/telemetry, events, tenants/fabric/NOC, health, metrics.

**Build:** `cmake -B build -DENABLE_HW_BRIDGE=ON -DENABLE_GATEWAY=ON && cmake --build build`

**Tests:** 62 new (8 VanRegistry + 8 OfflineBuffer + 8 FabricManager + 8 SessionPool + 8 FleetSubscription + 6 CommandRelay + 10 GatewayServer + 6 Scenarios)

**Total tests after Phase 5: 206** (112 base + 32 hw bridge + 62 gateway)

**Docs:** `docs/FLEET_GATEWAY.md`

---

## What's Next (Phases 6-7)

### Phase 6: Phone Controller App

**Status: NOT STARTED**
**Priority: MEDIUM**
**Goal:** Driver phone app for on-site commissioning and diagnostics.

| Task | Description | Depends On |
|------|-------------|------------|
| iOS app skeleton | Swift app using Darwin Framework from connectedhomeip | Phase 4 (Darwin build) |
| Android app skeleton | Kotlin app using CHIPTool APIs | Phase 4 (Android build) |
| BLE commissioning flow | Scan QR code → PASE → Thread credential provisioning → fleet fabric NOC | Phase 5 (fabric mgmt) |
| Sensor diagnostics UI | Read all endpoints, show mesh topology, signal strength | Commissioning flow |
| Manual lock/unlock | Invoke DoorLock commands with timed invoke | Commissioning flow |
| Offline mode | Queue commands when cellular is down, execute when reconnected | — |
| Field replacement wizard | Guide technician through replacing a failed sensor | Commissioning flow |

---

### Phase 7: Production Hardening

**Status: NOT STARTED**
**Priority: LOW (until Phases 4-6 are validated)**
**Goal:** Get the system ready for real vans.

| Task | Description | Depends On |
|------|-------------|------------|
| Secure element integration | Store Thread network key + Matter DAC in ATECC608B / SE050 | Hardware available |
| OTA firmware update | Matter OTA cluster implementation for field updates | connectedhomeip OTA |
| Power management firmware | Engine ON / Ignition OFF / Deep Sleep state machine on nRF5340 | Hardware available |
| Vehicle power conditioning | 9-32V input, load dump protection, brownout detection | Hardware design |
| Cold chain compliance | Tamper-evident temperature logging, gap detection, vendor-specific cluster | Phase 5 (gateway) |
| Depot interference testing | Multi-van 802.15.4 channel coordination | Multiple hardware kits |
| EMI testing | Validate Thread comms during cranking, alternator noise | Vehicle + hardware |
| Certification | Matter cert, Thread cert, FCC/CE, SAE J1455, ISO 16750 | All above |

---

## Documentation Index

| Doc | Covers |
|-----|--------|
| `docs/ARCHITECTURE.md` | Process architecture, library layers, IPC, Thread simulation, fault injection, metrics |
| `docs/CLI.md` | CLI usage, options, interactive commands, predefined scenarios |
| `docs/DELIVERY_VAN_ENDPOINT.md` | Van endpoints, network architecture, hardware BOM, commissioning, power management, fleet integration, regulatory |
| `docs/DELIVERY_VAN_ARCH_DIAGRAM.md` | ASCII architecture diagrams: system, in-van, protocol stack, Thread topology, IPv6, data flows, power states, failure scenarios, simulation mapping |
| `docs/IPV6_AND_DISCOVERY.md` | Thread IPv6 addressing (link-local, ML-EID, RLOC), 6LoWPAN, DNS-SD discovery (commissioning + operational), SRP, common discovery bugs |
| `docs/MOBILE_SDK_GUIDE.md` | Apple vs Google vs connectedhomeip SDK comparison, recommendation for fleet, commissioning flow, code examples, build notes, certification |
| `docs/POWER_LIFECYCLE.md` | Power lifecycle HLD: state machine, shutdown/boot sequences, integration map, hard cutoff consequences, test coverage |
| `docs/FLEET_GATEWAY.md` | Fleet Gateway service: architecture, REST API, subscription rules, session pool, offline buffer, multi-tenant |
| `docs/ROADMAP.md` | This file — project plan and implementation status |
| `docs/diagrams/*.svg` | 14 SVG architecture diagrams (includes power state machine, shutdown/boot sequences, fleet gateway) |

---

## Build & Test

```bash
cmake -B build && cmake --build build                                          # Default (112 tests)
cmake -B build -DENABLE_HW_BRIDGE=ON && cmake --build build                    # HW bridge (144 tests)
cmake -B build -DENABLE_HW_BRIDGE=ON -DENABLE_GATEWAY=ON && cmake --build build  # Full (206 tests)
ctest --test-dir build                                                         # Run all tests
./build/matterthreads --topology van                                           # Van simulation
./build/matterthreads --hw --chip-tool /path/to/chip-tool                      # HW bridge mode
./build/src/gateway/mt_gateway_server --port 8090                              # Fleet gateway server
./build/matterthreads --help                                                   # All options
```

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++20 |
| Build | CMake 3.20+ |
| Compiler | Apple Clang 17 (macOS), GCC 12+ (Linux) |
| Dependencies | nlohmann/json (FetchContent), GoogleTest (FetchContent) |
| Matter SDK | connectedhomeip (Apache 2.0) — sidecar via chip-tool subprocess |
| Mobile — iOS | Darwin Framework (Obj-C/Swift) |
| Mobile — Android | CHIPTool APIs (Kotlin) |
| Fleet gateway | chip-tool or Python controller (Linux) |
| Hardware | nRF5340 (BR), nRF52840 (relay/sensors) |
| Cellular | Quectel BG95-M3 (LTE-M + NB-IoT + GNSS) |
