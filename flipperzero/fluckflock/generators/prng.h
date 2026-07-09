#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct ChaffRng ChaffRng;

/* Allocate PRNG state. */
ChaffRng* chaff_rng_alloc(void);

/* Free PRNG state. */
void chaff_rng_free(ChaffRng* rng);

/* Seed from a 64-bit value. Deterministic: same seed → same sequence. */
void chaff_rng_seed(ChaffRng* rng, uint64_t seed);

/* Return a uniform random uint32. */
uint32_t chaff_rng_u32(ChaffRng* rng);

/* Fill buffer with random bytes. */
void chaff_rng_bytes(ChaffRng* rng, uint8_t* buf, size_t len);

/* Return a uniform random value in [0, upper_bound). */
uint32_t chaff_rng_bounded(ChaffRng* rng, uint32_t upper_bound);
