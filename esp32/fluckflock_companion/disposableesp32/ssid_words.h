#pragma once

#include <stddef.h>

// Word lists for SSID (and BLE name) generation per spec.md section 5.2.1.
// Defined in ssid_words.c. Kept as plain string arrays, one concept per
// list, so they're easy to hand-edit/extend without touching generator
// logic in identity_gen.c.

extern const char *const protest_words[];
extern const size_t protest_words_count;

extern const char *const everyday_words[];
extern const size_t everyday_words_count;

extern const char *const suffixes[];
extern const size_t suffixes_count;

extern const char *const connectors[];
extern const size_t connectors_count;
