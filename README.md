# DroneRID

Remote ID packet inspection for Flipper Zero, with saved-capture replay and an optional ESP32-S2 Wi-Fi Beacon receiver.

**Development prototype — not yet hardware-validated or Catalog-ready.**

**Current limit:** official Flipper firmware 1.4.3 has no exported passive BLE scanning API. Full BLE stack detection works, but Full alone does not enable scanning. Standalone mode inspects saved captures. Live reception requires the official ESP32-S2 Wi-Fi Devboard flashed with this project's companion firmware, and currently covers 2.4 GHz Wi-Fi Beacon RID only.

One FAP handles capability display, capture replay, live UART reception, decoding, track inspection and logging. No unofficial Flipper firmware dependency. The factory devboard debugger firmware must be replaced with the companion for Wi-Fi reception. No device has been flashed during development.

## Build on macOS

For a fresh checkout, install Apple's Command Line Tools if needed (`xcode-select --install`), then:

```sh
git clone https://github.com/mobius2016/DroneRID.git
cd DroneRID
./scripts/setup.sh
source venv/bin/activate
./scripts/test.sh
./scripts/build-flipper.sh
./scripts/build-companion.sh
./scripts/package.sh
```

If you already have a checkout and environment, skip cloning and setup. The setup script creates/reuses `venv`, installs pinned Python entry-point tools there, and obtains the official release SDK. Tools, SDKs and the ESP-IDF dependency environment stay under `.tools`. Nothing installs into the base Python environment. Android Studio is not needed for either firmware target.

Build outputs:

- `flipper/dist/drone_rid.fap` — compiled for official release SDK 1.4.3, API 87.1.
- `companion/.pio/build/devboard/firmware.bin` — companion application.
- `dist/companion-merged.bin` — complete companion image for offset 0, made by `scripts/package.sh`.
- `companion/.pio/build/devboard/bootloader.bin` and `partitions.bin` — companion flashing dependencies.

The SDK setup follows the official release channel, so future runs can select a newer SDK. Consult `docs/PROVENANCE.md` for the exact tested versions. The [GitHub Actions workflow](https://github.com/mobius2016/DroneRID/actions) builds both firmware targets and runs host tests. Check its latest run for CI status.

## Install and inspect without a board

1. Copy `drone_rid.fap` to the Flipper SD card's `apps/GPIO` folder using qFlipper.
2. Launch DroneRID once to create `apps_data/drone_rid`.
3. Copy `fixtures/synthetic-basic-id.rid` there. This is a **synthetic test packet**, not a real aircraft detection.
4. Open DroneRID and press OK on its status page to select the file. The header must say **REPLAY**.
5. Left/right changes pages. Up/down selects a track slot; the first fixture uses slot 1. On the raw page, OK advances through bytes. Return to the status page and press OK to leave replay. Back exits.

A standalone Flipper cannot perform live RID scanning with the currently supported API. Do not interpret an empty list as absence of drones.

## Optional official Wi-Fi Devboard

See [hardware instructions](docs/HARDWARE.md). The companion is receive-only Wi-Fi firmware for the official board, not a modification to the Flipper's official firmware. It replaces the board's debugger firmware. A CRC-checked protocol heartbeat automatically enables live packet processing after the board starts; the status times out after three seconds without a heartbeat.

Accepted packets append to `apps_data/drone_rid/capture.rid`. Logging stops at 4 MiB or on a storage error; move/delete that capture to begin another. Replay has processing-time ages, not historical reception timestamps.

## Status

- Compiled and API-checked FAP against official 1.4.3.
- Compiled companion against ESP-IDF 5.5.0.
- Host parser/framing tests passed under AddressSanitizer and UndefinedBehaviorSanitizer, including 100,000 randomized cases.
- RF reception, physical board attachment, screen layout, storage failure behavior and actual runtime memory remain hardware-unverified.
- Native BLE, Coded PHY, Wi-Fi NAN, 5 GHz and cryptographic authentication verification are unsupported.
- Catalog submission is **not ready**: hardware testing, qFlipper screenshots and final manifest validation remain.

Read [specification](docs/SPECIFICATION.md), [validation record](docs/VALIDATION.md), [protocol](docs/PROTOCOL.md), and [provenance](docs/PROVENANCE.md).

Apache-2.0. OpenDroneID license and notices are preserved in `flipper/vendor`.
