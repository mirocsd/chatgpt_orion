#ifndef MESH_H
#define MESH_H

#include "packet.h"
#include "mirf.h"

void mesh_init(void);
void mesh_handle_packet(NRF24_t *dev, packet_t *pkt, uint8_t my_id);
void mesh_send(NRF24_t *dev, packet_t *packet);

#endif
