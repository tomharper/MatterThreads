# Delivery Van Matter Endpoint — Architecture Diagrams

## Full System Architecture

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║                              CLOUD / FLEET PLATFORM                             ║
║                                                                                  ║
║  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────────┐   ║
║  │  Fleet Dashboard  │    │  Route Optimizer  │    │  Compliance Engine       │   ║
║  │  (dispatch UI)    │    │  (delivery plan)  │    │  (cold chain, audit)     │   ║
║  └────────┬─────────┘    └────────┬─────────┘    └────────────┬─────────────┘   ║
║           └───────────────────────┼───────────────────────────┘                  ║
║                                   │ REST / gRPC                                  ║
║                          ┌────────┴────────┐                                    ║
║                          │  Fleet Gateway   │                                    ║
║                          │  (Matter Ctrlr)  │                                    ║
║                          │                  │                                    ║
║                          │  - CASE sessions │                                    ║
║                          │  - Subscriptions │                                    ║
║                          │  - Command relay │                                    ║
║                          └────────┬────────┘                                    ║
╚═══════════════════════════════════╪══════════════════════════════════════════════╝
                                    │
                    Matter over TCP/IPv6 over cellular
                                    │
          ┌─────────────────────────┼─────────────────────────┐
          │                         │                         │
          ▼                         ▼                         ▼
   ┌─────────────┐          ┌─────────────┐          ┌─────────────┐
   │   VAN #1    │          │   VAN #2    │          │   VAN #N    │
   └─────────────┘          └─────────────┘          └─────────────┘
```

## In-Van Network Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            DELIVERY VAN                                     │
│                                                                             │
│  ┌──── CAB ────────────────────┐  ┌──── CARGO BAY ──────────────────────┐  │
│  │                              │  │                                     │  │
│  │  ┌────────────────────────┐  │  │                                     │  │
│  │  │    BORDER ROUTER       │  │  │     ┌───────────┐  ┌───────────┐   │  │
│  │  │    (Node 0 / Leader)   │  │  │     │  Temp     │  │ Humidity  │   │  │
│  │  │                        │  │  │     │  Sensor   │  │ Sensor    │   │  │
│  │  │  ┌──────┐ ┌────────┐  │  │  │     │  EP:2     │  │ EP:3      │   │  │
│  │  │  │nRF   │ │Quectel │  │  │  │     │  (MED)    │  │ (MED)     │   │  │
│  │  │  │5340  │ │BG95-M3 │  │  │  │     └─────┬─────┘  └─────┬─────┘   │  │
│  │  │  │      │ │        │──┼──┼──┼── LTE-M ──────────────── CLOUD      │  │
│  │  │  │802.15│ │LTE-M   │  │  │  │           │              │          │  │
│  │  │  │.4    │ │NB-IoT  │  │  │  │     ┌─────┴──────────────┴─────┐    │  │
│  │  │  │radio │ │GNSS    │  │  │  │     │                          │    │  │
│  │  │  └──┬───┘ └────────┘  │  │  │     │     RELAY ROUTER        │    │  │
│  │  │     │  ┌──────────┐   │  │  │     │     (Node 1 / Router)   │    │  │
│  │  │     │  │Secure    │   │  │  │     │                          │    │  │
│  │  │     │  │Element   │   │  │  │     │     ┌──────┐             │    │  │
│  │  │     │  │(keys)    │   │  │  │     │     │nRF   │             │    │  │
│  │  │     │  └──────────┘   │  │  │     │     │52840 │             │    │  │
│  │  └─────┼─────────────────┘  │  │     │     │      │             │    │  │
│  │        │ 802.15.4 mesh      │  │     │     │802.15│             │    │  │
│  │        │                    │  │     │     │.4    │             │    │  │
│  └────────┼────────────────────┘  │     │     └──┬───┘             │    │  │
│           │                       │     │        │                 │    │  │
│           │     Thread mesh       │     └────────┼─────────────────┘    │  │
│           │     (802.15.4)        │              │                      │  │
│           └───────────┬───────────┼──────────────┘                      │  │
│                       │           │              │                      │  │
│                       │           │     ┌────────┴────────┐             │  │
│                       │           │     │                 │             │  │
│                       │           │  ┌──┴────┐  ┌────────┴──┐          │  │
│                       │           │  │ Door  │  │ Door      │          │  │
│                       │           │  │ Lock  │  │ Contact   │          │  │
│                       │           │  │ EP:1  │  │ EP:4      │          │  │
│                       │           │  │ (MED) │  │ (SED)     │          │  │
│                       │           │  └───────┘  └───────────┘          │  │
│                       │           │                                     │  │
│                       │           │  ┌───────┐  ┌───────────┐          │  │
│                       │           │  │ Light │  │ Occupancy │          │  │
│                       │           │  │ EP:5  │  │ EP:6      │          │  │
│                       │           │  │ (MED) │  │ (SED)     │          │  │
│                       │           │  └───────┘  └───────────┘          │  │
│                       │           │                                     │  │
│                       │           └─────────────────────────────────────┘  │
│                       │                                                    │
│                  12V Vehicle Power Bus                                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Protocol Stack (Per Layer)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATION LAYER                            │
│                                                                     │
│  Fleet Mgmt ◄──REST──► Fleet Gateway ◄──Matter IM──► Van Endpoints │
│                                                                     │
│  Commands:  Lock/Unlock, Light On/Off, Read Temp, Set Thermostat   │
│  Reports:   Subscription reports (temp, door state, lock state)     │
│  Events:    Door opened in transit, temp out of range, low battery  │
├─────────────────────────────────────────────────────────────────────┤
│                        MATTER PROTOCOL                              │
│                                                                     │
│  ┌───────────┐  ┌──────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │Interaction│  │ Session  │  │   Fabric     │  │  Discovery   │  │
│  │  Model    │  │ (CASE)   │  │   (NOC +     │  │  (DNS-SD /   │  │
│  │           │  │          │  │    ACL)      │  │   SRP)       │  │
│  │ Read      │  │ Sigma1   │  │              │  │              │  │
│  │ Write     │  │ Sigma2   │  │ Fleet CA     │  │ BR advertises│  │
│  │ Subscribe │  │ Sigma3   │  │ signs NOCs   │  │ via mDNS on  │  │
│  │ Invoke    │  │          │  │ for all vans │  │ cellular PDN │  │
│  └───────────┘  └──────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│                        TRANSPORT                                    │
│                                                                     │
│  ┌─────────────────────────────┐  ┌─────────────────────────────┐  │
│  │    Cloud ◄─► Border Router  │  │  Border Router ◄─► Sensors  │  │
│  │                             │  │                             │  │
│  │    UDP (or TCP) / IPv6      │  │    UDP / IPv6               │  │
│  │    over cellular PDN        │  │    over 6LoWPAN             │  │
│  │                             │  │    over 802.15.4            │  │
│  │    ┌─────────────────┐      │  │    ┌─────────────────┐      │  │
│  │    │  LTE-M / NB-IoT │      │  │    │  Thread mesh    │      │  │
│  │    │  / 5G radio     │      │  │    │  (802.15.4)     │      │  │
│  │    └─────────────────┘      │  │    └─────────────────┘      │  │
│  └─────────────────────────────┘  └─────────────────────────────┘  │
│              BACKHAUL                        MESH                    │
└─────────────────────────────────────────────────────────────────────┘
```

## Thread Mesh Topology (Logical View)

```
                    ┌──────────────────────┐
                    │    BORDER ROUTER      │
                    │    Node 0 (Leader)    │
                    │                      │
                    │  RLOC16: 0x0000      │
                    │  Router ID: 0        │
                    │  EID: fd00::1000     │
                    │                      │
                    │  Roles:              │
                    │   - Thread Leader    │
                    │   - Border Router    │
                    │   - SRP Server       │
                    │   - Matter Controller│
                    └──────────┬───────────┘
                               │
                    802.15.4   │   LQI: 200
                    (direct)   │   RSSI: -45 dBm
                               │
                    ┌──────────┴───────────┐
                    │    RELAY ROUTER       │
                    │    Node 1 (Router)    │
                    │                      │
                    │  RLOC16: 0x0400      │
                    │  Router ID: 1        │
                    │  EID: fd00::1001     │
                    │                      │
                    │  Role:               │
                    │   - Thread Router    │
                    │   - Extends range    │
                    │     to cargo bay     │
                    └──┬─────────────┬─────┘
                       │             │
           ┌───────────┘             └───────────┐
           │ 802.15.4                 802.15.4   │
           │ LQI: 180                 LQI: 190   │
           │ RSSI: -55 dBm           RSSI: -50  │
           │                                     │
  ┌────────┴─────────────┐          ┌────────────┴────────────┐
  │  CARGO SENSORS (MED)  │          │  CARGO SENSORS (SED)    │
  │                       │          │                         │
  │  Temp     EP:2        │          │  Door     EP:4          │
  │  0x0402   RLOC:0x0401 │          │  Contact  RLOC:0x0403   │
  │                       │          │  0x0045                 │
  │  Humidity EP:3        │          │  Poll: 2s               │
  │  0x0405   RLOC:0x0402 │          │                         │
  │                       │          │  Occupancy EP:6         │
  │  Light    EP:5        │          │  0x0406   RLOC:0x0404   │
  │  0x0006   RLOC:0x0405 │          │  Poll: 5s               │
  │                       │          │                         │
  │  Lock     EP:1        │          └─────────────────────────┘
  │  0x0101   RLOC:0x0406 │
  │                       │
  │  Power    EP:7        │
  │  0x002F   RLOC:0x0407 │
  └───────────────────────┘

  MED = Minimal End Device (radio always on, vehicle-powered)
  SED = Sleepy End Device (radio off between polls, battery-powered)
```

## IPv6 Address Assignment

```
Thread Network: fd00::/64  (Mesh-Local Prefix)

┌──────────────┬───────────────────────────┬────────────────────────────┐
│    Device    │     Mesh-Local EID        │     RLOC Address           │
│              │     (stable)              │     (topology-dependent)   │
├──────────────┼───────────────────────────┼────────────────────────────┤
│ Border Rtr   │ fd00::0200:0000:0000:1000 │ fd00::ff:fe00:0000         │
│ Relay Rtr    │ fd00::0200:0000:0000:1001 │ fd00::ff:fe00:0400         │
│ Temp Sensor  │ fd00::0200:0000:0000:1002 │ fd00::ff:fe00:0401         │
│ Humidity     │ fd00::0200:0000:0000:1003 │ fd00::ff:fe00:0402         │
│ Door Contact │ fd00::0200:0000:0000:1004 │ fd00::ff:fe00:0403         │
│ Door Lock    │ fd00::0200:0000:0000:1005 │ fd00::ff:fe00:0406         │
│ Light        │ fd00::0200:0000:0000:1006 │ fd00::ff:fe00:0405         │
│ Occupancy    │ fd00::0200:0000:0000:1007 │ fd00::ff:fe00:0404         │
│ Power Source │ fd00::0200:0000:0000:1008 │ fd00::ff:fe00:0407         │
└──────────────┴───────────────────────────┴────────────────────────────┘

RLOC16 breakdown:
  0x0000 = Router 0, child 0 (router itself)
  0x0400 = Router 1, child 0 (router itself)
  0x0401 = Router 1, child 1 (temp sensor, attached to relay)
  0x0402 = Router 1, child 2 (humidity, attached to relay)
  ...
```

## Data Flow: Temperature Report

```
 Temp Sensor (EP:2)          Relay Router           Border Router          Cloud
      │                          │                       │                   │
  1.  │  Read SHT40 sensor       │                       │                   │
      │  temp = 4.2°C            │                       │                   │
      │                          │                       │                   │
  2.  │──802.15.4 data frame──►  │                       │                   │
      │  src: RLOC 0x0401        │                       │                   │
      │  dst: RLOC 0x0000        │                       │                   │
      │  payload: Matter IM      │                       │                   │
      │    ReportData {          │                       │                   │
      │      sub_id: 1           │                       │                   │
      │      attr: 2/0x0402/0    │                       │                   │
      │      value: 420 (0.01°C) │                       │                   │
      │    }                     │                       │                   │
      │                          │                       │                   │
  3.  │                          │──802.15.4 forward──►  │                   │
      │                          │  (relayed, same       │                   │
      │                          │   payload, new        │                   │
      │                          │   MAC src/dst)        │                   │
      │                          │                       │                   │
  4.  │                          │                       │  Process Matter   │
      │                          │                       │  subscription     │
      │                          │                       │  report           │
      │                          │                       │                   │
  5.  │                          │                       │──LTE-M upload──►  │
      │                          │                       │  Matter over TCP  │
      │                          │                       │  (or buffered if  │
      │                          │                       │   no signal)      │
      │                          │                       │                   │
  6.  │                          │                       │                   │ Store in
      │                          │                       │                   │ time-series
      │                          │                       │                   │ DB
      │                          │                       │                   │
  7.  │                          │                       │                   │ Check:
      │                          │                       │                   │ 4.2°C in
      │                          │                       │                   │ range
      │                          │                       │                   │ [2°C, 8°C]?
      │                          │                       │                   │ ✓ OK
```

## Data Flow: Remote Door Unlock

```
 Dispatch App           Cloud Gateway          Border Router         Door Lock (EP:1)
      │                      │                       │                      │
  1.  │──"Unlock van"──►     │                       │                      │
      │  (REST API call)     │                       │                      │
      │                      │                       │                      │
  2.  │                      │  Build Matter         │                      │
      │                      │  InvokeRequest:       │                      │
      │                      │    EP:1               │                      │
      │                      │    Cluster:0x0101     │                      │
      │                      │    Cmd: UnlockDoor    │                      │
      │                      │    Timed: 30s         │                      │
      │                      │                       │                      │
  3.  │                      │──Matter over TCP──►   │                      │
      │                      │  (cellular)           │                      │
      │                      │                       │                      │
  4.  │                      │                       │──802.15.4 mesh──►    │
      │                      │                       │  via relay router    │
      │                      │                       │  InvokeRequest       │
      │                      │                       │                      │
  5.  │                      │                       │                      │ Actuate
      │                      │                       │                      │ motor
      │                      │                       │                      │ driver
      │                      │                       │                      │ UNLOCKED
      │                      │                       │                      │
  6.  │                      │                       │◄──InvokeResponse──   │
      │                      │                       │   status: SUCCESS    │
      │                      │                       │                      │
  7.  │                      │◄──Matter over TCP──   │                      │
      │                      │   InvokeResponse      │                      │
      │                      │                       │                      │
  8.  │◄──"Door unlocked"──  │                       │                      │
      │   (REST response)    │                       │                      │
      │                      │                       │                      │
  9.  │                      │                       │                      │ T+30s:
      │                      │                       │                      │ Auto-relock
      │                      │                       │                      │ (timed
      │                      │                       │                      │  invoke
      │                      │                       │                      │  expired)
      │                      │                       │                      │
 10.  │                      │                       │◄──SubscriptionRpt──  │
      │                      │                       │  LockState: LOCKED   │
      │                      │◄──────────────────────│                      │
      │◄─────────────────────│                       │                      │
      │  "Door re-locked"    │                       │                      │
```

## Power State Machine

```
                         ┌─────────────────────┐
                         │                     │
              Engine     │    ENGINE ON         │
              start      │                     │
         ┌──────────────►│  BR: Full power     │
         │               │  Relay: Full power  │
         │               │  Sensors: MED/SED   │
         │               │  Cellular: Connected│
         │               │  Thread: All active │
         │               │                     │
         │               └──────────┬──────────┘
         │                          │
         │               Ignition off (ACC off)
         │                          │
         │                          ▼
         │               ┌─────────────────────┐
         │               │                     │
         │               │    IGNITION OFF      │
         │               │                     │
         │               │  BR: Low power      │
         │               │    (wake every 30s) │
         │               │  Relay: Low power   │
         │               │  Sensors: SED mode  │
         │               │  Cellular: Periodic │
         │               │    (wake every 15m) │
         │               │  Thread: Reduced    │
         │               │    advertisements   │
         │               │                     │
         │               └──────────┬──────────┘
         │                          │
         │               Parked > 24 hours
         │                          │
         │                          ▼
         │               ┌─────────────────────┐
         │               │                     │
         │               │    DEEP SLEEP        │
         │               │                     │
         │               │  BR: Ultra-low      │
         │               │    (50μA, wake on   │
         │               │     CAN or timer)   │
         │               │  Relay: Off         │
         │               │  Sensors: Off or    │
         │               │    battery-only     │
         │               │  Cellular: Off      │
         │               │  Thread: Off        │
         │               │                     │
         │               │  Wake triggers:     │
         │               │   - CAN bus activity│
         │               │   - Door open event │
         │               │   - 6-hour timer    │
         │               │                     │
         └───────────────┴─────────────────────┘

Power consumption:
  ENGINE ON:      ~600mA peak (cellular TX), ~80mA average
  IGNITION OFF:   ~8mA average (all devices combined)
  DEEP SLEEP:     ~110μA (BR + battery sensors only)
```

## Failure Scenarios and Recovery

```
SCENARIO 1: Relay router loses power (engine cranking dip)
═══════════════════════════════════════════════════════════

  Timeline:
  ────────────────────────────────────────────────────────
  T=0s     Normal operation, all routes active
           BR ──── Relay ──── Sensors

  T=0.1s   Cranking dip: 12V drops to 6V
           Relay browns out, Thread radio resets

  T=0.5s   Voltage recovers to 13.8V
           Relay reboots, sends MLE Parent Request

  T=1.0s   Relay re-attaches to BR as Router
           Gets same Router ID (Leader remembers)

  T=2.0s   Relay sends MLE Advertisement
           Sensors re-learn route through Relay

  T=3.0s   Full mesh restored
           ─────────────────────────────
           TOTAL DISRUPTION: ~3 seconds


SCENARIO 2: Cellular backhaul lost (tunnel)
═══════════════════════════════════════════

  Timeline:
  ────────────────────────────────────────────────────────
  T=0s     Van enters tunnel, LTE signal lost

  T=0s+    Thread mesh continues normally
           Sensors still report to BR locally
           BR buffers reports in flash ring buffer

  T=5min   Van exits tunnel, LTE reconnects

  T=5m+2s  BR drains buffer to cloud
           300 buffered reports uploaded (6 sensors × 10/min × 5min)

           SENSOR DATA LOSS: zero (buffered locally)


SCENARIO 3: Border Router crash (firmware panic)
════════════════════════════════════════════════

  Timeline:
  ────────────────────────────────────────────────────────
  T=0s     BR crashes, watchdog timer starts

  T=2s     Hardware watchdog resets BR
           BR boots, rejoins Thread as Leader
           (stored credentials in secure element)

  T=4s     BR sends MLE Advertisements
           Relay and direct-attached sensors reconnect

  T=8s     BR re-establishes CASE sessions to cloud

  T=10s    Subscriptions from cloud re-created
           ─────────────────────────────
           TOTAL DISRUPTION: ~10 seconds
           DATA LOSS: events during 0-10s window
           MITIGATION: sensors buffer locally


SCENARIO 4: Sensor battery dies (door contact)
══════════════════════════════════════════════

  Timeline:
  ────────────────────────────────────────────────────────
  T=0      Door contact sensor battery at 5%
           Sensor sends Power Source low-battery event

  T+1h     Battery at 2%, sensor enters shutdown
           Sends final "going offline" event
           Stops responding to polls

  T+25s    Parent router (Relay) detects child timeout
           (no data polls received for 5× poll period)
           Removes from child table

  T+30s    BR receives route update: sensor unreachable
           Reports to cloud: "Door contact offline"
           Fleet dashboard shows alert

  Recovery: technician replaces CR2032
            Sensor reboots, sends MLE Parent Request
            Re-attaches, SRP re-registers
            Cloud subscription auto-recovers
```

## Mapping to MatterThreads Simulation

```
┌─────────────────────────┬────────────────────────────────────────┐
│  REAL VAN                │  SIMULATION                            │
├─────────────────────────┼────────────────────────────────────────┤
│  Border Router (cab)     │  Node 0 (Leader, FTD)                 │
│  Relay Router (wall)     │  Node 1 (Router, FTD)                 │
│  Cargo Sensors (rear)    │  Node 2 (End Device, SED/MED)         │
├─────────────────────────┼────────────────────────────────────────┤
│  Metal partition wall    │  linearChain() topology               │
│  (blocks Node 0↔2)      │  (Node 0↔2 link down)                 │
├─────────────────────────┼────────────────────────────────────────┤
│  Engine cranking EMI     │  link 0 1 30  (30% packet loss)       │
│                          │  latency 0 1 20                       │
├─────────────────────────┼────────────────────────────────────────┤
│  Relay power cycle       │  crash 1  /  restart 1                │
├─────────────────────────┼────────────────────────────────────────┤
│  Sensor battery death    │  crash 2  (no restart)                │
├─────────────────────────┼────────────────────────────────────────┤
│  Tunnel (no cellular)    │  (future: cellular backhaul sim)      │
│                          │  Currently: observe mesh-only behavior│
├─────────────────────────┼────────────────────────────────────────┤
│  Cold chain temp breach  │  write 0 2 1/1026/0 1200              │
│                          │  (write 12.00°C, triggers alert)      │
├─────────────────────────┼────────────────────────────────────────┤
│  Door opened in transit  │  write 0 2 1/69/0 true                │
│                          │  (write door=open, triggers event)    │
└─────────────────────────┴────────────────────────────────────────┘
```
