#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "rid.h"
/* Official Wi-Fi Devboard V1: ESP TXD0 GPIO43 -> Flipper RX pin14;
 * ESP RXD0 GPIO44 <- Flipper TX pin13. USB uses GPIO19/20. */
static QueueHandle_t packets;
static uint32_t dropped;
static void receive(void* buf, wifi_promiscuous_pkt_type_t type) {
    if(type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* p = buf;
    if(p->rx_ctrl.rx_state || p->rx_ctrl.sig_len < 40) return;
    RidPacket packet;
    /* ESP-IDF sig_len includes the 4-byte FCS. */
    if(rid_beacon(
           p->payload, p->rx_ctrl.sig_len - 4, p->rx_ctrl.rssi, p->rx_ctrl.channel, &packet)) {
        if(xQueueSend(packets, &packet, 0) != pdTRUE) dropped++;
    }
}
void app_main(void) {
    /* WIFI_STORAGE_RAM avoids altering saved settings. No NVS partition erase. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 1024, 0, NULL, 0));
    packets = xQueueCreate(16, sizeof(RidPacket));
    configASSERT(packets);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    wifi_country_t country = {
        .cc = "01", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_MANUAL};
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(receive));
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    /* Channel 6 receives half of all dwell time; remaining 1..11 rotate.
     * No AP association, active scanning or packet injection. */
    uint8_t channel = 1;
    bool primary = true;
    int64_t hop = esp_timer_get_time(), heartbeat = 0;
    while(1) {
        char line[RID_MAX_LINE];
        int64_t now = esp_timer_get_time();
        if(now - heartbeat >= 1000000) {
            size_t n = rid_wire_heartbeat(line, sizeof(line));
            uart_write_bytes(UART_NUM_0, line, n);
            heartbeat = now;
        }
        RidPacket packet;
        if(xQueueReceive(packets, &packet, pdMS_TO_TICKS(20)) == pdTRUE) {
            size_t n = rid_wire_packet(&packet, line, sizeof(line));
            uart_write_bytes(UART_NUM_0, line, n);
        }
        if(now - hop >= 250000) {
            uint8_t next = primary ? channel : 6;
            ESP_ERROR_CHECK(esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE));
            if(primary) {
                channel = channel % 11 + 1;
                if(channel == 6) channel = 7;
            }
            primary = !primary;
            hop = now;
        }
    }
}
