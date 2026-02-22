#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t sender_id;
    uint16_t seq_num;
    uint8_t recipient_id;
    uint8_t ttl;
    uint8_t type;
    uint8_t payload[18];
    uint64_t auth_tag;
} packet_t;
#pragma pack(pop)

packet_t create_packet(uint8_t sender_id, uint16_t seq_num, uint8_t recipient_id,
                       uint8_t ttl, uint8_t type, const uint8_t *payload, uint8_t payload_len);
void process_packet(packet_t *pkt);

#endif