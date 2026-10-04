#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <stdio.h>
#include "core/rid.h"
#define DATA_DIR EXT_PATH("apps_data/drone_rid")
typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* keys;
    FuriStreamBuffer* rx;
    RidStore store;
    RidStream stream;
    bool replay, connected, uart_busy, log_error;
    uint32_t heartbeat, now;
    volatile uint32_t drops;
    FuriHalBtStack stack;
    unsigned selected, page, raw_offset;
    File* log;
} App;
static void input(InputEvent* e, void* ctx) {
    App* a = ctx;
    if(e->type == InputTypeShort) furi_message_queue_put(a->keys, e, 0);
}
static void rx_cb(FuriHalSerialHandle* h, FuriHalSerialRxEvent e, void* ctx) {
    App* a = ctx;
    if(e & FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(h);
        if(furi_stream_buffer_send(a->rx, &b, 1, 0) != 1) a->drops++;
    }
}
static void safe_text(char* dst, const char* src, size_t n) {
    for(size_t i = 0; i < n; i++) {
        unsigned char ch = src[i];
        dst[i] = (ch >= 32 && ch <= 126) ? ch : '?';
        if(!ch) {
            dst[i] = 0;
            return;
        }
    }
    dst[n] = 0;
}
static void draw(Canvas* c, void* ctx) {
    App* a = ctx;
    char b[64];
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 0, 8, a->replay ? "DroneRID [REPLAY]" : "DroneRID [LIVE]");
    if(a->page == 0) {
        snprintf(
            b,
            sizeof(b),
            "BLE %s: no scan API",
            a->stack == FuriHalBtStackFull  ? "Full" :
            a->stack == FuriHalBtStackLight ? "Light" :
                                              "unknown");
        canvas_draw_str(c, 0, 19, b);
        canvas_draw_str(
            c,
            0,
            29,
            a->uart_busy ? "UART busy" :
            a->connected ? "Wi-Fi: companion ready" :
                           "Wi-Fi: no companion");
        snprintf(
            b,
            sizeof(b),
            "Packets %lu / bad %lu",
            (unsigned long)a->store.accepted,
            (unsigned long)(a->store.rejected + a->stream.bad));
        canvas_draw_str(c, 0, 39, b);
        canvas_draw_str(
            c,
            0,
            49,
            a->log_error ? "Log full / write failed" :
            a->replay    ? "OK: return to live" :
            a->log       ? "OK: replay (log saved)" :
                           "OK: open .rid capture");
        canvas_draw_str(c, 0, 62, "<> pages   Back: exit");
    } else {
        RidTrack* t = &a->store.tracks[a->selected];
        if(!t->used) {
            canvas_draw_str(c, 0, 25, "No packets in this slot");
            canvas_draw_str(c, 0, 39, "Up/Down: select aircraft");
        } else {
            snprintf(
                b,
                sizeof(b),
                "#%u %ddBm age %lus",
                a->selected + 1,
                t->last.rssi,
                (unsigned long)((a->now - t->seen_ms) / 1000));
            canvas_draw_str(c, 0, 19, b);
            if(a->page == 1) {
                if(t->uas.BasicIDValid[0])
                    safe_text(b, t->uas.BasicID[0].UASID, 20);
                else
                    snprintf(b, sizeof(b), "ID: not received");
                canvas_draw_str(c, 0, 29, b);
                bool fresh = (t->field_seen & 2) && (uint32_t)(a->now - t->field_ms[1]) < 15000;
                if(t->uas.LocationValid && fresh &&
                   (t->uas.Location.Latitude != 0 || t->uas.Location.Longitude != 0)) {
                    snprintf(b, sizeof(b), "Lat %.5f", t->uas.Location.Latitude);
                    canvas_draw_str(c, 0, 39, b);
                    snprintf(b, sizeof(b), "Lon %.5f", t->uas.Location.Longitude);
                    canvas_draw_str(c, 0, 49, b);
                } else
                    canvas_draw_str(c, 0, 42, "Position unknown / stale");
            } else if(a->page == 2) {
                bool fresh = (t->field_seen & 16) && (uint32_t)(a->now - t->field_ms[4]) < 15000;
                if(t->uas.SystemValid && fresh &&
                   (t->uas.System.OperatorLatitude != 0 || t->uas.System.OperatorLongitude != 0)) {
                    snprintf(b, sizeof(b), "Op lat %.5f", t->uas.System.OperatorLatitude);
                    canvas_draw_str(c, 0, 31, b);
                    snprintf(b, sizeof(b), "Op lon %.5f", t->uas.System.OperatorLongitude);
                    canvas_draw_str(c, 0, 42, b);
                } else
                    canvas_draw_str(c, 0, 35, "Operator unknown / stale");
                snprintf(b, sizeof(b), "Transport %u ch %u", t->last.transport, t->last.channel);
                canvas_draw_str(c, 0, 52, b);
            } else if(a->page == 3) {
                bool fresh = (t->field_seen & 2) && (uint32_t)(a->now - t->field_ms[1]) < 15000;
                if(t->uas.LocationValid && fresh) {
                    if(t->uas.Location.AltitudeGeo != -1000)
                        snprintf(b, sizeof(b), "HAE %.1fm", (double)t->uas.Location.AltitudeGeo);
                    else
                        snprintf(b, sizeof(b), "HAE unknown");
                    canvas_draw_str(c, 0, 31, b);
                    if(t->uas.Location.SpeedHorizontal != 255)
                        snprintf(
                            b, sizeof(b), "Speed %.1fm/s", (double)t->uas.Location.SpeedHorizontal);
                    else
                        snprintf(b, sizeof(b), "Speed unknown");
                    canvas_draw_str(c, 0, 42, b);
                } else
                    canvas_draw_str(c, 0, 35, "Telemetry stale / unknown");
            } else if(a->page == 5) {
                snprintf(b, sizeof(b), "UART drops %lu", (unsigned long)a->drops);
                canvas_draw_str(c, 0, 31, b);
                snprintf(b, sizeof(b), "Evictions %lu", (unsigned long)a->store.evicted);
                canvas_draw_str(c, 0, 42, b);
                canvas_draw_str(c, 0, 52, "Auth not verified");
            } else {
                for(unsigned row = 0; row < 3; row++) {
                    size_t n = 0;
                    b[0] = 0;
                    for(unsigned k = 0; k < 8 && a->raw_offset + row * 8 + k < t->last.length; k++)
                        n += snprintf(
                            b + n,
                            sizeof(b) - n,
                            "%02X",
                            t->last.payload[a->raw_offset + row * 8 + k]);
                    canvas_draw_str(c, 0, 30 + row * 10, b);
                }
            }
        }
        canvas_draw_str(c, 0, 63, a->page == 4 ? "OK: more hex   <> page" : "^v target   <> page");
    }
    furi_mutex_release(a->mutex);
}
static void consume(App* a, uint8_t byte, bool live) {
    RidPacket p;
    int kind = rid_stream_byte(&a->stream, byte, &p);
    if(kind == 2 && live) {
        a->heartbeat = a->now;
        a->connected = true;
    }
    if(kind == 1 && (!live || (a->connected && p.transport == 1))) {
        if(rid_ingest(&a->store, &p, a->now) && live && a->log && !a->log_error) {
            char line[RID_MAX_LINE];
            size_t n = rid_wire_packet(&p, line, sizeof(line));
            if(storage_file_size(a->log) + n > 4 * 1024 * 1024 ||
               storage_file_write(a->log, line, n) != n)
                a->log_error = true;
        }
    }
}
static File* open_replay(App* a, Storage* storage, ViewPort* vp) {
    view_port_enabled_set(vp, false);
    DialogsApp* d = furi_record_open(RECORD_DIALOGS);
    FuriString* path = furi_string_alloc_set(DATA_DIR);
    DialogsFileBrowserOptions opt;
    dialog_file_browser_set_basic_options(&opt, ".rid", NULL);
    File* f = NULL;
    if(dialog_file_browser_show(d, path, path, &opt)) {
        f = storage_file_alloc(storage);
        if(!storage_file_open(f, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(f);
            f = NULL;
        }
    }
    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    if(f) {
        memset(&a->store, 0, sizeof(a->store));
        memset(&a->stream, 0, sizeof(a->stream));
        a->replay = true;
        a->connected = false;
    }
    furi_mutex_release(a->mutex);
    view_port_enabled_set(vp, true);
    return f;
}
int32_t drone_rid_app(void* ctx) {
    UNUSED(ctx);
    App* a = calloc(1, sizeof(App));
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->keys = furi_message_queue_alloc(8, sizeof(InputEvent));
    a->rx = furi_stream_buffer_alloc(2048, 1);
    a->stack = furi_hal_bt_get_radio_stack();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, DATA_DIR);
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    FuriHalSerialHandle* serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    a->uart_busy = !serial;
    if(serial) {
        furi_hal_serial_init(serial, 115200);
        furi_hal_serial_async_rx_start(serial, rx_cb, a, false);
    }
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw, a);
    view_port_input_callback_set(vp, input, a);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    bool running = true;
    File* replay = NULL;
    uint32_t last_sync = 0;
    while(running) {
        InputEvent e;
        bool key = furi_message_queue_get(a->keys, &e, 20) == FuriStatusOk;
        if(key && e.key == InputKeyOk && a->page == 0) {
            if(!a->replay) {
                furi_mutex_acquire(a->mutex, FuriWaitForever);
                if(a->log) {
                    storage_file_close(a->log);
                    storage_file_free(a->log);
                    a->log = NULL;
                }
                furi_mutex_release(a->mutex);
                replay = open_replay(a, storage, vp);
            } else {
                if(replay) {
                    storage_file_close(replay);
                    storage_file_free(replay);
                    replay = NULL;
                }
                furi_mutex_acquire(a->mutex, FuriWaitForever);
                a->replay = false;
                a->connected = false;
                memset(&a->store, 0, sizeof(a->store));
                memset(&a->stream, 0, sizeof(a->stream));
                furi_mutex_release(a->mutex);
            }
        }
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        a->now = (uint32_t)((uint64_t)furi_get_tick() * 1000 / furi_kernel_get_tick_frequency());
        if(key) {
            if(e.key == InputKeyBack) running = false;
            if(e.key == InputKeyRight) a->page = (a->page + 1) % 6;
            if(e.key == InputKeyLeft) a->page = (a->page + 5) % 6;
            if(e.key == InputKeyUp) a->selected = (a->selected + RID_TRACKS - 1) % RID_TRACKS;
            if(e.key == InputKeyDown) a->selected = (a->selected + 1) % RID_TRACKS;
            if(e.key == InputKeyOk && a->page == 4) {
                a->raw_offset += 24;
                if(a->raw_offset >= a->store.tracks[a->selected].last.length) a->raw_offset = 0;
            } else if(e.key == InputKeyOk && a->page)
                a->page = 0;
            if(e.key == InputKeyUp || e.key == InputKeyDown) a->raw_offset = 0;
        }
        if(a->connected && (uint32_t)(a->now - a->heartbeat) > 3000) a->connected = false;
        uint8_t bytes[256];
        size_t n = furi_stream_buffer_receive(a->rx, bytes, sizeof(bytes), 0);
        for(unsigned batch = 0; n && batch < 8; batch++) {
            if(!a->replay)
                for(size_t i = 0; i < n; i++)
                    consume(a, bytes[i], true);
            if(batch < 7) n = furi_stream_buffer_receive(a->rx, bytes, sizeof(bytes), 0);
        }
        if(replay) {
            n = storage_file_read(replay, bytes, sizeof(bytes));
            for(size_t i = 0; i < n; i++)
                consume(a, bytes[i], false);
            if(!n) {
                storage_file_close(replay);
                storage_file_free(replay);
                replay = NULL;
            }
        }
        if(a->connected && !a->log && !a->replay && !a->log_error) {
            a->log = storage_file_alloc(storage);
            if(!storage_file_open(a->log, DATA_DIR "/capture.rid", FSAM_WRITE, FSOM_OPEN_APPEND)) {
                storage_file_free(a->log);
                a->log = NULL;
                a->log_error = true;
            }
        }
        if(a->log && (uint32_t)(a->now - last_sync) > 2000) {
            if(!storage_file_sync(a->log)) a->log_error = true;
            last_sync = a->now;
        }
        furi_mutex_release(a->mutex);
        view_port_update(vp);
    }
    if(serial) {
        furi_hal_serial_async_rx_stop(serial);
        furi_hal_serial_deinit(serial);
        furi_hal_serial_control_release(serial);
    }
    expansion_enable(expansion);
    view_port_enabled_set(vp, false);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    if(replay) {
        storage_file_close(replay);
        storage_file_free(replay);
    }
    if(a->log) {
        storage_file_close(a->log);
        storage_file_free(a->log);
    }
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_EXPANSION);
    furi_stream_buffer_free(a->rx);
    furi_message_queue_free(a->keys);
    furi_mutex_free(a->mutex);
    free(a);
    return 0;
}
