#include "rid.h"
#include <string.h>
static uint16_t crc16(const uint8_t* p, size_t n) {
    uint16_t c = 0xffff;
    while(n--) {
        c ^= (uint16_t)*p++ << 8;
        for(int i = 0; i < 8; i++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}
static int hex(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
static size_t wire(uint8_t* b, size_t n, char* out, size_t cap) {
    const char* h = "0123456789ABCDEF";
    if(cap < 4 + 2 * (n + 2) + 2) return 0;
    uint16_t c = crc16(b, n);
    b[n++] = c & 255;
    b[n++] = c >> 8;
    memcpy(out, "DR1:", 4);
    for(size_t i = 0; i < n; i++) {
        out[4 + i * 2] = h[b[i] >> 4];
        out[5 + i * 2] = h[b[i] & 15];
    }
    out[4 + 2 * n] = '\n';
    out[5 + 2 * n] = 0;
    return 5 + 2 * n;
}
size_t rid_wire_heartbeat(char* out, size_t cap) {
    uint8_t b[4] = {1, 1};
    return wire(b, 2, out, cap);
}
size_t rid_wire_packet(const RidPacket* p, char* out, size_t cap) {
    if(p->length > RID_MAX_PAYLOAD) return 0;
    uint8_t b[RID_MAX_PAYLOAD + 14] = {2, p->transport};
    memcpy(b + 2, p->mac, 6);
    b[8] = (uint8_t)p->rssi;
    b[9] = p->channel;
    b[10] = p->length & 255;
    b[11] = p->length >> 8;
    memcpy(b + 12, p->payload, p->length);
    return wire(b, 12 + p->length, out, cap);
}
int rid_stream_byte(RidStream* s, uint8_t ch, RidPacket* p) {
    if(ch != '\n') {
        if(s->used == sizeof(s->line)) s->dropping = true;
        if(!s->dropping) s->line[s->used++] = (char)ch;
        return 0;
    }
    size_t n = s->used;
    s->used = 0;
    if(s->dropping) {
        s->dropping = false;
        s->bad++;
        return 0;
    }
    if(n < 12 || memcmp(s->line, "DR1:", 4) || (n - 4) % 2) {
        s->bad++;
        return 0;
    }
    uint8_t b[RID_MAX_PAYLOAD + 14];
    size_t len = (n - 4) / 2;
    if(len > sizeof(b)) {
        s->bad++;
        return 0;
    }
    for(size_t i = 0; i < len; i++) {
        int a = hex(s->line[4 + i * 2]), z = hex(s->line[5 + i * 2]);
        if(a < 0 || z < 0) {
            s->bad++;
            return 0;
        }
        b[i] = (uint8_t)((a << 4) | z);
    }
    if(crc16(b, len - 2) != (uint16_t)(b[len - 2] | (b[len - 1] << 8))) {
        s->bad++;
        return 0;
    }
    if(len == 4 && b[0] == 1 && b[1] == 1) return 2;
    if(len < 14 || b[0] != 2 || b[1] < 1 || b[1] > 3) {
        s->bad++;
        return 0;
    }
    size_t plen = b[10] | (b[11] << 8);
    if(plen > RID_MAX_PAYLOAD || plen + 14 != len) {
        s->bad++;
        return 0;
    }
    memset(p, 0, sizeof(*p));
    p->transport = b[1];
    memcpy(p->mac, b + 2, 6);
    p->rssi = (int8_t)b[8];
    p->channel = b[9];
    p->length = plen;
    memcpy(p->payload, b + 12, plen);
    return 1;
}
static bool valid_message(const uint8_t* p) {
    /* Versions 0/1/2 known to the reference library. Never accept a nested pack. */
    return (p[0] & 15) <= 2 && (p[0] >> 4) <= 5;
}
bool rid_ingest(RidStore* s, const RidPacket* p, uint32_t now) {
    if(p->transport < 1 || p->transport > 3 || p->length > RID_MAX_PAYLOAD) goto bad;
    size_t count = 1, offset = 0;
    if(p->length >= 3 && p->payload[0] >> 4 == 15) {
        count = p->payload[2];
        offset = 3;
        if((p->payload[0] & 15) > 2 || p->payload[1] != 25 || !count ||
           count > ODID_PACK_MAX_MESSAGES || p->length != 3 + 25 * count)
            goto bad;
    } else if(p->length != 25)
        goto bad;
    unsigned types[6] = {0};
    for(size_t i = 0; i < count; i++) {
        const uint8_t* msg = p->payload + offset + 25 * i;
        if(!valid_message(msg)) goto bad;
        types[msg[0] >> 4]++;
    }
    if(types[0] > ODID_BASIC_ID_MAX_MESSAGES || types[1] > 1 || types[2] > ODID_AUTH_MAX_PAGES ||
       types[3] > 1 || types[4] > 1 || types[5] > 1)
        goto bad;
    int slot = -1;
    for(int i = 0; i < RID_TRACKS; i++)
        if(s->tracks[i].used && s->tracks[i].last.transport == p->transport &&
           !memcmp(s->tracks[i].last.mac, p->mac, 6)) {
            slot = i;
            break;
        }
    bool existing = slot >= 0;
    if(slot < 0)
        for(int i = 0; i < RID_TRACKS; i++)
            if(!s->tracks[i].used) {
                slot = i;
                break;
            }
    if(slot < 0) {
        slot = 0;
        for(int i = 1; i < RID_TRACKS; i++)
            if((uint32_t)(now - s->tracks[i].seen_ms) > (uint32_t)(now - s->tracks[slot].seen_ms))
                slot = i;
    }
    /* Transactional decoding: malformed packs cannot partly update a track. */
    ODID_UAS_Data candidate;
    if(existing)
        candidate = s->tracks[slot].uas;
    else
        odid_initUasData(&candidate);
    for(size_t i = 0; i < count; i++)
        if(decodeOpenDroneID(&candidate, p->payload + offset + 25 * i) == ODID_MESSAGETYPE_INVALID)
            goto bad;
    /* Coordinates outside the geographic domain are malformed, not positions. */
    if(candidate.LocationValid &&
       (candidate.Location.Latitude < -90 || candidate.Location.Latitude > 90 ||
        candidate.Location.Longitude < -180 || candidate.Location.Longitude > 180))
        goto bad;
    if(candidate.SystemValid &&
       (candidate.System.OperatorLatitude < -90 || candidate.System.OperatorLatitude > 90 ||
        candidate.System.OperatorLongitude < -180 || candidate.System.OperatorLongitude > 180))
        goto bad;
    RidTrack* t = &s->tracks[slot];
    if(!existing) {
        if(t->used) s->evicted++;
        memset(t, 0, sizeof(*t));
    }
    /* Do not retain a previous aircraft's position when its identity changes
     * on a reused transport address. Re-decode only this packet on a fresh UAS. */
    if(existing) {
        bool changed = false;
        for(unsigned k = 0; k < ODID_BASIC_ID_MAX_MESSAGES; k++) {
            if(t->uas.BasicIDValid[k] && candidate.BasicIDValid[k] &&
               t->uas.BasicID[k].IDType == candidate.BasicID[k].IDType &&
               memcmp(t->uas.BasicID[k].UASID, candidate.BasicID[k].UASID, ODID_ID_SIZE))
                changed = true;
        }
        if(changed) {
            odid_initUasData(&candidate);
            for(size_t i = 0; i < count; i++)
                decodeOpenDroneID(&candidate, p->payload + offset + 25 * i);
            memset(t, 0, sizeof(*t));
        }
    }
    t->uas = candidate;
    t->last = *p;
    t->used = true;
    t->seen_ms = now;
    t->messages++;
    for(size_t i = 0; i < count; i++) {
        unsigned type = p->payload[offset + 25 * i] >> 4;
        t->field_ms[type] = now;
        t->field_seen |= 1u << type;
    }
    s->accepted++;
    return true;
bad:
    s->rejected++;
    return false;
}
bool rid_beacon(const uint8_t* f, size_t n, int8_t rssi, uint8_t ch, RidPacket* out) {
    if(n < 36 || (f[0] & 0xfc) != 0x80 || (f[1] & 0x40)) return false;
    for(size_t pos = 36; pos + 2 <= n;) {
        size_t len = f[pos + 1];
        if(len > n - pos - 2) return false;
        if(f[pos] == 221 && len >= 8 && !memcmp(f + pos + 2, "\xFA\x0B\xBC\x0D", 4)) {
            size_t plen = len - 5;
            if(plen > RID_MAX_PAYLOAD) return false;
            memset(out, 0, sizeof(*out));
            out->transport = 1;
            memcpy(out->mac, f + 10, 6);
            out->rssi = rssi;
            out->channel = ch;
            out->length = plen;
            memcpy(out->payload, f + pos + 7, plen);
            return true;
        }
        pos += 2 + len;
    }
    return false;
}
bool rid_ble_ad(const uint8_t* a, size_t n, const uint8_t mac[6], int8_t rssi, RidPacket* out) {
    for(size_t pos = 0; pos < n;) {
        size_t len = a[pos];
        if(!len) break;
        if(len > n - pos - 1) return false;
        if(len >= 5 && a[pos + 1] == 0x16 && a[pos + 2] == 0xfa && a[pos + 3] == 0xff &&
           a[pos + 4] == 0x0d) {
            size_t plen = len - 5;
            if(plen > RID_MAX_PAYLOAD) return false;
            memset(out, 0, sizeof(*out));
            out->transport = 2;
            memcpy(out->mac, mac, 6);
            out->rssi = rssi;
            out->length = plen;
            memcpy(out->payload, a + pos + 6, plen);
            return true;
        }
        pos += len + 1;
    }
    return false;
}
