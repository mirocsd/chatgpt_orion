#include "mesh.h"
#include "packet.h"
#include "cache.h"
#include "crypto.h"
#include "mirf.h"
#include "esp_log.h"
#include "esp_random.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static seen_cache_t g_cache;

void mesh_init(void)
{
    seen_cache_init(&g_cache);
}

void mesh_handle_packet(NRF24_t *dev, packet_t *pkt, uint8_t my_id) 
{
    if (pkt->sender_id == my_id || pkt->sender_id == 0x00)
        return;

    if (seen_cache_contains(&g_cache, pkt->sender_id, pkt->seq_num))
        return;

    seen_cache_add(&g_cache, pkt->sender_id, pkt->seq_num);

    if (!verify_packet(pkt)) {
        ESP_LOGW("MESH", "Auth failed from 0x%02x seq=%u, dropping",
                 pkt->sender_id, pkt->seq_num);
        return;
    }

    if (pkt->ttl == 0)
        return;

    if (pkt->recipient_id != my_id) {
        pkt->ttl--;
        vTaskDelay(pdMS_TO_TICKS(esp_random() % 45 + 5));
        mesh_send(dev, pkt);
    }

    if (pkt->recipient_id == my_id || pkt->recipient_id == 0xFF) {
        decrypt_packet(pkt);
        process_packet(pkt);
    }
}

void mesh_send(NRF24_t *dev, packet_t *packet)
{
    Nrf24_send(dev, (uint8_t *)packet);
    if (!Nrf24_isSend(dev, 1000))
        ESP_LOGW("MESH", "Send FAILED!");

    Nrf24_powerUpRx(dev);
}