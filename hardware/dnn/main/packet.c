#include "packet.h"
#include "simulate.h"
#include "crypto.h"
#include <string.h>
#include "esp_log.h"
#include <stdio.h>
#define TAG "PACKET"

packet_t create_packet(uint8_t sender_id, uint16_t seq_num, uint8_t recipient_id,
                       uint8_t ttl, uint8_t type, const uint8_t *payload, uint8_t payload_len)
{
    packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.sender_id = sender_id;
    pkt.seq_num = seq_num;
    pkt.recipient_id = recipient_id;
    pkt.ttl = ttl;
    pkt.type = type;
    if (payload && payload_len > 0) {
        if (payload_len > sizeof(pkt.payload))
            payload_len = sizeof(pkt.payload);
        memcpy(pkt.payload, payload, payload_len);
    }
    return pkt;
}

void process_packet(packet_t *pkt)
{
    ESP_LOGI(TAG, "from=0x%02x seq=%u type=0x%02x",
             pkt->sender_id, pkt->seq_num, pkt->type);

    switch (pkt->type) {
    case 0x00:
        ESP_LOGI(TAG, "Beacon from node 0x%02x", pkt->sender_id);
        printf("$BEC,%02x\n", pkt->sender_id);
        break;
    case 0x01: {
        position_t pos;
        memcpy(&pos, pkt->payload, sizeof(pos));
        ESP_LOGI(TAG, "Position: lat=%.6f lon=%.6f", pos.lat, pos.lon);
        printf("$POS,%02x,%.6f,%.6f\n", pkt->sender_id, pos.lat, pos.lon);
        break;
    }
    case 0x02:
        ESP_LOGI(TAG, "Message: %.*s", (int)sizeof(pkt->payload), (char *)pkt->payload);
        printf("$MSG,%02x,%.*s\n", pkt->sender_id, (int)sizeof(pkt->payload), (char *)pkt->payload);
        break;
    case 0x03: {
        uint8_t target = pkt->payload[0];
        ESP_LOGW(TAG, "Revoke node 0x%02x (by 0x%02x)", target, pkt->sender_id);
        crypto_revoke_node(target);
        printf("$REV,%02x\n", target);
        break;
    }
    case 0x04: {
        uint8_t target = pkt->payload[0];
        ESP_LOGI(TAG, "Reinstate node 0x%02x (by 0x%02x)", target, pkt->sender_id);
        crypto_reinstate_node(target);
        printf("$REI,%02x\n", target);
        break;
    }
    default:
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, pkt->payload, sizeof(pkt->payload), ESP_LOG_INFO);
        break;
    }
}
