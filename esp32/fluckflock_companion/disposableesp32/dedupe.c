#include "dedupe.h"

#include <stdlib.h>

void dedupe_init(dedupe_ring_t *ring, size_t capacity) {
    ring->hashes = calloc(capacity ? capacity : 1, sizeof(uint32_t));
    ring->capacity = ring->hashes ? capacity : 0;
    ring->count = 0;
    ring->next_slot = 0;
}

void dedupe_free(dedupe_ring_t *ring) {
    free(ring->hashes);
    ring->hashes = NULL;
    ring->capacity = 0;
    ring->count = 0;
    ring->next_slot = 0;
}

uint32_t dedupe_hash(const uint8_t *data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

bool dedupe_check_and_add(dedupe_ring_t *ring, uint32_t hash) {
    if (ring->hashes == NULL || ring->capacity == 0) {
        return true; // dedupe unavailable -- fail open, don't block broadcasting
    }

    for (size_t i = 0; i < ring->count; i++) {
        if (ring->hashes[i] == hash) {
            return false;
        }
    }

    ring->hashes[ring->next_slot] = hash;
    ring->next_slot = (ring->next_slot + 1) % ring->capacity;
    if (ring->count < ring->capacity) ring->count++;
    return true;
}
