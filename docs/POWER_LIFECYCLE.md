# Power Lifecycle — High-Level Design

## Overview

Delivery vans have a 60-90 second battery window after engine-off before full power loss. The PowerManager simulates this lifecycle: ordered shutdown, hard cutoff, and cold boot recovery across all mesh devices.

## Power States

```
                    initiateShutdown()
  ┌──────────┐    (60-90s battery)     ┌──────────────┐
  │ EngineOn │ ───────────────────────▶│ ShuttingDown  │
  └──────────┘                         └──────┬───────┘
       ▲                                      │
       │                          battery expired │ hardCutoff()
       │                          or all nodes off│
       │                                      ▼
       │         initiateBoot()         ┌─────────┐
       │  ◀──────────────────────────── │   Off   │
       │                                └─────────┘
       │                                      │
       │         initiateBoot()               │
       │                                      ▼
       │                               ┌──────────┐
       └───────────────────────────────│ Booting  │
            all nodes on               └──────────┘
```

## Shutdown Sequence

Priority-ordered: **Sensors first, Relay second, Border Router last.**

The BR shuts down last because it needs to:
1. Relay final sensor readings upstream
2. Drain buffered messages over cellular
3. Deregister proxy table entries
4. Send final telemetry (GPS, cargo state)

```
T=0s    Engine OFF → ShuttingDown
        ┌─────────────────────────────────────────────────────────────┐
        │                   60-90s Battery Window                     │
        │                                                             │
        │  T+15s        T+30s         T+45s                          │
        │    │            │             │                              │
        │    ▼            ▼             ▼                              │
        │  ┌──────┐    ┌──────┐    ┌──────┐                          │
        │  │Sensor│    │Relay │    │  BR  │                          │
        │  │ OFF  │    │ OFF  │    │ OFF  │                          │
        │  └──────┘    └──────┘    └──────┘                          │
        │                                                             │
        │  Deregister   Invalidate  Drain proxy                      │
        │  SRP lease    routes      Flush buffer                      │
        │                           Power down                        │
        └─────────────────────────────────────────────────────────────┘
T=45-60s  SystemOff
```

### Per-Node Shutdown Callback

Each node's shutdown callback handles cleanup for that node type:

| Node | Priority | Callback Actions |
|------|----------|-----------------|
| Sensor (2) | 0 (first) | Deregister SRP lease, send final reading |
| Relay (1) | 1 (second) | Invalidate routes through this router |
| BR (0) | 2 (last) | Drain proxy table, flush buffers, emit PowerDown event |

## Hard Cutoff

If the battery dies before all nodes complete graceful shutdown:

```
T=0s    Engine OFF → ShuttingDown (expecting 90s)
T+15s   Sensor shuts down gracefully ✓
T+45s   Battery dies! → hardCutoff()
        Relay: HARD CUTOFF ✗ (was still On, routes not invalidated)
        BR:    HARD CUTOFF ✗ (proxy table not cleaned, buffers not drained)
T+45s   SystemOff (dirty state)
```

**Consequences of hard cutoff:**
- SRP leases still active on BR (will expire naturally after 2 hours)
- Proxy table entries orphaned (will expire after 5 minute idle timeout)
- Buffered messages lost
- Routes stale until MLE timeout (25s)

## Boot Sequence

Reverse priority order: **BR first, Relay second, Sensors last.**

The BR boots first because:
1. It's the network leader — other nodes can't attach without it
2. It starts the SRP server before sensors try to register
3. It establishes cellular backhaul for cloud connectivity

```
T=0s    Engine ON → Booting
T+3s    BR boots   → re-init SRP server, establish backhaul
T+6s    Relay boots → re-attach to BR, advertise routes
T+9s    Sensor boots → re-attach via relay, register SRP lease
T+9s    SystemOn (all operational)
```

### Recovery After Extended Parking

If the van is parked for hours:

```
Shutdown T=0          │  Parked (Off)  │  Boot T+3h
─────────────────────┼────────────────┼──────────────────
SRP leases active    │  Leases expire │  Re-register SRP
Proxy entries active │  (at 2hr mark) │  Re-create proxy
Routes valid         │                │  Rebuild routes
Subscriptions active │                │  Re-subscribe
                     │                │  Phone re-discovers
```

## Integration Map

```
┌──────────────┐
│ PowerManager │ ──── orchestrates shutdown/boot order
└──────┬───────┘
       │ callbacks
       ├──────────▶ RoutingTable.invalidateRouter()    (relay shutdown)
       ├──────────▶ SRPServer.removeLeasesForNode()    (sensor shutdown)
       ├──────────▶ BorderRouterProxy.expireIdle()     (BR shutdown)
       ├──────────▶ SelfHealingEngine.onSystemPowerDown/Up()
       │
       │ on boot:
       ├──────────▶ RoutingTable.addDirectNeighbor()   (relay boot)
       ├──────────▶ SRPServer.registerLease()          (sensor boot)
       └──────────▶ SelfHealingEngine.onSystemPowerUp()
```

PowerManager is **decoupled** — it doesn't include ThreadNode, SRP, or BorderRouter headers. Instead, callers wire up callbacks that call those modules' existing methods.

## Test Coverage

### Unit Tests (14 tests)

| Test | What It Validates |
|------|-------------------|
| InitialState | System starts EngineOn |
| RegisterNodes | Nodes registered with priorities |
| ShutdownTransition | EngineOn → ShuttingDown → Off |
| PriorityOrdering | Sensors off before relay, relay before BR |
| ShutdownCallbacks | Each node's callback invoked in order |
| HardCutoff | Force cutoff mid-shutdown |
| BootSequence | Off → Booting → EngineOn |
| BootCallbacksReverseOrder | BR boots first, sensor last |
| ShutdownRemaining | Countdown timer accuracy |
| EventHistory | All events recorded with timestamps |
| DoubleShutdownFails | Can't shut down twice |
| BootWhileOnFails | Can't boot when already on |
| BatteryExpiresAutoHardCutoff | Auto cutoff when battery timer runs out |
| SelfHealingPowerEvents | PowerDown/PowerUp events emitted |

### Scenario Tests (5 tests)

| Scenario | What It Validates |
|----------|-------------------|
| GracefulShutdownAndBoot | Full cycle: SRP deregister → route invalidate → proxy clear → boot → re-register → route rebuild |
| HardCutoffAt45s | Battery dies early, dirty state (orphaned proxy entries, stale SRP leases) |
| ColdBootAfterExtendedParking | 3hr parking, SRP leases naturally expire, boot re-registers, phone re-discovers |
| RaceConditionSensorVsRelay | Sensor sends final reading — succeeds because sensor shuts down before relay |
| PowerCycleSubscriptionRecovery | Active subscription dropped during shutdown, recovered after boot |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| Battery window | 60-90s | Passed to `initiateShutdown()` by caller |
| Boot delay per priority group | 3s | Time between each priority group booting |
| Shutdown spacing | `battery_life / (num_groups + 1)` | Time between each priority group shutting down |
