#include "packet.h"
#include <string.h>
#include "esp_log.h"

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
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, pkt->payload, sizeof(pkt->payload), ESP_LOG_INFO);
}
