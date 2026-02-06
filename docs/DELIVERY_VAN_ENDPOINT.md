# Putting a Matter Endpoint in a Delivery Van

## Why Matter in a Vehicle

A delivery van has the same fundamental problem as a smart home: multiple sensors and actuators that need to talk to each other and report to a central system. Door locks, temperature monitors, cargo sensors, lighting — these are all the same device types Matter already defines. The difference is the network moves, loses connectivity, and runs on vehicle power.

Using Matter gives you a vendor-neutral protocol that any fleet management platform can integrate with, and Thread gives you a low-power mesh that doesn't depend on Wi-Fi infrastructure.

---

## What Goes in the Van

### Matter Device Types (Endpoints)

A typical instrumented delivery van would expose these Matter endpoints:

```
Endpoint 0: Root Node (required)
  - Basic Information cluster (van ID, VIN, firmware version)
  - Network Commissioning cluster
  - OTA Software Update cluster

Endpoint 1: Cargo Door Lock
  - Door Lock cluster (0x0101)
  - Lock/unlock via Matter commands
  - Lock state reporting (locked, unlocked, jammed)
  - Audit trail via events

Endpoint 2: Cargo Temperature Sensor
  - Temperature Measurement cluster (0x0402)
  - Reports cargo bay temperature
  - Critical for cold chain (pharmaceuticals, food)
  - Min/max alarms via subscription reports

Endpoint 3: Cargo Humidity Sensor
  - Relative Humidity Measurement cluster (0x0405)
  - Paired with temperature for cold chain compliance

Endpoint 4: Cargo Door Contact Sensor
  - Boolean State cluster (0x0045)
  - Detects door open/close events
  - Triggers alerts if door open during transit

Endpoint 5: Interior Lighting
  - On/Off cluster (0x0006)
  - Level Control cluster (0x0008)
  - Cargo bay lights, controllable by driver or dispatch

Endpoint 6: Occupancy Sensor
  - Occupancy Sensing cluster (0x0406)
  - Detects presence in cargo bay
  - Security: alert if someone is in the bay when doors are locked

Endpoint 7: Power Source
  - Power Source cluster (0x002F)
  - Reports vehicle battery voltage, charging state
  - Alerts on low voltage (van sitting overnight)
```

### Optional Endpoints for Specialized Vans

```
Endpoint 8: Refrigeration Unit Controller
  - Thermostat cluster (0x0201)
  - Setpoint control for reefer unit
  - Compressor on/off via On/Off cluster

Endpoint 9: GPS/Location
  - No standard Matter cluster yet — use a vendor-specific cluster
  - Cluster ID: 0xFFF1FC01 (manufacturer-specific)
  - Attributes: latitude, longitude, speed, heading, fix quality
  - Reports position at configurable interval

Endpoint 10: Fuel/EV Charge Level
  - Power Source cluster (0x002F) or vendor-specific
  - Remaining range, charge percentage
```

---

## Network Architecture

### In-Van Thread Network

The van runs its own self-contained Thread network. The topology is simple because the physical space is small (a van is roughly 3m x 2m x 2m):

```
                    ┌─────────────────────────────────────────────┐
                    │                 DELIVERY VAN                 │
                    │                                             │
                    │   ┌─────────────┐                          │
                    │   │ Border      │ ← Cellular backhaul      │
                    │   │ Router      │   (LTE-M / NB-IoT / 5G)  │
                    │   │ (cab area)  │                          │
                    │   └──────┬──────┘                          │
                    │          │ Thread mesh (802.15.4)           │
                    │          │                                  │
                    │   ┌──────┴──────┐                          │
                    │   │ Router      │ ← Mounted mid-chassis    │
                    │   │ (relay)     │   Extends range to rear  │
                    │   └──────┬──────┘                          │
                    │          │                                  │
                    │   ┌──────┴──────────────────────┐          │
                    │   │         Cargo Bay           │          │
                    │   │                             │          │
                    │   │  [Temp]  [Humidity]  [Door] │          │
                    │   │  [Light] [Occupancy] [Lock] │          │
                    │   │                             │          │
                    │   └─────────────────────────────┘          │
                    └─────────────────────────────────────────────┘
```

**Why a relay router in the middle:** Van cargo bays are often metal-walled (Faraday cage effect). A single Border Router in the cab may not have reliable 802.15.4 radio reach to sensors at the rear doors. A relay router mounted on the cargo partition wall solves this.

### Thread Network Configuration

```
Network Name:     VAN-<VIN-last-6>
PAN ID:           Derived from VIN (unique per van)
Channel:          25 (pick a quiet 802.15.4 channel, avoid 2.4GHz Wi-Fi overlap)
Mesh-Local Prefix: fd<VIN-derived>::/64
Network Key:      Provisioned during van build-out, stored in secure element
```

**Node roles:**
- Border Router (cab): Leader + Border Router. Always powered (connected to vehicle ignition + battery with sleep current).
- Relay Router (partition wall): Router. Powered by vehicle 12V.
- Sensors: Sleepy End Devices (SEDs) or Minimal End Devices (MEDs) depending on power source.
  - Battery-powered sensors (door contact, occupancy): SED, poll period 2-5 seconds
  - Vehicle-powered sensors (temperature, humidity, light): MED or Router, always-on radio

### Backhaul: Thread to Cloud

The Border Router bridges the Thread mesh to the internet via cellular:

```
Thread Devices  ──802.15.4──>  Border Router  ──cellular──>  Cloud/Fleet Mgmt
                                    │
                                    ├── LTE-M (low power, good coverage)
                                    ├── NB-IoT (lowest power, limited throughput)
                                    └── 5G (high throughput, higher power)
```

**Protocol stack for backhaul:**

```
Fleet Platform (cloud)
        │
   Matter over TCP (or UDP with DTLS)
        │
   IPv6 over cellular PDN
        │
   LTE-M / NB-IoT / 5G radio
        │
   Border Router (van)
        │
   Thread IPv6 mesh
        │
   Matter endpoints (sensors/actuators)
```

The Border Router runs a Matter controller that:
1. Maintains CASE sessions to all in-van Thread devices
2. Subscribes to critical attributes (temperature, door state, lock state)
3. Forwards subscription reports to the cloud via cellular
4. Accepts commands from the cloud (unlock door, turn on light) and proxies them to Thread devices

---

## Hardware Bill of Materials

### Border Router (Cab Unit)

| Component | Part | Notes |
|-----------|------|-------|
| MCU + Thread radio | Nordic nRF5340 + nRF21540 | Dual-core Cortex-M33, 802.15.4, range-extended |
| Cellular modem | Quectel BG95-M3 | LTE-M + NB-IoT + GNSS |
| SIM | Industrial eSIM or MFF2 SIM | Soldered, vibration-resistant |
| GNSS antenna | Active patch antenna | Mounted on roof |
| 802.15.4 antenna | PCB antenna or external dipole | Pointed toward cargo bay |
| Cellular antenna | External LTE antenna | Mounted on roof |
| Power | 12V vehicle → 3.3V buck converter | Wide input range (9-32V for load dump) |
| Enclosure | IP54 rated, DIN-rail or bracket mount | Vibration: ISO 16750 |
| Secure element | ATECC608B or SE050 | Stores Thread network key + Matter DAC |

Estimated cost: $45-80 per unit at volume.

### Relay Router (Partition Wall)

| Component | Part | Notes |
|-----------|------|-------|
| MCU + Thread radio | Nordic nRF52840 | Cortex-M4, 802.15.4 |
| Power | 12V vehicle → 3.3V | Simple buck converter |
| Enclosure | IP44 | Interior mount, less environmental stress |

Estimated cost: $12-20 per unit at volume.

### Cargo Bay Sensors

| Sensor | Chipset | Power | Matter Cluster |
|--------|---------|-------|---------------|
| Temperature + Humidity | Sensirion SHT40 + nRF52840 | Vehicle 12V | 0x0402 + 0x0405 |
| Door contact | Reed switch + nRF52833 | CR2032 battery | 0x0045 |
| Door lock controller | Motor driver + nRF52840 | Vehicle 12V | 0x0101 |
| Occupancy (PIR) | Panasonic EKMC + nRF52833 | CR2032 battery | 0x0406 |
| Cargo bay light | LED driver + nRF52840 | Vehicle 12V | 0x0006 + 0x0008 |

---

## Commissioning and Provisioning

### Factory Provisioning (During Van Build-Out)

Each van is provisioned on the assembly line or at the upfitter:

```
1. Flash firmware to all Thread devices (BR, relay, sensors)
2. Generate Thread network credentials:
   - Network key (random, stored in secure element on each device)
   - PAN ID (derived from VIN)
   - Channel (assigned per fleet to avoid interference)
3. Pre-commission all devices into a Matter fabric:
   - Fleet fabric ID (one per fleet operator)
   - Each device gets a Node ID and NOC
   - Fabric root CA is the fleet operator's PKI
4. Program device attestation certificates (DAC):
   - Unique per device
   - Signed by the fleet operator's PAI (Product Attestation Intermediate)
5. Store van metadata:
   - VIN, van ID, depot assignment
   - Sensor locations (which endpoint is the rear door vs. side door)
6. Test: verify all endpoints respond to read requests
```

This is done with a commissioning station that:
- Connects to the van's Thread network via a temporary Border Router
- Runs Matter commissioning (PASE with setup codes printed on device labels)
- Provisions fabric credentials via CASE
- Verifies all attributes readable

### Field Replacement

When a sensor fails in the field:

```
1. Technician installs new physical sensor
2. Scans QR code on the sensor with fleet management app
3. App sends commissioning request via cellular to the van's Border Router
4. Border Router performs PASE with the new sensor over Thread
5. Border Router provisions the sensor into the fleet fabric
6. Sensor starts reporting to the existing subscription
```

This works even if the van is at a remote depot with no Wi-Fi. The phone talks to the cloud, the cloud talks to the van's Border Router over cellular, and the BR commissions the sensor locally over Thread.

---

## Power Management

Vehicle power is hostile: load dumps (up to 40V spikes), cranking dips (down to 6V), and the van may sit for days without starting.

### Power States

```
State           Vehicle     BR Power        Sensors Power    Cellular
─────────────────────────────────────────────────────────────────────
Engine On       Running     Full (50mA)     Full             Connected
Ignition Off    Acc off     Low power       SED mode         Periodic wake
                            (5mA sleep,                      (every 15min
                             wake every                       check-in)
                             30s for poll)
Deep Sleep      Parked      Ultra-low       Off or           Off
(>24h parked)   overnight   (50μA, wake     battery-only     (wake on
                            on interrupt)   sensors only     CAN bus or
                                                             timer)
```

### Battery Budget

Vehicle battery (typical 12V 60Ah):

```
Deep sleep draw:  50μA (BR) + 20μA (relay) + 10μA×4 (battery sensors) = 110μA
Days to drain:    60,000mAh / 0.11mA = 545,454 hours = 62,000 days

Low power draw:   5mA (BR) + 1mA (relay) + 0.5mA×4 (sensors) = 8mA
Days to drain:    60,000mAh / 8mA = 7,500 hours = 312 days
```

Battery drain is not a concern even in the low power state. The cellular modem is the biggest consumer (peaks at 500mA during transmit), which is why it uses periodic wake in ignition-off state.

---

## Connectivity Challenges

### Problem: Cellular Dead Zones

The van drives through tunnels, rural areas, and underground parking garages.

**Solution:** The Border Router buffers subscription reports and events locally. When connectivity returns, it bulk-uploads the backlog. The Thread mesh operates independently of cellular — sensors continue reporting to the BR regardless of backhaul status.

```
Architecture:
  Thread devices → BR (local subscription reports, always works)
  BR → local ring buffer (stores reports when offline)
  BR → cellular → cloud (drains buffer when connected)
```

Buffer sizing: 1MB flash ring buffer holds approximately 50,000 sensor reports (20 bytes each). At one report per sensor per 30 seconds with 6 sensors, that is 12 reports/minute = 720/hour. The buffer holds ~69 hours of offline data.

### Problem: Van-to-Van Interference

Multiple vans parked at a depot, each running its own Thread network on the same 802.15.4 channel.

**Solution:**
- Assign channels per van or per depot (802.15.4 has 16 channels: 11-26)
- Use unique PAN IDs (derived from VIN, guaranteed unique)
- Thread's MAC-layer filtering drops frames from foreign PANs
- If a depot has more than 16 vans, stagger channels across vans (Thread can scan and pick the quietest channel)

### Problem: Thread Network Disruption from Vehicle Electrical Noise

Starter motors, alternators, ignition systems, and electric power steering generate significant EMI in the 2.4GHz band.

**Solution:**
- Use shielded cabling for 802.15.4 antenna feeds
- Mount the 802.15.4 antenna away from the engine bay
- Use the nRF21540 range extender (+20dBm TX, -10dBm RX sensitivity) to overpower noise
- Thread's CSMA-CA and automatic retransmissions handle transient interference

### Problem: Cold Chain Compliance Requires Continuous Logging

Pharmaceutical and food deliveries require unbroken temperature records with timestamps.

**Solution:**
- Temperature sensor stores readings locally in flash (redundant to BR buffer)
- Sensor tags each reading with a monotonic counter and Unix timestamp
- BR verifies no gaps in the sequence when it receives reports
- If a gap is detected (sensor was briefly unreachable), the BR requests the missing readings from the sensor's local buffer via a vendor-specific Matter command
- On delivery, the complete temperature log is uploaded to the cloud and attached to the delivery receipt

```
Endpoint 2 (Temperature Sensor):
  Standard clusters:
    - Temperature Measurement (0x0402)

  Vendor-specific cluster (0xFFF1FC10):
    Attributes:
      0x0000: log_entry_count (uint32)
      0x0001: oldest_entry_timestamp (epoch_s, uint32)
      0x0002: newest_entry_timestamp (epoch_s, uint32)
    Commands:
      0x00: ReadLogRange(start_index, count) -> array of {timestamp, temp_c}
      0x01: ClearLog()
```

---

## Fleet Management Integration

### Architecture

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│  Van 1   │     │  Van 2   │     │  Van N   │
│  (Thread  │     │  (Thread  │     │  (Thread  │
│   mesh)  │     │   mesh)  │     │   mesh)  │
└────┬─────┘     └────┬─────┘     └────┬─────┘
     │ cellular       │ cellular       │ cellular
     └────────────────┼────────────────┘
                      │
              ┌───────┴────────┐
              │  Fleet Gateway  │  (cloud)
              │  (Matter        │
              │   Controller)   │
              └───────┬────────┘
                      │ REST/gRPC
              ┌───────┴────────┐
              │  Fleet Mgmt    │
              │  Platform      │
              │  (dashboard,   │
              │   routing,     │
              │   compliance)  │
              └────────────────┘
```

### Data Flow

**Periodic telemetry (every 30s):**
```json
{
  "van_id": "VAN-A1B2C3",
  "timestamp": "2026-02-06T14:30:00Z",
  "endpoints": {
    "temperature": {"value_c": 4.2, "endpoint": 2},
    "humidity": {"value_pct": 65.1, "endpoint": 3},
    "door_rear": {"open": false, "endpoint": 4},
    "lock_rear": {"state": "locked", "endpoint": 1},
    "cargo_light": {"on": false, "endpoint": 5},
    "occupancy": {"occupied": false, "endpoint": 6},
    "battery_v": 13.8
  },
  "gps": {"lat": 37.7749, "lon": -122.4194, "speed_kmh": 45}
}
```

**Event-driven alerts (immediate):**
```json
{
  "van_id": "VAN-A1B2C3",
  "timestamp": "2026-02-06T14:31:15Z",
  "alert": "DOOR_OPENED_IN_TRANSIT",
  "endpoint": 4,
  "gps": {"lat": 37.7751, "lon": -122.4190, "speed_kmh": 30},
  "severity": "critical"
}
```

**Remote commands (from dispatch):**
```
Dispatch → Cloud → Cellular → BR → Thread → Door Lock endpoint
  "Unlock rear door for delivery at stop #5"
  InvokeCommand: endpoint=1, cluster=0x0101, command=UnlockDoor
  With timed invoke (30s timeout): door re-locks automatically
```

---

## Testing with MatterThreads

Our simulation framework can model the in-van scenario directly. The 3-node topology maps naturally:

```
Simulation Node 0  →  Border Router (cab, Leader)
Simulation Node 1  →  Relay Router (partition wall, Router)
Simulation Node 2  →  Cargo Sensor (rear, End Device)
```

### Test Scenarios to Run

**1. Relay router power cycle (engine start/stop):**
```
matterthreads --topology linear
> subscribe 0 2 1/1026/0 5 60     # Subscribe to temperature
> crash 1                           # Kill relay router (engine off)
> # Observe: does subscription recover?
> restart 1                         # Relay comes back (engine start)
> metrics                           # Check subscription recovery time
```

**2. Sensor unreachability during drive:**
```
matterthreads --topology linear --seed 42
> link 1 2 30                       # 30% packet loss (vibration/EMI)
> latency 1 2 50                    # 50ms added latency
> subscribe 0 2 1/1026/0 5 60
> # Run for 2 minutes, check subscription report intervals
> metrics
```

**3. Cold chain monitoring gap:**
```
matterthreads --topology linear
> subscribe 0 2 1/1026/0 5 30      # Subscribe to temp, 30s max interval
> link 0 2 down                     # Total link failure
> # Wait 90 seconds
> link 0 2 up                       # Link restored
> timeline                          # Check: how long was the gap?
> metrics                           # Check: subscription recovery time
```

**4. Door security alert during partition:**
```
matterthreads --topology star
> subscribe 0 2 1/69/0 5 10        # Subscribe to door contact on Node 2
> crash 1                           # Kill relay, partitioning Node 2
> # Simulate: door opens on Node 2 but alert can't reach BR
> restart 1                         # Relay restored
> timeline                          # Was the door event delivered after recovery?
```

---

## Regulatory and Certification

### Matter Certification
- Each device type (sensor, lock, BR) needs separate Matter certification
- Vendor ID from the Connectivity Standards Alliance (CSA)
- Product Attestation Authority (PAA) for your organization
- Test at an authorized test lab

### 802.15.4 / Thread Radio
- FCC Part 15.247 (US) or CE RED (EU) for the 2.4GHz radio
- Thread 1.3+ certification from the Thread Group
- Each unique hardware design needs its own radio certification

### Vehicle Installation
- SAE J1455 (electrical systems in trucks)
- ISO 16750 (environmental conditions for electrical equipment in vehicles)
- FMVSS compliance if any device interfaces with vehicle safety systems
- E-Mark (ECE R10) for electromagnetic compatibility in vehicles

### Cold Chain Specific
- FDA 21 CFR Part 11 (electronic records) if carrying pharmaceuticals
- FSMA (Food Safety Modernization Act) for food transport
- GDP (Good Distribution Practice) for EU pharmaceutical logistics
- Temperature logs must be tamper-evident and include device calibration records
