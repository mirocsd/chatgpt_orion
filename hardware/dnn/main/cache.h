#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stdbool.h>

#define SEEN_CACHE_SIZE 64

typedef struct {
    uint8_t sender_id;
    uint16_t seq_num;
} seen_entry_t;

typedef struct {
    seen_entry_t entries[SEEN_CACHE_SIZE];
    uint8_t head;
    uint8_t count;
} seen_cache_t;

void seen_cache_init(seen_cache_t *cache);
bool seen_cache_contains(seen_cache_t *cache, uint8_t sender_id, uint16_t seq_num);
void seen_cache_add(seen_cache_t *cache, uint8_t sender_id, uint16_t seq_num);

#endif