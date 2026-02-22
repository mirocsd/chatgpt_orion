#include "cache.h"
#include <string.h>

void seen_cache_init(seen_cache_t *cache) {
    memset(cache, 0, sizeof(seen_cache_t));
}

bool seen_cache_contains(seen_cache_t *cache, uint8_t sender_id, uint16_t seq_num) {
    for (uint8_t i = 0; i < cache->count; i++) {
        seen_entry_t *e = &cache->entries[i];
        if (e->sender_id == sender_id && e->seq_num == seq_num)
            return true;
    }
    return false;
}

void seen_cache_add(seen_cache_t *cache, uint8_t sender_id, uint16_t seq_num) {
    cache->entries[cache->head].sender_id = sender_id;
    cache->entries[cache->head].seq_num = seq_num;
    cache->head = (cache->head + 1) % SEEN_CACHE_SIZE;
    if (cache->count < SEEN_CACHE_SIZE)
        cache->count++;
}