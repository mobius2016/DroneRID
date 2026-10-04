#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../vendor/opendroneid.h"
#define RID_MAX_PAYLOAD 228
#define RID_MAX_LINE    490
#define RID_TRACKS      8
/* Transport 1 = Wi-Fi beacon, 2 = BLE legacy, 3 = BLE extended.
 * Replay is a session mode, never a radio transport. */
typedef struct {
    uint8_t transport, mac[6], channel;
    int8_t rssi;
    uint16_t length;
    uint8_t payload[RID_MAX_PAYLOAD];
} RidPacket;
typedef struct {
    bool used;
    RidPacket last;
    ODID_UAS_Data uas;
    uint32_t seen_ms, messages, field_ms[6];
    uint8_t field_seen;
} RidTrack;
typedef struct {
    RidTrack tracks[RID_TRACKS];
    uint32_t accepted, rejected, evicted;
} RidStore;
typedef struct {
    char line[RID_MAX_LINE];
    size_t used;
    bool dropping;
    uint32_t bad;
} RidStream;
/* Returns 1 for data, 2 for a versioned beacon-capable companion heartbeat. */
int rid_stream_byte(RidStream*, uint8_t, RidPacket*);
size_t rid_wire_packet(const RidPacket*, char*, size_t);
size_t rid_wire_heartbeat(char*, size_t);
bool rid_ingest(RidStore*, const RidPacket*, uint32_t now_ms);
bool rid_beacon(const uint8_t*, size_t, int8_t, uint8_t, RidPacket*);
bool rid_ble_ad(const uint8_t*, size_t, const uint8_t mac[6], int8_t, RidPacket*);
