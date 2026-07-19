#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Simple ring buffer of identity hashes used to enforce the "no-repeat
// window" from spec.md section 5.1: never rebroadcast an identity that was
// used within the last N ticks (per radio).
typedef struct {
    uint32_t *hashes;
    size_t    capacity;
    size_t    count;
    size_t    next_slot;
} dedupe_ring_t;

// Allocates a ring buffer sized `capacity` (spec: no_repeat_window, default
// 500). If allocation fails, the ring degrades to a no-op (fails open --
// dedupe_check_and_add always reports "unique" rather than blocking
// broadcasting).
void dedupe_init(dedupe_ring_t *ring, size_t capacity);
void dedupe_free(dedupe_ring_t *ring);

// FNV-1a hash helper over arbitrary bytes (e.g. MAC + SSID/name combined).
uint32_t dedupe_hash(const uint8_t *data, size_t len);

// Returns true if `hash` was NOT seen in the current window (and records
// it). Returns false if it's a repeat -- caller should regenerate and retry.
bool dedupe_check_and_add(dedupe_ring_t *ring, uint32_t hash);
