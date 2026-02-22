#ifndef CRYPTO_H
#define CRYPTO_H

#include "packet.h"
#include <stdbool.h>

#define MESH_KEY_LEN 16
#define MAX_NODES    16

void crypto_init(void);
void crypto_set_node_key(uint8_t node_id, const uint8_t key[MESH_KEY_LEN]);

void encrypt_packet(packet_t *pkt);
bool verify_packet(const packet_t *pkt);
bool decrypt_packet(packet_t *pkt);

bool crypto_is_revoked(uint8_t node_id);
void crypto_revoke_node(uint8_t node_id);
void crypto_reinstate_node(uint8_t node_id);

#endif
