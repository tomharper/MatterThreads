# Fleet Gateway Service

The Fleet Gateway is a cloud-side Matter controller that manages all delivery vans in a fleet. It maintains persistent CASE sessions, aggregates subscriptions, relays commands, and buffers events for offline vans.

## Architecture

See [`diagrams/11-fleet-gateway-architecture.svg`](diagrams/11-fleet-gateway-architecture.svg) for the full visual.

```
                    +-------------------+
                    |   Fleet Gateway   |
                    |   (mt_gateway)    |
                    +--------+----------+
                             |
         +-------------------+-------------------+
         |                   |                   |
    +---------+        +---------+         +---------+
    |  VAN-1  |        |  VAN-2  |   ...   |  VAN-N  |
    | (CASE)  |        | (CASE)  |         | (CASE)  |
    +---------+        +---------+         +---------+
```

### Components

| Component | File | Purpose |
|-----------|------|---------|
| GatewayTypes | `GatewayTypes.h` | VanId, TenantId, VanState, VanRegistration, GatewayConfig |
| VanRegistry | `VanRegistry.h/.cpp` | CRUD for van records, per-tenant filtering, JSON serialization |
| CASESessionPool | `SessionPool.h/.cpp` | Persistent CASE sessions with keepalive and exponential backoff reconnect |
| FleetSubscriptionManager | `FleetSubscriptionManager.h/.cpp` | Fleet-wide subscription rules, per-van state, liveness tracking |
| CommandRelay | `CommandRelay.h/.cpp` | Routes REST commands to ChipToolDriver per van/endpoint |
| OfflineBuffer | `OfflineBuffer.h/.cpp` | Per-van ring buffer with monotonic sequence IDs and age-based eviction |
| FabricManager | `FabricManager.h/.cpp` | Per-tenant fabric isolation, simulated NOC issuance |
| GatewayServer | `GatewayServer.h/.cpp` | HTTP server with full REST routing (GET/POST/PUT/DELETE), path params, JSON body |
| GatewayMain | `GatewayMain.cpp` | Standalone executable entry point with signal handling and event loop |

## REST API

### Van Management

```
GET    /api/vans                   List all vans (filter by X-Tenant-Id header)
POST   /api/vans                   Register a van
GET    /api/vans/{van_id}          Get van details
PUT    /api/vans/{van_id}          Update van
DELETE /api/vans/{van_id}          Deregister van
```

### Commands

```
GET    /api/vans/{van_id}/endpoints/{ep}/clusters/{cl}/attributes/{attr}   Read attribute
POST   /api/vans/{van_id}/endpoints/{ep}/clusters/{cl}/commands/{cmd}      Invoke command
POST   /api/vans/{van_id}/lock       Lock cargo door (DoorLock cluster 0x0101, cmd 0x0000)
POST   /api/vans/{van_id}/unlock     Unlock cargo door (DoorLock cluster 0x0101, cmd 0x0001)
```

### Subscriptions

```
GET    /api/vans/{van_id}/subscriptions   List active subscriptions for a van
POST   /api/subscriptions                 Create subscription rule
DELETE /api/subscriptions/{sub_id}        Remove subscription rule
```

### Fleet Overview

```
GET    /api/fleet/status       Fleet summary (online/offline counts, active subscriptions)
GET    /api/fleet/alerts       Active alerts (unreachable vans)
GET    /api/fleet/telemetry    Telemetry snapshot (report counts, buffer sizes)
```

### Events

```
GET    /api/vans/{van_id}/events   Buffered events for a van (?since=seq)
GET    /api/events                 Fleet-wide events (?since=seq)
```

### Tenants

```
GET    /api/tenants                List tenants
POST   /api/tenants                Create tenant
GET    /api/tenants/{id}/fabric    Fabric info for tenant
POST   /api/tenants/{id}/fabric/noc   Issue NOC for a van
```

### Operational

```
GET    /api/health     Health check
GET    /api/metrics    Gateway metrics
```

## Default Subscription Rules

When `loadDefaultVanRules()` is called, 8 subscription rules are registered based on the delivery van endpoint design:

| Rule | Endpoint | Cluster | Attribute | Min/Max Interval | Critical |
|------|----------|---------|-----------|------------------|----------|
| cargo-temp | 2 | 0x0402 (Temperature) | 0x0000 (MeasuredValue) | 5s / 30s | Yes |
| cargo-humidity | 3 | 0x0405 (Humidity) | 0x0000 | 5s / 60s | No |
| door-contact | 4 | 0x0045 (BooleanState) | 0x0000 | 1s / 10s | Yes |
| door-lock-state | 1 | 0x0101 (DoorLock) | 0x0000 (LockState) | 1s / 10s | Yes |
| interior-light | 5 | 0x0006 (OnOff) | 0x0000 | 5s / 60s | No |
| occupancy | 6 | 0x0406 (Occupancy) | 0x0000 | 1s / 10s | Yes |
| battery-voltage | 7 | 0x002F (PowerSource) | 0x000B (BatVoltage) | 30s / 300s | No |
| reefer-setpoint | 8 | 0x0201 (Thermostat) | 0x0012 (OccupiedCoolingSetpoint) | 5s / 60s | Yes |

## CASE Session Pool

- `connect(van_id, device_id)` establishes a CASE session via ChipToolDriver
- Keepalive: reads BasicInformation:SoftwareVersion every 60 seconds
- On failure: exponential backoff reconnect (5s base, doubles per attempt, capped at 5 min, max 10 attempts)
- Session states: `Disconnected -> Connecting -> Connected -> Reconnecting -> Failed`
- Session event callbacks for state transitions

## Offline Buffer

- Per-van ring buffer: 10,000 events per van, 100,000 total across fleet
- Monotonic sequence IDs for cursor-based pagination
- `drain(van_id, since_seq)` — get events for a van since a sequence number
- `drainAll(since_seq)` — fleet-wide event stream
- `evict(max_age, now)` — clean up old events

## Multi-Tenant Support

- Each tenant gets its own fabric (unique FabricId, simulated root CA and IPK)
- NOC issuance per tenant/van pair
- `canAccess(tenant_id, van_id)` enforces tenant isolation
- `X-Tenant-Id` HTTP header for tenant-scoped API requests
- Per-tenant van limit (default 100)

## Build

```bash
# Requires ENABLE_HW_BRIDGE (which provides ChipToolDriver)
cmake -B build -DENABLE_HW_BRIDGE=ON -DENABLE_GATEWAY=ON
cmake --build build
ctest --test-dir build    # 206 tests

# Run the gateway server
./build/src/gateway/mt_gateway_server --port 8090 --chip-tool /path/to/chip-tool
```

## Tests

| Test File | Tests | Coverage |
|-----------|-------|----------|
| TestVanRegistry.cpp | 8 | CRUD, tenant filtering, state tracking |
| TestOfflineBuffer.cpp | 8 | Push/drain, capacity limits, eviction |
| TestFabricManager.cpp | 8 | Tenant CRUD, NOC issuance, isolation |
| TestSessionPool.cpp | 8 | Connect/disconnect, keepalive, reconnect |
| TestFleetSubscription.cpp | 8 | Rules, subscribe/unsubscribe, liveness |
| TestCommandRelay.cpp | 6 | Invoke/read/write on disconnected vans |
| TestGatewayServer.cpp | 10 | Route matching, HTTP round-trips, REST API |
| TestGatewayScenarios.cpp | 6 | Fleet lifecycle, multi-tenant, offline buffer |
| **Total** | **62** | |
