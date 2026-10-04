# Source and build provenance

Inspected October 3, 2026 (America/New_York).

- Official release SDK: Flipper 1.4.3, hardware f7, API 87.1. The release feed selected this version. Snapshot: `evidence/flipper-sdk.json`.
- Official development firmware inspected: `flipperdevices/flipperzero-firmware` commit `2bbf7a54bae7316da858ab62f22ed8db0b4c661b`.
- OpenDroneID: `opendroneid/opendroneid-core-c` commit `6484f26545d4f012682524e2d843fab0fbdc0b34`.
  Vendored `libopendroneid/opendroneid.c`, `.h` and Apache-2.0 license. Two diagnostic printf arguments are explicitly cast to unsigned int for ARM/newlib format compatibility. Decoding algorithms are unchanged.
- Companion: PlatformIO espressif32 6.12.0, Espressif ESP-IDF 5.5.0; ESP32-S2, 4 MB flash, no PSRAM dependency. The Saola build target supplies SoC parameters only; UART pins are explicitly assigned for the official Flipper board.
- uFBT 0.2.6, PlatformIO 6.2.0, Python 3.14.8 in the user-created `venv` on this Mac. uFBT uses its own bundled Python/toolchain; PlatformIO creates an additional isolated ESP-IDF venv under `.tools/platformio/penv`.

## Official references

- [Flipper API exports](https://github.com/flipperdevices/flipperzero-firmware/blob/2bbf7a54bae7316da858ab62f22ed8db0b4c661b/targets/f7/api_symbols.csv)
- [BT HAL](https://github.com/flipperdevices/flipperzero-firmware/blob/2bbf7a54bae7316da858ab62f22ed8db0b4c661b/targets/furi_hal_include/furi_hal_bt.h)
- [Full-stack update documentation](https://github.com/flipperdevices/flipperzero-firmware/blob/2bbf7a54bae7316da858ab62f22ed8db0b4c661b/documentation/OTA.md)
- [OpenDroneID decoder](https://github.com/opendroneid/opendroneid-core-c/tree/6484f26545d4f012682524e2d843fab0fbdc0b34)
- [Wi-Fi framing reference](https://github.com/opendroneid/opendroneid-core-c/blob/6484f26545d4f012682524e2d843fab0fbdc0b34/libopendroneid/wifi.c)
- [Official devboard schematic](https://cdn.flipperzero.one/Flipper_Zero_WI-FI_Module_V1_Schematic.PDF), sheets 2/4: GPIO43 TX to Flipper pin14 RX; GPIO44 RX from Flipper pin13 TX.
- [Espressif Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s2/api-reference/network/esp_wifi.html)
- [Catalog requirements](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md)

The official release and development export lists provide stack identification, advertising, profile APIs and event handlers, but no supported passive scanner start/stop API. `hci_send_req` is explicitly unexported. An exported event handler alone does not initiate scanning. No private symbol resolution, raw HCI injection, firmware patching, or unofficial firmware is used.
