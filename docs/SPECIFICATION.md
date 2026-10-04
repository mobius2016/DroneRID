# DroneRID specification and implementation boundary

## Purpose

A passive Remote ID packet inspection app for Flipper Zero, with a single Catalog-compatible FAP, optional official ESP32-S2 Wi-Fi Devboard companion, and a shared parser/model/UI. Receiving an RID message demonstrates its presence; this hardware does not establish absence of nearby drones or authenticate a claimed identity.

The referenced conversation contains design discussion but no completed comprehensive specification. This document consolidates that discussion and the current request, correcting assumptions against official source.

## Capability contract

| Configuration | Requested behavior | Current achievable behavior |
|---|---|---|
| Stock Flipper, no board | Useful standalone RID reception | Saved-capture inspection and capabilities page; **no live native BLE reception** |
| Official Full BLE stack | Automatic enhanced BLE reception | Stack recognized at runtime; **scanner API still unavailable** |
| Official ESP32-S2 board with DroneRID companion | Automatically add Wi-Fi | Versioned heartbeat enables live Wi-Fi Beacon packet processing; timeout disables it |
| Board with factory debugger firmware | Automatic Wi-Fi | Not supported until companion is installed; factory firmware does not send RID |
| STM32WB55 LE Coded PHY | Never assume support | Unsupported, no implementation or claim |
| Wi-Fi NAN / 5 GHz | Investigate | Unsupported in this build; ESP32-S2 is 2.4 GHz |

No version of this FAP can unlock an absent host firmware API merely by detecting Full. Enabling a future official scanning API will require implementing its adapter and rebuilding against that SDK. Supporting older SDKs with one binary then depends on the official ABI's availability rules. Do not import new symbols unconditionally and claim backward compatibility.

## Architecture

`UART / saved .rid stream -> framing + CRC -> RidPacket -> bounds/type validation -> OpenDroneID -> RidStore -> UI`

`Wi-Fi management frame -> beacon adapter -> RidPacket -> UART encoder`

`BLE advertising data -> BLE AD adapter -> RidPacket` is implemented and host-tested for future integration/import tools. It is not connected to a live Flipper radio backend. Every transport uses the same length-delimited packet model. A replay session is visibly distinct from live input and cannot mix packets into live tracks.

The portable C core has no Furi or ESP-IDF dependencies. A fixed-size eight-track store bounds memory. Keys are transport plus source MAC: identity collisions and MAC rotation are not automatically merged. Both transports appear through the same UI, but this is not proof that equal IDs from different addresses represent one aircraft. Oldest tracks are evicted by wrap-safe arrival age. Changed Basic ID on the same address resets retained state.

## Parsing and validation

Support the reference library's Basic ID, Location, Authentication pages, Self ID, System, Operator ID, and up to nine-message packs. Accept known protocol versions 0–2; reject unknown future versions, nested packs, impossible lengths, disallowed duplicate message types, and out-of-domain coordinates. Only complete valid packets update a track. Authentication payloads are decoded, not cryptographically verified. This is reference-based compatibility, not formal ASTM conformance certification.

Each message class has its own last-received time. A fresh Basic ID cannot make an old Location or System field fresh. Location and operator position expire from the display after 15 seconds. Unknown sentinel values remain unknown. Untrusted textual IDs are bounded and sanitized for the screen.

## User interface

Six pages: status, aircraft location/identity, operator position, telemetry, raw payload, diagnostics. Left/right changes page; up/down selects one of eight track slots. Raw page OK advances through the full payload, 24 bytes at a time. Status OK opens a saved `.rid` file or returns from replay to live mode. Back exits. RSSI, reception age and transport/channel accompany parsed data. Capability text must report BLE scanner absence even with Full installed.

## UART and concurrency

115200 8N1, Flipper USART pins 13/14, ESP GPIO44/43. Receive-only from the FAP. A protocol-v1, CRC-checked heartbeat must precede live Beacon data. Heartbeat once per second; disconnect after three seconds without one. Factory devboard output must not enable a receiver. The FAP temporarily disables the expansion service per its documented contract and restores saved settings on exit. A busy USART produces a status message rather than stealing another app's handle.

Flipper interrupt callbacks only enqueue bytes; parsing and storage occur in the app thread. GUI reads share a mutex. Queue overflow is counted; line framing and CRC recover at a later newline. ESP Wi-Fi callbacks enqueue bounded packets without serial blocking. A worker emits records and heartbeats. Companion queue overflow is counted internally; exposing that counter on the FAP is a follow-up.

## Capture and replay

Accepted live packets append to `apps_data/drone_rid/capture.rid`, synchronized every two seconds and closed on exit. The file is capped at 4 MiB; full/error status stops writes while inspection continues. Move or delete the file to resume a new capture. Replay accepts this format and the supplied synthetic fixture, with explicit REPLAY labeling. Replay uses processing-time ages and runs as fast as read; the initial format does not preserve original wall-clock timing or log rejected raw frames. Timestamped sessions, paced playback and configurable logging remain release improvements.

## Wi-Fi scope

Passively inspect beacon vendor information elements with OUI FA:0B:BC, type 0D, skip message counter, forward RID payload. Check all bounds before reads. Management FCS is removed using ESP-IDF's documented length convention. No active probe, AP connection or injected frame is used. Channel 6 receives alternating 250 ms dwell periods with channels 1–11; channel switching necessarily misses traffic. Channel 12/13, Wi-Fi NAN, 5 GHz, sensitivity characterization and emitter interoperability remain unimplemented or unverified.

## Release gates

Host sanitizers and official SDK linkage must pass. Actual stock-Light, Full-stack, board attach/detach, UART conflict, SD removal/full, sustained traffic and exit/relaunch tests must be recorded on hardware. Confirm matching packets with a known RID broadcaster/reference receiver. Capture genuine qFlipper screenshots. Publish a source repository at an immutable commit and validate the Catalog manifest. No Catalog-ready or RF-validated claim before those steps.

Stock live reception and Full enhanced reception remain blocked on official firmware support. Saved-file usefulness is a partial fulfillment of the standalone requirement, not a replacement silently presented as the original goal.
