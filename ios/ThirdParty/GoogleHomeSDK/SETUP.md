# Google Home APIs iOS SDK — drop-in setup

`GoogleHomeAPIsBackend.swift` is written against the real Google Home APIs iOS SDK but
is guarded by `#if canImport(GoogleHomeSDK)`, so the app **compiles today without the
SDK** (the backend simply reports `isAvailable == false`). This directory is the
placeholder where the SDK goes. Once you complete the steps below, `canImport(GoogleHomeSDK)`
becomes true and the backend goes live.

> The SDK **binary is not in this repo** — it's download-gated behind Google Home
> Developers sign-in (no public URL), so it can't be fetched automatically.

## 1. Download the SDK
1. Sign in at <https://developers.home.google.com/apis/ios/sdk> (Google account required).
2. Download the Home APIs iOS SDK archive.
3. **Unpack it into this directory** (`ios/ThirdParty/GoogleHomeSDK/`), replacing this
   `SETUP.md`. You should end up with the `GoogleHomeSDK` + `GoogleHomeTypes` package/xcframeworks here.

## 2. Add to the Xcode project
Local Swift Package (preferred):
- Xcode → **File ▸ Add Package Dependencies… ▸ Add Local…** → select this `GoogleHomeSDK` dir.
- In *Add to Target*, add **both** `GoogleHomeSDK` and `GoogleHomeTypes` to the `MatterAI` target.
- Build Settings: add `$(inherited)` to **Other Linker Flags**, set **User Script Sandboxing** to **No**.

(CocoaPods / manual xcframework alternatives: see the SDK page above.)

## 3. Capabilities / entitlements (on the `MatterAI` target)
- **App Attest** (DeviceCheck) — the SDK attests every app instance. Consequence: the SDK
  **cannot run in the iOS Simulator**; commissioning must be tested on a **physical device**.
- **App Groups** — one group (e.g. `group.<your-bundle>.commissioning`) shared between the
  app and the MatterExtension target; passed as `sharedAppGroup` in `Home.configure`.
- Provisioning profile must carry both App Attest and App Groups.

## 4. MatterExtension target (for commissioning)
Add an app extension (Apple **MatterSupport**) subclassing
`MatterAddDeviceExtensionRequestHandler`; join it to the **same App Group** as step 3.
The system add-device sheet drives onboarding out-of-process.

## 5. Google Cloud project + OAuth
- Enable **Home API** (APIs & Services ▸ Enable APIs and Services).
- Configure the OAuth consent screen; create an **iOS OAuth client** with the app's Bundle ID
  and Apple Developer Team ID. Note the **Client ID**.

## 6. Initialize at launch
```swift
Home.configure {
    $0.clientID = "<your-oauth-client-id>"
    $0.sharedAppGroup = "group.<your-bundle>.commissioning"
}
// then, when the user connects:  try await Home.connect()  (or Home.restoreSession())
```
Enable the backend at runtime via `MatterHomeSDK.enableGoogleHomeAPIs()` (Settings toggle
also wires this once you build the UI entry).

## 7. Runtime requirement
Matter commissioning through Google's fabric needs an **online Cast-OS Nest hub**
(Nest Hub / Mini / Audio / Hub Max) in the user's structure. The SDK **cannot create its
own fabric** — it adds devices to the user's existing Google Home fabric via the hub.

---

### Symbol notes for `GoogleHomeAPIsBackend.swift`
Verified against Google's official sample (`github.com/google-home/google-home-api-sample-app-ios`):
- Device types are top-level (e.g. `OnOffLightDeviceType`), **not** `Matter.`-prefixed; traits **are**
  (`Matter.OnOffTrait`).
- `await device.types.get(SomeDeviceType.self)` returns an **optional** (no `try`).
- Typed trait accessors carry the `Trait` suffix: `matterTraits.onOffTrait`, `.levelControlTrait`, etc.

Still to confirm against the real headers at first compile: `DoorLockTrait` lock/unlock command
labels, thermostat setpoint accessors, `PowerSourceDeviceType`, and the exact
`Structure.completeMatterCommissioning()` shape.
