#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mirf.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "packet.h"
#include "mesh.h"
#include "crypto.h"
#include "simulate.h"

#define TAG "MAIN"
#define MY_NODE_ID 0x03
#define MESH_CHANNEL 90
#define DEFAULT_TTL 4
#define UART_BUF_SIZE 128

_Static_assert(sizeof(packet_t) == 32, "Packet size is not 32 bytes");

static uint8_t  s_uart_line[UART_BUF_SIZE];
static uint16_t s_uart_pos = 0;

static void handle_command(NRF24_t *dev, char *line, uint16_t *seq)
{
    while (*line == ' ') line++;
    if (line[0] == '\0')
        return;

    if (strncmp(line, "msg ", 4) == 0) {
        char *p = line + 4;
        uint8_t dest = (uint8_t)strtoul(p, &p, 16);
        while (*p == ' ') p++;
        uint8_t len = strlen(p);
        if (len > 18) len = 18;
        packet_t tx = create_packet(MY_NODE_ID, (*seq)++, dest,
                                    DEFAULT_TTL, 0x02,
                                    (const uint8_t *)p, len);
        encrypt_packet(&tx);
        mesh_send(dev, &tx);
        ESP_LOGI(TAG, "Sent msg to 0x%02x: %.*s", dest, len, p);

    } else if (strncmp(line, "pos", 3) == 0) {
        position_t pos = simulate_step();
        packet_t tx = create_packet(MY_NODE_ID, (*seq)++, 0xFF,
                                    DEFAULT_TTL, 0x01,
                                    (const uint8_t *)&pos, sizeof(pos));
        encrypt_packet(&tx);
        mesh_send(dev, &tx);
        ESP_LOGI(TAG, "Sent pos lat=%.6f lon=%.6f", pos.lat, pos.lon);

    } else if (strncmp(line, "revoke ", 7) == 0) {
        uint8_t target = (uint8_t)strtoul(line + 7, NULL, 16);
        crypto_revoke_node(target);
        uint8_t pl[1] = { target };
        packet_t tx = create_packet(MY_NODE_ID, (*seq)++, 0xFF,
                                    DEFAULT_TTL, 0x03, pl, sizeof(pl));
        encrypt_packet(&tx);
        mesh_send(dev, &tx);
        ESP_LOGI(TAG, "Broadcast revoke for node 0x%02x", target);

    } else if (strncmp(line, "reinstate ", 10) == 0) {
        uint8_t target = (uint8_t)strtoul(line + 10, NULL, 16);
        crypto_reinstate_node(target);
        uint8_t pl[1] = { target };
        packet_t tx = create_packet(MY_NODE_ID, (*seq)++, 0xFF,
                                    DEFAULT_TTL, 0x04, pl, sizeof(pl));
        encrypt_packet(&tx);
        mesh_send(dev, &tx);
        ESP_LOGI(TAG, "Broadcast reinstate for node 0x%02x", target);

    } else {
        ESP_LOGW(TAG, "Unknown command. Usage:");
        ESP_LOGW(TAG, "  msg <hex_dest> <text>   - send text message");
        ESP_LOGW(TAG, "  pos                     - send position");
        ESP_LOGW(TAG, "  revoke <hex_id>         - revoke a node");
        ESP_LOGW(TAG, "  reinstate <hex_id>      - reinstate a node");
    }
}

static void poll_uart(NRF24_t *dev, uint16_t *seq)
{
    uint8_t byte;
    while (uart_read_bytes(UART_NUM_0, &byte, 1, 0) > 0) {
        if (byte == '\n' || byte == '\r') {
            if (s_uart_pos > 0) {
                s_uart_line[s_uart_pos] = '\0';
                handle_command(dev, (char *)s_uart_line, seq);
                s_uart_pos = 0;
            }
        } else if (s_uart_pos < UART_BUF_SIZE - 1) {
            s_uart_line[s_uart_pos++] = byte;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting node 0x%02x...", MY_NODE_ID);

    uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    NRF24_t dev;
    Nrf24_init(&dev);
    Nrf24_config(&dev, MESH_CHANNEL, sizeof(packet_t));

    uint8_t addr[5] = {0x07, 0x07, 0x07, 0x07, 0x07};
    Nrf24_setRADDR(&dev, addr);
    Nrf24_setTADDR(&dev, addr);
    Nrf24_printDetails(&dev);

    crypto_init();
    crypto_set_node_key(0x01, (const uint8_t[]){
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    });
    crypto_set_node_key(0x02, (const uint8_t[]){
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F
    });
    crypto_set_node_key(0x03, (const uint8_t[]){
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
    });
    mesh_init();
    simulate_init(43.6532f, -79.3832f);

    while (Nrf24_dataReady(&dev)) {
        uint8_t dump[32];
        Nrf24_getData(&dev, dump);
    }

    ESP_LOGI(TAG, "Listening...");
    ESP_LOGI(TAG, "Commands: msg, pos, revoke, reinstate");

    uint16_t seq = 0;
    TickType_t last_send = 0;

    while (1) {
        while (Nrf24_dataReady(&dev)) {
            packet_t rx_pkt;
            Nrf24_getData(&dev, (uint8_t *)&rx_pkt);
            mesh_handle_packet(&dev, &rx_pkt, MY_NODE_ID);
        }

        poll_uart(&dev, &seq);

        TickType_t now = xTaskGetTickCount();
        if ((now - last_send) >= pdMS_TO_TICKS(5000)) {
            last_send = now;
            packet_t tx = create_packet(MY_NODE_ID, seq++, 0xFF,
                                        DEFAULT_TTL, 0x00, NULL, 0);
            encrypt_packet(&tx);
            mesh_send(&dev, &tx);
            ESP_LOGI(TAG, "Beacon seq=%u", seq - 1);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}