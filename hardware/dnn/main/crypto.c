#include "crypto.h"
#include <string.h>
#include "mbedtls/gcm.h"
#include "esp_log.h"

#define TAG "CRYPTO"
#define GCM_FULL_TAG_LEN  16
#define TRUNCATED_TAG_LEN 8
#define NONCE_LEN         12
#define AAD_LEN           3  /* sender_id (1) + seq_num (2) */

static uint8_t g_keys[MAX_NODES][MESH_KEY_LEN];
static bool    g_key_valid[MAX_NODES];
static bool    g_revoked[MAX_NODES];
static mbedtls_gcm_context g_gcm;

void crypto_init(void)
{
    memset(g_keys, 0, sizeof(g_keys));
    memset(g_key_valid, 0, sizeof(g_key_valid));
    memset(g_revoked, 0, sizeof(g_revoked));
    mbedtls_gcm_init(&g_gcm);
}

void crypto_set_node_key(uint8_t node_id, const uint8_t key[MESH_KEY_LEN])
{
    if (node_id >= MAX_NODES) {
        ESP_LOGE(TAG, "node_id 0x%02x exceeds MAX_NODES", node_id);
        return;
    }
    memcpy(g_keys[node_id], key, MESH_KEY_LEN);
    g_key_valid[node_id] = true;
    ESP_LOGI(TAG, "Key set for node 0x%02x", node_id);
}

static bool load_key_for_node(uint8_t node_id)
{
    if (node_id >= MAX_NODES || !g_key_valid[node_id])
        return false;
    if (g_revoked[node_id])
        return false;
    int ret = mbedtls_gcm_setkey(&g_gcm, MBEDTLS_CIPHER_ID_AES,
                                 g_keys[node_id], MESH_KEY_LEN * 8);
    return ret == 0;
}

static void build_nonce(const packet_t *pkt, uint8_t nonce[NONCE_LEN])
{
    memset(nonce, 0, NONCE_LEN);
    memcpy(nonce, pkt, AAD_LEN);
}

void encrypt_packet(packet_t *pkt)
{
    if (!load_key_for_node(pkt->sender_id)) {
        ESP_LOGE(TAG, "No key for sender 0x%02x", pkt->sender_id);
        return;
    }

    uint8_t nonce[NONCE_LEN];
    build_nonce(pkt, nonce);

    uint8_t full_tag[GCM_FULL_TAG_LEN];

    mbedtls_gcm_crypt_and_tag(&g_gcm, MBEDTLS_GCM_ENCRYPT,
        sizeof(pkt->payload),
        nonce, NONCE_LEN,
        (const uint8_t *)pkt, AAD_LEN,
        pkt->payload, pkt->payload,
        GCM_FULL_TAG_LEN, full_tag);

    memcpy(&pkt->auth_tag, full_tag, TRUNCATED_TAG_LEN);
}

bool verify_packet(const packet_t *pkt)
{
    if (!load_key_for_node(pkt->sender_id))
        return false;

    uint8_t nonce[NONCE_LEN];
    build_nonce(pkt, nonce);

    uint8_t computed_tag[GCM_FULL_TAG_LEN];
    uint8_t tmp[sizeof(pkt->payload)];

    mbedtls_gcm_crypt_and_tag(&g_gcm, MBEDTLS_GCM_DECRYPT,
        sizeof(pkt->payload),
        nonce, NONCE_LEN,
        (const uint8_t *)pkt, AAD_LEN,
        pkt->payload, tmp,
        GCM_FULL_TAG_LEN, computed_tag);

    return memcmp(&pkt->auth_tag, computed_tag, TRUNCATED_TAG_LEN) == 0;
}

bool decrypt_packet(packet_t *pkt)
{
    if (!load_key_for_node(pkt->sender_id))
        return false;

    uint8_t nonce[NONCE_LEN];
    build_nonce(pkt, nonce);

    uint8_t computed_tag[GCM_FULL_TAG_LEN];
    uint8_t plaintext[sizeof(pkt->payload)];

    mbedtls_gcm_crypt_and_tag(&g_gcm, MBEDTLS_GCM_DECRYPT,
        sizeof(pkt->payload),
        nonce, NONCE_LEN,
        (const uint8_t *)pkt, AAD_LEN,
        pkt->payload, plaintext,
        GCM_FULL_TAG_LEN, computed_tag);

    if (memcmp(&pkt->auth_tag, computed_tag, TRUNCATED_TAG_LEN) != 0) {
        ESP_LOGW(TAG, "decrypt: auth tag mismatch");
        return false;
    }

    memcpy(pkt->payload, plaintext, sizeof(pkt->payload));
    return true;
}

bool crypto_is_revoked(uint8_t node_id)
{
    if (node_id >= MAX_NODES)
        return true;
    return g_revoked[node_id];
}

void crypto_revoke_node(uint8_t node_id)
{
    if (node_id >= MAX_NODES)
        return;
    g_revoked[node_id] = true;
    ESP_LOGW(TAG, "Node 0x%02x REVOKED", node_id);
}

void crypto_reinstate_node(uint8_t node_id)
{
    if (node_id >= MAX_NODES)
        return;
    if (!g_key_valid[node_id]) {
        ESP_LOGW(TAG, "Cannot reinstate 0x%02x: no key", node_id);
        return;
    }
    g_revoked[node_id] = false;
    ESP_LOGI(TAG, "Node 0x%02x REINSTATED", node_id);
}
