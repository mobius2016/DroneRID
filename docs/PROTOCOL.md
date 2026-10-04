# DroneRID UART and capture protocol v1

UART: 115200 baud, 8N1. Records are ASCII `DR1:` followed by hexadecimal bytes and LF. No carriage return. A record is at most 489 bytes including LF (490-byte C buffer includes NUL). Hex decoding accepts upper/lower case. Length mismatch, invalid hex, overflow, unrecognized types and CRC failure discard the entire line. Recovery occurs after newline. No dynamic allocation in framing or decoding.

CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection, xorout 0. CRC covers the binary body, excludes the ASCII prefix, and is appended low byte first. CRC detects corruption; it does not authenticate an accessory.

Heartbeat body: `[0x01, 0x01]` (type=heartbeat, capabilities=Wi-Fi Beacon). CRC follows. Only this known capability combination is accepted. Emit once each second. A live FAP requires a heartbeat and expires connection status after 3 seconds.

Packet body:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 1 | type 0x02 |
| 1 | 1 | transport: 1 Wi-Fi Beacon, 2 BLE legacy, 3 BLE extended |
| 2 | 6 | source MAC, wire order |
| 8 | 1 | signed RSSI dBm, two's complement |
| 9 | 1 | channel, 0 if not applicable |
| 10 | 2 | payload length, little endian, maximum 228 |
| 12 | length | single 25-byte RID message or 3+25*N message pack |
| 12+length | 2 | CRC |

The live companion currently announces/emits only transport 1. Saved captures may contain any recognized transport. Types 2/3 in a saved file do not imply that the app received BLE itself. No scanning API or PHY support is inferred from file contents.

Wi-Fi Beacon extraction: IEEE 802.11 management header 24 bytes, beacon fixed parameters 12 bytes, then bounded IE iteration. Match vendor IE 221 with OUI FA:0B:BC and vendor type 0D. Skip one message-counter byte and copy the remaining RID data. Only the first matching IE is forwarded. FCS is not part of the input to the portable extractor.

BLE AD extraction: bounded advertising data elements, AD type 0x16 (16-bit Service Data), UUID bytes FA FF (0xFFFA), app code 0D, message counter then RID data. This adapter is available for tests/future backends; there is no native radio scanner attached to it in this release.

No wall-clock/arrival timestamps or message-counter deduplication are encoded in v1. `.rid` is a project format, not PCAP or a standardized ASTM log. Captures are replayed at processing speed. Add a versioned record extension before claiming timing-preserving forensic capture.
