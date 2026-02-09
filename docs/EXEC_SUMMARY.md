# MatterThreads — Executive Summary

## The Problem

Delivery van fleets need real-time visibility into cargo conditions (temperature, humidity, door state, occupancy) and the ability to remotely control equipment (locks, lights, climate). Today this is done with proprietary, vendor-locked sensor systems that don't interoperate, are expensive to maintain, and require custom integrations for every fleet management platform.

## The Approach

**MatterThreads** is a simulation and testing framework that validates using two open standards — **Matter** and **Thread** — to solve this problem before deploying to real hardware.

- **Matter** is the smart home interoperability standard (backed by Apple, Google, Amazon, Samsung). It defines a common language for devices — a "door lock" or "temperature sensor" works the same way regardless of manufacturer. We apply it to vehicles instead of homes.

- **Thread** is a low-power wireless mesh network protocol (based on IEEE 802.15.4). It creates a self-healing mesh inside the van — if one node goes down, the others reroute around it. No Wi-Fi required, battery-friendly, and works without internet.

The key insight: a delivery van has the same problem as a smart home — multiple sensors and actuators that need to talk to each other and report to a central system. Matter already defines device types for locks, temperature sensors, contact sensors, lighting, and occupancy. Thread already handles unreliable radio environments. We combine them in a vehicle context with cellular backhaul to the cloud.

## What We Built

### Simulation Framework (C++20, ~15K lines)

A multi-process simulator that runs a 4-node Thread mesh on localhost:

| Node | Role | Simulates |
|------|------|-----------|
| Node 0 | Leader / Border Router | Cab-mounted gateway — bridges Thread mesh to cellular |
| Node 1 | Router / Relay | Partition wall relay — extends mesh into cargo bay |
| Node 2 | End Device / Sensor | Cargo bay sensors (temp, humidity, door, occupancy) |
| Node 3 | Phone / Controller | Driver's phone — commissions devices, runs diagnostics |

Each node runs as a separate OS process. A central broker simulates the radio medium with configurable packet loss, latency, jitter, and link quality per link pair. This lets us test realistic failure scenarios:

- **Cellular dead zones** (tunnel, rural areas) — backhaul drops, mesh continues locally, events buffer for later delivery
- **Engine cranking** — voltage dip causes relay router brownout, mesh self-heals when power recovers
- **Sensor failure** — cargo sensor crashes, mesh detects loss, auto-recovers when sensor reboots
- **Network partitions** — link degradation between nodes, routing table recalculates

### Fleet Gateway Service

A cloud-side REST API that manages entire van fleets:

- **Van registry** — CRUD for van records, commissioning lifecycle, state tracking
- **Persistent sessions** — maintains secure connections to each van with keepalive and automatic reconnect (exponential backoff)
- **Fleet-wide subscriptions** — 8 default monitoring rules (cargo temp, door lock, humidity, occupancy, etc.) applied automatically to every van
- **Offline buffering** — per-van event ring buffer (10K events/van) for when vans are out of coverage, with sequence-based pagination for consumers
- **Multi-tenant isolation** — separate cryptographic fabrics per fleet operator, enforced at the API layer
- **24 REST endpoints** — van management, attribute reads, command invocation, subscription management, fleet status/alerts/telemetry, event streaming

### Hardware Bridge (Optional)

Adapter layer that connects the simulation to real Matter/Thread hardware:

- Wraps the Matter SDK's `chip-tool` CLI as a subprocess
- REST client for OpenThread Border Router API
- Enables gradual transition from simulated to real devices without changing application code

### Live Dashboard

Browser-based monitoring UI (http://localhost:8080) showing node status, mesh topology, metrics, and event timeline with 2-second auto-refresh. Includes a scripted healing demo that runs through fault injection scenarios automatically.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Fleet Gateway (REST API)                     │
│  VanRegistry · SessionPool · Subscriptions · OfflineBuffer      │
│  FabricManager · CommandRelay · 24 endpoints                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │ chip-tool CLI (sidecar)
┌───────────────────────────┴─────────────────────────────────────┐
│                     Hardware Bridge (optional)                   │
│  ChipToolDriver · OTBRClient · HardwareNode                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────────────┐
│                     Simulation Framework                         │
│  Matter:  TLV · PASE/CASE · Interaction Model · Subscriptions   │
│  Thread:  Mesh Routing · MLE · Border Router · SRP · Discovery  │
│  Network: Broker · Channels · Fault Injection · Self-Healing    │
│  Core:    Clock · Logging · Random · Result<T> Error Handling   │
└─────────────────────────────────────────────────────────────────┘
```

Six static libraries with clear dependency ordering. No circular dependencies. Optional components gated by CMake flags.

## Test Coverage

**206 automated tests** across three build tiers:

| Build | Tests | What's Covered |
|-------|-------|----------------|
| Default | 112 | Core simulation: TLV encoding, routing, channels, interaction model, fault injection, discovery, self-healing, border router, SRP, power management, dashboard |
| + Hardware Bridge | 144 | Above + process management, chip-tool output parsing, driver, OTBR client, hardware node adapter |
| + Fleet Gateway | 206 | Above + van registry, offline buffer, fabric manager, session pool, fleet subscriptions, command relay, gateway server, integration scenarios |

All tests run in under 5 seconds on a MacBook. No external dependencies at test time — everything is self-contained.

## Van Endpoint Design

Each instrumented van exposes 8 standard Matter endpoints:

| # | Device | Cluster | Use Case |
|---|--------|---------|----------|
| 0 | Root Node | Basic Info | Van ID, VIN, firmware version |
| 1 | Door Lock | 0x0101 | Remote lock/unlock cargo door |
| 2 | Temp Sensor | 0x0402 | Cold chain compliance (pharma, food) |
| 3 | Humidity | 0x0405 | Paired with temp for cold chain |
| 4 | Contact Sensor | 0x0045 | Door open/close detection |
| 5 | Lighting | 0x0006 | Cargo bay lights |
| 6 | Occupancy | 0x0406 | Security — presence in locked bay |
| 7 | Power Source | 0x002F | Vehicle battery monitoring |

Optional: reefer thermostat (endpoint 8), GPS (9), fuel/EV (10).

**Estimated hardware cost:** $57-100 per van (border router + relay + sensors).

## Key Technical Decisions

| Decision | Rationale |
|----------|-----------|
| **Matter over proprietary** | Vendor-neutral, certified interop, existing device type definitions for all our use cases, growing ecosystem |
| **Thread over Wi-Fi/BLE** | Self-healing mesh survives node failures, no infrastructure required, low power (312+ day battery budget), 802.15.4 radio handles vehicle EMI |
| **Simulation-first** | Validate mesh behavior, failure recovery, and fleet management before $100K+ hardware investment. Find protocol-level bugs at zero cost |
| **chip-tool sidecar** | Use official Matter SDK without embedding it (complex C++ build). Run chip-tool as subprocess, parse output. Swap for direct SDK integration later |
| **Multi-process sim** | Each node is an OS process — realistic isolation, no shared memory cheating, tests real IPC and failure modes |

## Current Status

| Phase | Status | Delivered |
|-------|--------|-----------|
| 1. Core simulation | Done | Thread mesh, Matter stack, fault injection, metrics |
| 2. Van scenario design | Done | Endpoint design, hardware BOM, commissioning flow, power analysis |
| 3. Phone + self-healing | Done | 4th node, DNS-SD discovery, partition recovery, SRP, power lifecycle, dashboard |
| 4. SDK integration | Done | chip-tool sidecar, OTBR client, hardware bridge adapter |
| 5. Fleet gateway | Done | REST API, session pool, subscriptions, offline buffer, multi-tenant |
| 6. Phone app | Not started | iOS/Android commissioning and diagnostics app |
| 7. Production hardening | Not started | Secure elements, OTA updates, EMI testing, certification |

## What This Enables

1. **Risk reduction** — Validate mesh reliability, failure recovery, and fleet management logic in software before committing to hardware
2. **Vendor independence** — Matter certification means any compliant sensor works. No lock-in to a proprietary fleet telemetry vendor
3. **Incremental deployment** — Start with simulation, bridge to real hardware one node at a time, scale to fleet
4. **Cold chain compliance** — Temperature/humidity monitoring with tamper-evident logging for FDA 21 CFR Part 11, FSMA, GDP
5. **Multi-tenant fleet management** — Single gateway serves multiple fleet operators with cryptographic isolation

## Next Steps

- **Phase 6:** Driver phone app (iOS/Android) for on-site commissioning and diagnostics
- **Phase 7:** Production hardening — secure elements, OTA firmware updates, vehicle power conditioning, EMI testing, Matter/Thread certification
- **Hardware pilot:** 3-5 van proof of concept using nRF5340 (border router) + nRF52840 (relay/sensors) — estimated 4-6 week timeline once hardware is available
