#include <stdio.h>
#include "mirf.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "packet.h"
#include "mesh.h"

#define TAG "MAIN"
#define MY_NODE_ID 0x01
#define MESH_CHANNEL 90
#define DEFAULT_TTL 4

_Static_assert(sizeof(packet_t) == 32, "Packet size is not 32 bytes");

void app_main(void)
{
    ESP_LOGI(TAG, "Starting node 0x%02x...", MY_NODE_ID);

    NRF24_t dev;
    Nrf24_init(&dev);
    Nrf24_config(&dev, MESH_CHANNEL, sizeof(packet_t));

    uint8_t addr[5] = {0x07, 0x07, 0x07, 0x07, 0x07};
    Nrf24_setRADDR(&dev, addr);
    Nrf24_setTADDR(&dev, addr);
    Nrf24_printDetails(&dev);

    mesh_init();

    while (Nrf24_dataReady(&dev)) {
        uint8_t dump[32];
        Nrf24_getData(&dev, dump);
    }

    ESP_LOGI(TAG, "Listening...");

    uint16_t seq = 0;
    TickType_t last_send = 0;

    while (1) {
        while (Nrf24_dataReady(&dev)) {
            packet_t rx_pkt;
            Nrf24_getData(&dev, (uint8_t *)&rx_pkt);
            mesh_handle_packet(&dev, &rx_pkt, MY_NODE_ID);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_send) >= pdMS_TO_TICKS(5000)) {
            last_send = now;
            uint8_t data[] = "hello";
            packet_t tx = create_packet(MY_NODE_ID, seq++, 0xFF,
                                        DEFAULT_TTL, 0x01, data, sizeof(data));
            mesh_send(&dev, &tx);
            ESP_LOGI(TAG, "Broadcast seq=%u", seq - 1);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}