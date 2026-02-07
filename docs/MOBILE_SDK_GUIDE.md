# Matter Mobile SDK Guide — Choosing the Right Stack

## Overview

Building a Matter controller for mobile (phone app for commissioning, diagnostics, and fleet interaction) requires choosing between three SDK options. This guide evaluates each for a delivery van fleet use case where you need your own fabric, cross-platform support, and server-side control.

---

## SDK Comparison

### 1. Apple Matter Framework (iOS only)

**Frameworks**: `Matter.framework` + `MatterSupport.framework`
**Available since**: iOS 16.1, iPadOS 16.1, macOS Ventura, tvOS 16.1

Apple ships a certified Matter SDK in Xcode. It provides:

- System-level BLE scanning and pairing UI
- PASE commissioning (setup code, QR scan)
- CASE operational sessions
- Thread credential sharing from Apple Border Routers (HomePod, Apple TV 4K)
- Wi-Fi network selection during commissioning
- HomeKit bridge — Matter devices appear in Apple Home automatically

**Pros**:
- Deep iOS integration (system BLE dialogs, credential management)
- Handles Thread network joining automatically via Apple's Thread Border Routers
- Polished commissioning UX out of the box

**Cons**:
- iOS/macOS only — no Android
- Apple controls the primary fabric; creating your own fabric is possible but fights the system design
- Legacy HomeKit framework ends **February 10, 2026** — must use the new Home architecture
- Less flexibility for fleet/enterprise scenarios where Apple Home isn't the hub

**Best for**: Consumer smart home apps on Apple devices.

**References**:
- https://developer.apple.com/documentation/matter
- https://developer.apple.com/apple-home/matter/

---

### 2. Google Home Mobile SDK (Android only)

**SDK**: Google Home Mobile SDK for Android
**Requires**: Google Play Services, Google Home Developer Console registration

Google's SDK provides:

- Commission devices to Google's fabric or a third-party fabric
- BLE, Wi-Fi, and Thread commissioning flows
- Intent-based commissioning (`ACTION_COMMISSION_DEVICE`)
- Device sharing between controllers
- Sample app: [google-home/sample-apps-for-matter-android](https://github.com/google-home/sample-apps-for-matter-android)

**Pros**:
- Smooth Android integration
- Supports third-party fabric commissioning
- Well-documented codelabs and sample apps

**Cons**:
- Android only — no iOS
- Depends on Google Play Services (not available on all Android devices)
- Requires Google Home Developer Console project setup
- Google ecosystem assumptions (Google Home as primary controller)

**Best for**: Android apps that work alongside Google Home.

**References**:
- https://developers.home.google.com/matter/apis/home
- https://developers.home.google.com/matter/apis/home/commissioning
- https://developers.home.google.com/codelabs/matter-sample-app

---

### 3. connectedhomeip SDK (Cross-platform, Open Source)

**Repository**: [project-chip/connectedhomeip](https://github.com/project-chip/connectedhomeip)
**License**: Apache 2.0

The CSA reference implementation. Provides platform-specific controller bindings:

| Platform | API | Language |
|----------|-----|----------|
| iOS / macOS | Darwin Framework | Objective-C / Swift |
| Android | CHIPTool APIs | Java / Kotlin |
| Linux / macOS | chip-tool | C++ CLI |
| Any | Python controller | Python |

**Pros**:
- Full control over your own fabric — no Apple/Google dependency
- Cross-platform: same codebase targets iOS, Android, Linux, macOS
- Server-side controllers (chip-tool, Python) for fleet gateway
- The actual reference code that Apple and Google build on top of
- Ideal for testing and CI/CD integration

**Cons**:
- More integration work (no system-level BLE pairing shortcuts)
- Must handle BLE scanning, Thread credential provisioning yourself
- Build system is complex (GN + Ninja, large dependency tree)
- Commissioning UX is your responsibility

**Best for**: Enterprise/fleet use cases, cross-platform apps, server-side controllers.

**References**:
- https://github.com/project-chip/connectedhomeip
- https://project-chip.github.io/connectedhomeip-doc/guides/darwin.html
- https://github.com/project-chip/connectedhomeip/tree/master/examples/android/CHIPTool

---

## Recommendation: Delivery Van Fleet

For a delivery van fleet, **connectedhomeip SDK** is the right choice:

### Why

1. **Own fabric** — Fleet vehicles aren't in someone's "home". Apple Home and Google Home don't make sense as the primary controller. You need a fleet-managed fabric where your cloud gateway is the fabric admin.

2. **Cross-platform** — Drivers may use Android or iOS. The Darwin Framework and Android CHIPTool APIs share the same underlying Matter stack.

3. **Server-side control** — The fleet gateway in the cloud needs to be a full Matter controller (CASE sessions, subscriptions, command relay). That runs on Linux using chip-tool or the Python controller.

4. **Testing** — chip-tool on Linux/macOS integrates directly with the MatterThreads simulation framework.

5. **Certification** — Starting in 2026, all Matter products require the CSA Alliance Interop Test. Using the reference SDK keeps you closest to the certification test harness.

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        FLEET CLOUD                              │
│                                                                 │
│  ┌──────────────────────┐    ┌───────────────────────────────┐  │
│  │  Fleet Gateway        │    │  Dispatch Dashboard           │  │
│  │                       │    │  (Web UI)                     │  │
│  │  chip-tool or Python  │    │                               │  │
│  │  controller           │◄──►│  REST/gRPC API                │  │
│  │                       │    │                               │  │
│  │  - Fabric admin       │    │  - Van status                 │  │
│  │  - CASE to all vans   │    │  - Lock/unlock                │  │
│  │  - Subscriptions      │    │  - Temperature alerts         │  │
│  │  - Command relay      │    │  - Route management           │  │
│  └──────────┬────────────┘    └───────────────────────────────┘  │
│             │                                                    │
╚═════════════╪════════════════════════════════════════════════════╝
              │ Matter over TCP/IPv6 (cellular)
              │
       ┌──────┴──────┐
       │              │
  ┌────▼────┐   ┌─────▼───┐
  │  VAN 1  │   │  VAN N   │    Border Routers (nRF5340 + LTE-M)
  └─────────┘   └──────────┘

       ┌──────────────────────────────────────────┐
       │           DRIVER PHONE APP               │
       │                                          │
       │  iOS: Darwin Framework (connectedhomeip) │
       │  Android: CHIPTool APIs (connectedhomeip)│
       │                                          │
       │  Used for:                               │
       │  - On-site commissioning (BLE + PASE)    │
       │  - Diagnostics (read sensors, check mesh)│
       │  - Manual lock/unlock                    │
       │  - Wi-Fi/Thread network provisioning     │
       │                                          │
       │  NOT the primary controller              │
       │  (fleet gateway is fabric admin)         │
       └──────────────────────────────────────────┘
```

### Commissioning Flow

```
Driver Phone                  Van Border Router           Fleet Gateway
     │                              │                          │
 1.  │──BLE advertisement scan──►   │                          │
     │  (discover _matterc._udp)    │                          │
     │                              │                          │
 2.  │──PASE (setup code)────────►  │                          │
     │  Establish PASE session      │                          │
     │  via BLE                     │                          │
     │                              │                          │
 3.  │──Thread credentials────────► │                          │
     │  (operational dataset)       │                          │
     │                              │                          │
 4.  │──AddNOC (fleet CA cert)────► │                          │
     │  Commission onto fleet       │                          │
     │  fabric with NOC from        │                          │
     │  fleet CA                    │                          │
     │                              │                          │
 5.  │                              │──CASE (Sigma1/2/3)─────► │
     │                              │  Establish operational   │
     │                              │  session to cloud        │
     │                              │                          │
 6.  │                              │◄──Subscribe──────────────│
     │                              │  Cloud subscribes to     │
     │                              │  all van endpoints       │
     │                              │                          │
 7.  │◄──"Commissioned OK"─────────│                          │
     │                              │                          │
```

### Phone App Implementation

#### iOS (Darwin Framework)

```objc
// Commission a van's Border Router
#import <Matter/Matter.h>

MTRDeviceController *controller = [MTRDeviceController sharedController];

// Scan QR code on van to get setup payload
MTRSetupPayload *payload = [MTRSetupPayload setupPayloadWithOnboardingPayload:qrString
                                                                        error:&error];

// Start commissioning over BLE
MTRCommissioningParameters *params = [[MTRCommissioningParameters alloc] init];
params.threadOperationalDataset = fleetThreadDataset;  // Fleet's Thread network credentials

[controller pairDevice:deviceId
         setupPINCode:payload.setupPINCode
        discriminator:payload.discriminator
         setupPayload:payload
                error:&error];
```

#### Android (CHIPTool APIs)

```kotlin
// Commission a van's Border Router
val controller = ChipDeviceController()

// Scan QR code
val payload = SetupPayloadParser().parseQrCode(qrString)

// Start commissioning over BLE
controller.pairDevice(
    bleManager.getConnectedDevice(),
    deviceId,
    payload.setupPinCode,
    NetworkCredentials.forThread(
        ThreadOperationalDataset(fleetThreadCredentials)
    )
)
```

#### Fleet Gateway (chip-tool on Linux)

```bash
# Commission (usually done by phone, but gateway can also commission over IP)
chip-tool pairing code-thread $NODE_ID hex:$THREAD_DATASET $SETUP_CODE

# Subscribe to temperature on van's temp sensor
chip-tool temperaturemeasurement subscribe measured-value 10 60 $NODE_ID 2

# Unlock door
chip-tool doorlock unlock-door $NODE_ID 1 --timedInteractionTimeoutMs 30000

# Read all endpoints
chip-tool descriptor read parts-list $NODE_ID 0
```

---

## Build Notes

### Building connectedhomeip for iOS

```bash
git clone --recurse-submodules https://github.com/project-chip/connectedhomeip.git
cd connectedhomeip
source scripts/activate.sh

# Build the Darwin framework
xcodebuild -project src/darwin/Framework/Matter.xcodeproj \
    -scheme Matter \
    -sdk iphoneos \
    -configuration Release
```

### Building connectedhomeip for Android

```bash
cd connectedhomeip
source scripts/activate.sh
source scripts/bootstrap.sh

# Build Android libraries
./scripts/build/build_examples.py \
    --target android-arm64-chip-tool \
    build

# Or build the CHIPTool app
cd examples/android/CHIPTool
./gradlew assembleDebug
```

### Building chip-tool for fleet gateway (Linux)

```bash
cd connectedhomeip
source scripts/activate.sh

# Build chip-tool
gn gen out/debug
ninja -C out/debug chip-tool

# Or use the Python controller
pip install chip-core chip-repl
```

---

## Integration with MatterThreads Simulation

The MatterThreads framework's Node 3 (phone) simulates the phone controller's behavior:

| Real Phone Action | MatterThreads Equivalent |
|-------------------|--------------------------|
| BLE scan for devices | `discover` command |
| PASE commissioning | `commission 3 0` (phone → BR) |
| Read temperature | `read 3 2 1/1026/0` (phone reads sensor via BR) |
| Subscribe to door state | `subscribe 3 2 1/69/0 5 60` |
| Unlock door | `invoke 3 2 1/0x0101/unlock` |
| Lose cellular | `backhaul down` |
| Enter tunnel | `tunnel 300` (5 min) |
| Engine crank interference | `crank` |

Test scenarios to validate before deploying the real phone app:

```bash
# Test: Phone commissions van, subscribes, then enters tunnel
matterthreads --topology van --scenario phone-commission-tunnel --seed 42

# Test: Phone loses backhaul during active subscription
matterthreads --topology van --scenario backhaul-loss-recovery --seed 42

# Test: Driver unlocks door while relay is rebooting
matterthreads --topology van --scenario unlock-during-crank --seed 42
```

---

## Certification (2026+)

Starting in 2026, all Matter integrations require the **CSA Alliance Interop Test**. Key points:

- Controller apps (phone or server) must pass interop testing
- Use the reference SDK (connectedhomeip) to stay closest to the test harness
- The `chip-tool` and `chip-cert` utilities are used in the official test suite
- Matter 1.4.2 (August 2025) added security enhancements
- Matter 1.5 (November 2025) added camera, soil moisture, energy management device types
- For van endpoints, the most relevant device types are: Temperature Sensor, Door Lock, Contact Sensor, Occupancy Sensor, Light, Power Source

---

## Summary

| Criteria | Apple Framework | Google SDK | connectedhomeip |
|----------|----------------|------------|-----------------|
| iOS | Yes | No | Yes (Darwin) |
| Android | No | Yes | Yes (CHIPTool) |
| Linux/Server | No | No | Yes (chip-tool, Python) |
| Own Fabric | Difficult | Possible | Native |
| Commissioning UX | System-level | Intent-based | DIY |
| Fleet/Enterprise | Poor fit | Moderate fit | Best fit |
| Certification alignment | Good | Good | Best |
| Build complexity | Low (Xcode) | Medium (Gradle) | High (GN+Ninja) |

**Bottom line**: Use connectedhomeip directly. It gives you full control over your fleet fabric, works on all platforms, and keeps you closest to the certification test harness. Use Apple/Google frameworks only if you also need HomeKit/Google Home interop for consumer scenarios.
