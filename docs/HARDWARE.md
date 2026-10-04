# Hardware validation and installation

## Flipper

The FAP is built for official Flipper firmware 1.4.3, f7, API 87.1. Copy it through qFlipper, or with a USB-connected device run:

```sh
./scripts/build-flipper.sh launch
```

This uploads/runs the app; it does not replace the Flipper firmware or radio stack. No connected Flipper was available for the development tests.

## Official Wi-Fi Devboard

The factory Wi-Fi debugger firmware does not capture RID. Installing the companion replaces that firmware on the accessory. The Flipper continues using official firmware. To restore the accessory later, use the [official devboard update instructions](https://developer.flipper.net/flipperzero/doxygen/dev_board_fw_update.html).

1. Build with `./scripts/build-companion.sh`.
2. Connect the **devboard's** USB-C port to the Mac. If necessary enter its bootloader by holding BOOT, tapping RESET, then releasing BOOT.
3. Identify that board's serial port. Flash only the intended ESP32-S2 device:

```sh
./scripts/build-companion.sh --target upload --upload-port /dev/cu.YOUR_DEVBOARD_PORT
```

The supplied PlatformIO target writes bootloader, partition table and application at the appropriate ESP32-S2 offsets. Do not flash `firmware.bin` alone at address zero. A packaged `companion-merged.bin`, if supplied, is flashed at address 0 using esptool with `--chip esp32s2`, 4 MB flash, DIO, 80 MHz. Do not substitute an ESP32/ESP32-S3 binary.

4. Power off the Flipper, attach the official board with pins correctly aligned, and power on. Start DroneRID. Normal heartbeat recognition is within about one second after the companion finishes booting. UART link: 115200 8N1; ESP GPIO43 TX -> Flipper pin14 RX; ESP GPIO44 RX <- pin13 TX. The factory USB lines are GPIO19/20 and are not reused.
5. Expect `Wi-Fi: companion ready`. This reports the companion's presence, not detection of an aircraft. Use a known Wi-Fi Beacon RID emitter to test packet reception. Channel hopping misses transmissions by design.

No board was flashed or automatically probed with transmit commands during development. No Full-stack installation is necessary for the implemented functionality; installing it does not overcome the missing Flipper scan API.

## Required manual checks

- Stock Light stack: status and replay launch correctly, correct text and coordinates, Back exits cleanly, Bluetooth pairing continues working afterward.
- Full stack: displays Full and still truthfully says no scan API; replay behavior unchanged.
- Factory board: remains “no companion”, no false detection from boot logs.
- Companion: heartbeat arrives, Beacon packets match a reference receiver, RSSI/channel plausible; remove board and confirm timeout, reconnect and confirm recovery.
- Load: sustained packets, UART overflow, malformed records and multiple emitters do not crash or freeze GUI; counters reflect rejected input.
- Track identity: source address reuse clears obsolete location; unknown and stale coordinates are not presented as current.
- SD: absent/full/removed card gives error rather than a crash; accepted frames still display; captured file reopens; 4 MiB cap respected.
- Resources: repeated launch/exit leaves USART and expansion settings restored; busy USART does not get stolen; measure heap/stack high-water marks.
- Catalog: capture genuine screenshots with qFlipper. Never substitute synthetic UI renders for required hardware screenshots.
