# Validation record — development build 0.1

Executed on the user's Mac, October 3, 2026.

| Check | Result |
|---|---|
| Official release SDK build | PASS: 1.4.3, f7, API 87.1 |
| FAP exported-symbol check | PASS (uFBT APPCHK) |
| Companion build | PASS: ESP-IDF 5.5.0, ESP32-S2, 4 MB |
| Host C tests | PASS with AddressSanitizer + UndefinedBehaviorSanitizer |
| Random parser/framing input | PASS: 100,000 deterministic randomized iterations |
| CRC/recovery/truncation | PASS, malformed line and packet rejection |
| Decoding | PASS: fixed Basic ID vector plus reference-encoded Location, System, Self ID, Operator ID, Auth page 15 |
| State | PASS: per-field freshness timestamps, identity reuse reset, bounds, transactionality, transport separation, capacity eviction and timestamp wrap |
| Wi-Fi/BLE payload adapters | PASS against constructed framing vectors and truncations |
| Radio capture | NOT RUN: no device connected |
| FAP runtime/UI/SD/heap | NOT RUN: no device connected |
| Full-stack device | NOT RUN; no scan API implemented |
| Catalog validation | NOT RUN: no published repository, real screenshots or completed manifest |
| Remote CI | NOT RUN |

These are parser robustness and build results, not an RF validation or ASTM conformance certificate. Known boundaries and release gates are in SPECIFICATION.md and HARDWARE.md.

The first companion configuration attempt hit a sandbox restriction when an unused dependency manager enumerated host processes. The project has no managed external ESP-IDF components; `IDF_COMPONENT_MANAGER=0` selects the supported built-in-components-only configuration. No sandbox policy was changed, and the full companion compilation then passed.
